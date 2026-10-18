
//***************************************************************************
// Canvas.cpp: implementation of the CCanvas class.
//
//***************************************************************************

#include "pch.h"
#include "Canvas.h"

#include <new>
#include <cstring>

CCanvas::CCanvas()
{
}

CCanvas::~CCanvas()
{
	Close();
}

//***************************************************************************
// @brief 지정한 크기로 Canvas 버퍼 생성 및 초기화
// @param nWidth 가로 크기(픽셀)
// @param nHeight 세로 크기(픽셀)
//***************************************************************************
void CCanvas::Init(int nWidth, int nHeight)
{
	Close();

	if( nWidth <= 0 || nHeight <= 0 ) return;

	m_canvas.Resize(static_cast<uint32_t>(nWidth), static_cast<uint32_t>(nHeight));

	// 기본 캔버스 배경색 설정 (RGBA: 172, 172, 172, 0)
	// alpha 0 = 배경(투명) — DrawImage가 그린 픽셀만 alpha 255로 덮어써서 구분됨
	for( uint32_t y = 0; y < m_canvas.Height(); ++y )
	{
		for( uint32_t x = 0; x < m_canvas.Width(); ++x )
		{
			m_canvas.SetPixel(x, y, 172, 172, 172, 0);
		}
	}
}

//***************************************************************************
// @brief 캔버스 버퍼 해제
//***************************************************************************
void CCanvas::Close()
{
	m_canvas.Resize(0, 0);
}

//***************************************************************************
// @brief 이미지 리샘플링(스케일링)으로 캔버스 크기 변경
// @param nWidth 새 가로 크기(픽셀)
// @param nHeight 새 세로 크기(픽셀)
// @param method 리샘플링 방식(기본값 Bilinear)
// @return 처리 성공 여부
//***************************************************************************
BOOL CCanvas::Resize(int nWidth, int nHeight, ResizeMethod method)
{
	if( m_canvas.Empty() || nWidth <= 0 || nHeight <= 0 ) return FALSE;

	ImageProcessor proc(std::move(m_canvas));
	proc.Resize(static_cast<uint32_t>(nWidth), static_cast<uint32_t>(nHeight), method);
	m_canvas = std::move(proc.Buffer());
	return TRUE;
}

//***************************************************************************
// @brief 스프라이트를 캔버스에 드로잉
// @param pSprite 스프라이트 객체 포인터
// @param x 렌더링 시작 X 좌표
// @param y 렌더링 시작 Y 좌표
// @param nIndex 스프라이트 프레임 인덱스
// @details 캔버스는 top-down 버퍼이므로 스프라이트 y를 그대로 행으로 사용한다.
//          (이전의 dh - y - 1 뒤집기는 bottom-up DIB 시절 방식이라 제거)
//          RLE 데이터는 Size 범위 안에서만 읽고, 픽셀 단위로 클리핑한다.
//***************************************************************************
void CCanvas::DrawImage(CSprite* pSprite, int x, int y, int nIndex)
{
	if( !pSprite || m_canvas.Empty() || !pSprite->IsValidIndex(nIndex) ) return;

	const int dw = static_cast<int>(m_canvas.Width());
	const int dh = static_cast<int>(m_canvas.Height());

	const int sw = pSprite->GetWidth(nIndex);
	const int sh = pSprite->GetHeight(nIndex);

	x += pSprite->GetAdjustX(nIndex);
	y += pSprite->GetAdjustY(nIndex);

	// 완전히 캔버스 밖이면 그릴 것이 없다
	if( x + sw < 0 || y + sh < 0 || x >= dw || y >= dh ) return;

	const RGBQUAD* pal = pSprite->Palette;

	pSprite->ForEachPixel(nIndex, x, y, dh, 1, [&](long px, long py, BYTE idx)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;

			const RGBQUAD& color = pal[idx];
			m_canvas.SetPixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color.rgbRed, color.rgbGreen, color.rgbBlue, 255);
		});
}

//***************************************************************************
// @brief 알파 채널 전용 마스크 그리기 (Alpha 값만 255로 갱신)
// @param pSprite 스프라이트 객체 포인터
// @param x 렌더링 시작 X 좌표
// @param y 렌더링 시작 Y 좌표
// @param nIndex 스프라이트 프레임 인덱스
//***************************************************************************
void CCanvas::DrawAlpha(CSprite* pSprite, int x, int y, int nIndex)
{
	if( !pSprite || m_canvas.Empty() || !pSprite->IsValidIndex(nIndex) ) return;

	const int dw = static_cast<int>(m_canvas.Width());
	const int dh = static_cast<int>(m_canvas.Height());

	const int sw = pSprite->GetWidth(nIndex);
	const int sh = pSprite->GetHeight(nIndex);

	x += pSprite->GetAdjustX(nIndex);
	y += pSprite->GetAdjustY(nIndex);

	if( x + sw < 0 || y + sh < 0 || x >= dw || y >= dh ) return;

	pSprite->ForEachPixel(nIndex, x, y, dh, 1, [&](long px, long py, BYTE)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;

			uint8_t* p = m_canvas.At(static_cast<uint32_t>(px), static_cast<uint32_t>(py));
			p[3] = 255; // Alpha 채널 갱신
		});
}

