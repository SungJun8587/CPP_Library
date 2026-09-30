
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