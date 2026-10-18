
//***************************************************************************
// Sprite.h : interface for the CSprite class.
//
//***************************************************************************

#ifndef	UC_SPRITE_H
#define	UC_SPRITE_H

#include <Image/FileManager.h>
#include <cstddef>

//***************************************************************************
// @brief 스프라이트 1프레임 정보
// @details 파일에는 Size ~ AdjustY(12바이트)까지만 저장되어 있고, Image 포인터는
//          로드 시 별도로 할당한다. (x86/x64 모두 파일 레코드 크기는 12바이트)
//***************************************************************************
typedef struct _SM_SPRITE
{
	long Size;
	short Width;
	short Height;
	short AdjustX;
	short AdjustY;
	unsigned char* Image;
} SM_SPRITE, * PSM_SPRITE;

//***************************************************************************
//
typedef struct _SM_SPRFILEHEADER
{
	BYTE reserved1[17];
	BYTE colorkey;
	BYTE reserved2[48];
	WORD sprn;
	BYTE reserved3[60];
} SM_SPRFILEHEADER, * PSM_SPRFILEHEADER;

//***************************************************************************
// @brief RLE 데이터에서 리틀 엔디언 WORD 읽기 (기존 *(WORD*)p 와 동일한 결과)
//***************************************************************************
inline unsigned short SprReadWord(const BYTE* p)
{
	return static_cast<unsigned short>(p[0] | (p[1] << 8));
}

//***************************************************************************
// @brief 팔레트 + RLE 스프라이트 클래스
// @details Load()의 반환값: 0 성공, -1 파일 열기 실패(경로 포함),
//          -2 스프라이트 데이터 이상(개수/크기/RLE 변환 실패), -3 파일 읽기 실패,
//          -4 메모리 부족
//          포인터를 소유하므로 복사는 금지한다.
//***************************************************************************
class CSprite
{
public:
	CSprite();
	~CSprite();

	CSprite(const CSprite&) = delete;
	CSprite& operator=(const CSprite&) = delete;

	long	Load(const TCHAR* pszDirectory, const TCHAR* pszFilename);
	bool	ReallocImage(SM_SPRITE* spr);
	void	Release();

	BOOL	IsDataNull() const;
	BOOL	IsValidIndex(int nIndex) const;

	// 24bpp 버퍼에 직접 그리기 (stride = (dw * 3 + 3) & ~3 바이트, BGR 순서)
	void	Draw(BYTE* dest, long dw, long dh, long x, long y, long cut);			// top-down
	void	DrawUpset(BYTE* dest, long dw, long dh, long x, long y, long cut);	// bottom-up
	void	DrawAlpha(BYTE* dest, long dw, long dh, long x, long y, long cut);	// 8bpp 마스크(폭 dw), bottom-up
	void	DrawFlip(BYTE* dest, long dw, long dh, long x, long y, long cut);		// 좌우 반전, top-down

	short	GetAdjustX(int nIndex) const;
	short	GetAdjustY(int nIndex) const;
	short	GetWidth(int nIndex) const;
	short	GetHeight(int nIndex) const;
	long	GetSize(int nIndex) const;
	BYTE* GetImage(int nIndex) const;

	//***********************************************************************
	// @brief RLE 데이터를 순회하며 픽셀마다 fn(x, y, 팔레트인덱스)를 호출
	// @param nIndex 프레임 인덱스
	// @param x, y   시작 좌표 (AdjustX/AdjustY가 이미 반영된 값)
	// @param dh     대상 높이 (이 행 이상이면 순회 종료)
	// @param dx     +1 일반, -1 좌우 반전
	// @details 데이터 끝(Size)을 넘어 읽지 않는다. 좌표 클리핑은 fn에서 수행한다.
	//***********************************************************************
	template<typename Fn>
	void ForEachPixel(int nIndex, long x, long y, long dh, long dx, Fn fn) const
	{
		if( !IsValidIndex(nIndex) ) return;

		const SM_SPRITE& spr = Sprs[nIndex];
		const BYTE* data = spr.Image;
		if( data == NULL ) return;

		const BYTE* const end = data + spr.Size;
		const long ox = x;

		for( ;; )
		{
			if( end - data < 2 ) return;
			long c1 = SprReadWord(data);
			data += 2;

			if( c1 == 0xffff ) return;
			if( c1 == 0xfffe )
			{
				++y;
				if( y >= dh ) return;
				x = ox;
				continue;
			}

			x += c1 * dx;

			if( end - data < 2 ) return;
			c1 = SprReadWord(data);
			data += 2;

			if( end - data < c1 ) return;
			for( long k = 0; k < c1; ++k )
			{
				fn(x, y, data[k]);
				x += dx;
			}
			data += c1;
		}
	}

public:
	long Sprn;
	//BYTE Colorkey;
	RGBQUAD	Palette[256];
	SM_SPRITE* Sprs;
};

#endif // ndef UC_SPRITE_H