//***************************************************************************
// @brief 지정 영역 자르기
// @param rRect 잘라낼 RECT 영역
// @return 성공 시 true
// @details RECT가 캔버스 밖으로 나가면(음수 좌표 포함) 해당 부분은 투명 픽셀로 채운다.
//***************************************************************************
BOOL CCanvas::Crop(RECT rRect)
{
	if( m_canvas.Empty() ) return FALSE;
	if( rRect.right <= rRect.left || rRect.bottom <= rRect.top ) return FALSE;

	const uint32_t newWidth = static_cast<uint32_t>(rRect.right - rRect.left);
	const uint32_t newHeight = static_cast<uint32_t>(rRect.bottom - rRect.top);

	const long srcW = static_cast<long>(m_canvas.Width());
	const long srcH = static_cast<long>(m_canvas.Height());

	ImageBuffer cropped(newWidth, newHeight);

	for( uint32_t y = 0; y < newHeight; ++y )
	{
		for( uint32_t x = 0; x < newWidth; ++x )
		{
			const long srcX = rRect.left + static_cast<long>(x);
			const long srcY = rRect.top + static_cast<long>(y);

			if( srcX >= 0 && srcY >= 0 && srcX < srcW && srcY < srcH )
			{
				const uint8_t* srcP = m_canvas.At(static_cast<uint32_t>(srcX), static_cast<uint32_t>(srcY));
				cropped.SetPixel(x, y, srcP[0], srcP[1], srcP[2], srcP[3]);
			}
			else
			{
				cropped.SetPixel(x, y, 0, 0, 0, 0);
			}
		}
	}

	m_canvas = std::move(cropped);
	return TRUE;
}

//***************************************************************************
// @brief 이미지 좌우 반전 (Horizontal Flip)
// @return 성공 시 true
//***************************************************************************
BOOL CCanvas::MirrorImage()
{
	if( m_canvas.Empty() ) return FALSE;

	uint32_t w = m_canvas.Width();
	uint32_t h = m_canvas.Height();

	for( uint32_t y = 0; y < h; ++y )
	{
		for( uint32_t x = 0; x < w / 2; ++x )
		{
			uint8_t* p1 = m_canvas.At(x, y);
			uint8_t* p2 = m_canvas.At(w - 1 - x, y);

			for( int c = 0; c < 4; ++c )
			{
				std::swap(p1[c], p2[c]);
			}
		}
	}
	return TRUE;
}

//***************************************************************************
// @brief 투명 영역(Alpha == 0)의 RGB 픽셀 값 정돈
//***************************************************************************
void CCanvas::FilterImage()
{
	if( m_canvas.Empty() ) return;

	for( uint32_t y = 0; y < m_canvas.Height(); ++y )
	{
		for( uint32_t x = 0; x < m_canvas.Width(); ++x )
		{
			uint8_t* p = m_canvas.At(x, y);
			if( p[3] == 0 )
			{
				p[0] = 0;
				p[1] = 0;
				p[2] = 0;
			}
		}
	}
}

//***************************************************************************
// @brief Canvas(ImageBuffer)를 지정한 이미지 포맷의 바이트 버퍼로 인코딩
// @param outBuffer 인코딩된 바이트 데이터가 저장될 vector
// @param format 저장할 이미지 포맷 (기본값: ImageFormat::PNG)
// @return 인코딩 성공 시 버퍼 바이트 크기, 실패 시 0
//***************************************************************************
int CCanvas::Encode(std::vector<uint8_t>& outBuffer, ImageFormat format)
{
	if( m_canvas.Empty() ) return 0;

	try
	{
		outBuffer = ImageIO::SaveToMemory(m_canvas, format);
		return static_cast<int>(outBuffer.size());
	}
	catch( const ImageException& )
	{
		outBuffer.clear();
		return 0;
	}
}

//***************************************************************************
// @brief C-Style 외부 호환용 (동적 할당 버퍼 생성)
// @param pbBuffer 할당된 버퍼 포인터 참조 (호출 측에서 delete[] 필요)
// @param format 저장할 이미지 포맷 (기본값: ImageFormat::PNG)
// @return 인코딩 성공 시 버퍼 바이트 크기, 실패 시 0
//***************************************************************************
int CCanvas::Encode(BYTE*& pbBuffer, ImageFormat format)
{
	pbBuffer = nullptr;
	std::vector<uint8_t> tempBuffer;

	int nSize = Encode(tempBuffer, format);
	if( nSize <= 0 ) return 0;

	pbBuffer = new(std::nothrow) BYTE[nSize];
	if( pbBuffer == nullptr ) return 0;

	memcpy(pbBuffer, tempBuffer.data(), nSize);

	return nSize;
}