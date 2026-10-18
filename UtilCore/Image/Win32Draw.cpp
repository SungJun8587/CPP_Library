
//***************************************************************************
// Win32Draw.cpp : implementation of the Win32Draw class.
//
//***************************************************************************

#include "pch.h"
#include "Win32Draw.h"

//***************************************************************************
// @brief ImageBuffer를 지정한 위치에 지정한 크기로 스트레치하여 HDC에 그림
// @param hdc 그려질 대상 디바이스 컨텍스트
// @param img 그릴 원본 이미지(RGBA8)
// @param x 대상 좌상단 가로 좌표
// @param y 대상 좌상단 세로 좌표
// @param destW 대상에 그려질 가로 크기(스트레치)
// @param destH 대상에 그려질 세로 크기(스트레치)
//***************************************************************************
void Win32Draw::DrawToDC(HDC hdc, const ImageBuffer& img, int x, int y, int destW, int destH)
{
	uint32_t w = img.Width(), h = img.Height();
	if( w == 0 || h == 0 ) return;

	// GDI 32bpp BI_RGB DIB는 픽셀당 B,G,R,X 바이트 순서를 요구하므로 변환한다.
	std::vector<uint8_t> bgra(static_cast<size_t>(w) * h * 4);
	const uint8_t* src = img.Data();
	for( size_t i = 0; i < static_cast<size_t>(w) * h; ++i )
	{
		bgra[i * 4 + 0] = src[i * 4 + 2]; // B
		bgra[i * 4 + 1] = src[i * 4 + 1]; // G
		bgra[i * 4 + 2] = src[i * 4 + 0]; // R
		bgra[i * 4 + 3] = src[i * 4 + 3]; // X(사용 안 함)
	}

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = static_cast<LONG>(w);
	bmi.bmiHeader.biHeight = -static_cast<LONG>(h); // 음수 = top-down DIB (ImageBuffer와 동일한 행 순서)
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	SetStretchBltMode(hdc, HALFTONE);
	StretchDIBits(hdc, x, y, destW, destH, 0, 0, static_cast<int>(w), static_cast<int>(h),
		bgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

//***************************************************************************
// @brief ImageBuffer를 원본 크기 그대로 지정한 위치에 HDC에 그림
// @param hdc 그려질 대상 디바이스 컨텍스트
// @param img 그릴 원본 이미지(RGBA8)
// @param x 대상 좌상단 가로 좌표
// @param y 대상 좌상단 세로 좌표
//***************************************************************************
void Win32Draw::DrawToDC(HDC hdc, const ImageBuffer& img, int x, int y)
{
	DrawToDC(hdc, img, x, y, static_cast<int>(img.Width()), static_cast<int>(img.Height()));
}

//***************************************************************************
// @brief ImageBuffer(RGBA8)의 알파 채널을 반영하여 지정한 위치에 지정한 크기로 HDC에 그림
// @param hdc 그려질 대상 디바이스 컨텍스트
// @param img 그릴 원본 이미지(RGBA8, 알파 채널 포함)
// @param x 대상 좌상단 가로 좌표
// @param y 대상 좌상단 세로 좌표
// @param destW 대상에 그려질 가로 크기(스트레치)
// @param destH 대상에 그려질 세로 크기(스트레치)
// @details RGBA를 premultiplied BGRA로 변환한 32bpp DIBSection을 만들고
//          AlphaBlend(AC_SRC_ALPHA)로 출력한다. 알파가 0인 영역은 대상 DC의
//          기존 내용이 그대로 보인다.
//***************************************************************************
void Win32Draw::DrawToDCAlpha(HDC hdc, const ImageBuffer& img, int x, int y, int destW, int destH)
{
	const uint32_t w = img.Width(), h = img.Height();
	if( w == 0 || h == 0 || destW <= 0 || destH <= 0 ) return;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = static_cast<LONG>(w);
	bmi.bmiHeader.biHeight = -static_cast<LONG>(h);	// 음수 = top-down DIB
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pBits = nullptr;
	HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	if( hBmp == nullptr || pBits == nullptr ) return;

	// RGBA -> premultiplied BGRA (AC_SRC_ALPHA는 premultiplied를 요구)
	uint8_t* dst = static_cast<uint8_t*>(pBits);
	const uint8_t* src = img.Data();
	const size_t nPixels = static_cast<size_t>(w) * h;
	for( size_t i = 0; i < nPixels; ++i )
	{
		const uint32_t a = src[i * 4 + 3];
		dst[i * 4 + 0] = static_cast<uint8_t>((src[i * 4 + 2] * a + 127) / 255);	// B
		dst[i * 4 + 1] = static_cast<uint8_t>((src[i * 4 + 1] * a + 127) / 255);	// G
		dst[i * 4 + 2] = static_cast<uint8_t>((src[i * 4 + 0] * a + 127) / 255);	// R
		dst[i * 4 + 3] = static_cast<uint8_t>(a);									// A
	}

	HDC hMemDC = CreateCompatibleDC(hdc);
	if( hMemDC )
	{
		HGDIOBJ hOld = SelectObject(hMemDC, hBmp);

		BLENDFUNCTION bf = {};
		bf.BlendOp = AC_SRC_OVER;
		bf.SourceConstantAlpha = 255;
		bf.AlphaFormat = AC_SRC_ALPHA;

		AlphaBlend(hdc, x, y, destW, destH, hMemDC, 0, 0, static_cast<int>(w), static_cast<int>(h), bf);

		SelectObject(hMemDC, hOld);
		DeleteDC(hMemDC);
	}
	DeleteObject(hBmp);
}

//***************************************************************************
// @brief ImageBuffer의 알파 채널을 반영하여 원본 크기 그대로 지정한 위치에 HDC에 그림
// @param hdc 그려질 대상 디바이스 컨텍스트
// @param img 그릴 원본 이미지(RGBA8, 알파 채널 포함)
// @param x 대상 좌상단 가로 좌표
// @param y 대상 좌상단 세로 좌표
//***************************************************************************
void Win32Draw::DrawToDCAlpha(HDC hdc, const ImageBuffer& img, int x, int y)
{
	DrawToDCAlpha(hdc, img, x, y, static_cast<int>(img.Width()), static_cast<int>(img.Height()));
}