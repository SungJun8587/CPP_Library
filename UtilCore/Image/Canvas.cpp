
//***************************************************************************
// Canvas.cpp: implementation of the CCanvas class.
//
//***************************************************************************

#include "pch.h"
#include "Canvas.h"

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

	m_canvas.Resize(static_cast<uint32_t>(nWidth), static_cast<uint32_t>(nHeight));

	// 기본 캔버스 배경색 설정 (RGBA: 172, 172, 172, 255)
	for( uint32_t y = 0; y < m_canvas.Height(); ++y )
	{
		for( uint32_t x = 0; x < m_canvas.Width(); ++x )
		{
			m_canvas.SetPixel(x, y, 172, 172, 172, 255);
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
// @brief 스프라이트를 캔버스에 드로잉
// @param pSprite 스프라이트 객체 포인터
// @param x 렌더링 시작 X 좌표
// @param y 렌더링 시작 Y 좌표
// @param nIndex 스프라이트 프레임 인덱스
//***************************************************************************
void CCanvas::DrawImage(CSprite* pSprite, int x, int y, int nIndex)
{
	if( !pSprite || m_canvas.Empty() ) return;

	int dw = static_cast<int>(m_canvas.Width());
	int dh = static_cast<int>(m_canvas.Height());

	int sw = pSprite->GetWidth(nIndex);
	int sh = pSprite->GetHeight(nIndex);

	x += pSprite->GetAdjustX(nIndex);
	y += pSprite->GetAdjustY(nIndex);

	BYTE* data = pSprite->GetImage(nIndex);
	RGBQUAD* pal = pSprite->Palette;

	int ox = x;
	long c1 = 0;

	if( x < 0 || y < 0 || x + sw >= dw || y + sh >= dh )
	{
		// Clipping 처리
		long ox2 = x;
		for( ;;)
		{
			c1 = *(WORD*)data;
			data += 2;
			if( c1 == 0xffff ) break;
			if( c1 == 0xfffe )
			{
				y++;
				if( y >= dh ) return;
				x = ox2;
				continue;
			}
			x += c1;
			c1 = *(WORD*)data;
			data += 2;

			while( c1-- )
			{
				int targetY = dh - y - 1;
				if( y >= 0 && x >= 0 && x < dw && targetY >= 0 && targetY < dh )
				{
					RGBQUAD color = pal[*data];
					m_canvas.SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(targetY), color.rgbRed, color.rgbGreen, color.rgbBlue, 255);
				}
				data++;
				x++;
			}
		}
		return;
	}

	// Non-clipping Fast Path
	for( ;;)
	{
		c1 = *(WORD*)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			x = ox;
			continue;
		}
		x += c1;
		c1 = *(WORD*)data;
		data += 2;

		while( c1-- )
		{
			int targetY = dh - y - 1;
			if( targetY >= 0 && targetY < dh )
			{
				RGBQUAD color = pal[*data];
				m_canvas.SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(targetY), color.rgbRed, color.rgbGreen, color.rgbBlue, 255);
			}
			x++;
			data++;
		}
	}
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
	if( !pSprite || m_canvas.Empty() ) return;

	int dw = static_cast<int>(m_canvas.Width());
	int dh = static_cast<int>(m_canvas.Height());

	int sw = pSprite->GetWidth(nIndex);
	int sh = pSprite->GetHeight(nIndex);

	x += pSprite->GetAdjustX(nIndex);
	y += pSprite->GetAdjustY(nIndex);

	BYTE* data = pSprite->GetImage(nIndex);
	int ox = x;
	long c1 = 0;

	for( ;;)
	{
		c1 = *(WORD*)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			if( y >= dh ) return;
			x = ox;
			continue;
		}
		x += c1;
		c1 = *(WORD*)data;
		data += 2;

		while( c1-- )
		{
			int targetY = dh - y - 1;
			if( y >= 0 && x >= 0 && x < dw && targetY >= 0 && targetY < dh )
			{
				uint8_t* p = m_canvas.At(static_cast<uint32_t>(x), static_cast<uint32_t>(targetY));
				p[3] = 255; // Alpha 채널 갱신
			}
			data++;
			x++;
		}
	}
}

//***************************************************************************
// @brief 지정 영역 자르기
// @param rRect 잘라낼 RECT 영역
// @return 성공 시 true
//***************************************************************************
BOOL CCanvas::Crop(RECT rRect)
{
	if( m_canvas.Empty() ) return FALSE;

	uint32_t newWidth = static_cast<uint32_t>(rRect.right - rRect.left);
	uint32_t newHeight = static_cast<uint32_t>(rRect.bottom - rRect.top);

	ImageBuffer cropped(newWidth, newHeight);

	for( uint32_t y = 0; y < newHeight; ++y )
	{
		for( uint32_t x = 0; x < newWidth; ++x )
		{
			uint32_t srcX = rRect.left + x;
			uint32_t srcY = rRect.top + y;

			if( srcX < m_canvas.Width() && srcY < m_canvas.Height() )
			{
				const uint8_t* srcP = m_canvas.At(srcX, srcY);
				cropped.SetPixel(x, y, srcP[0], srcP[1], srcP[2], srcP[3]);
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

	pbBuffer = new BYTE[nSize];
	memcpy(pbBuffer, tempBuffer.data(), nSize);

	return nSize;
}