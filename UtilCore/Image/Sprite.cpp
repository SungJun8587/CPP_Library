
//***************************************************************************
// Sprite.cpp : implementation of the CSprite class.
//
//***************************************************************************

#include "pch.h"
#include "Sprite.h"

#include <new>
#include <vector>
#include <cstring>

//***************************************************************************
// 파일 레코드(Size ~ AdjustY)의 바이트 크기. Image 포인터는 파일에 없으므로
// sizeof(SM_SPRITE) - 4 (x86 전용 계산) 대신 offsetof 기반으로 구한다.
// x86: 12, x64: 12 (Image 앞 패딩은 포함하지 않는다)
static const size_t kSprRecordSize = offsetof(SM_SPRITE, AdjustY) + sizeof(short);
static_assert(kSprRecordSize == 12, "SM_SPRITE file record layout changed");
static_assert(sizeof(SM_SPRFILEHEADER) == 128, "SM_SPRFILEHEADER file layout changed");

// 손상된 파일에서 비정상적으로 큰 할당을 막기 위한 프레임당 크기 상한
static const long kMaxSprFrameSize = 64 * 1024 * 1024;

// ReallocImage가 가정하는 원본 데이터의 가로 폭
static const long kSprRowWidth = 640;

//***************************************************************************
// 24bpp DIB의 행 stride(바이트). 행은 4바이트 경계로 정렬된다.
static inline ptrdiff_t CalcStride24(long dw)
{
	return static_cast<ptrdiff_t>((dw * 3 + 3) & ~3L);
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CSprite::CSprite()
{
	Sprn = 0;
	Sprs = NULL;
	ZeroMemory(Palette, sizeof(Palette));
}

CSprite::~CSprite()
{
	Release();
}

//***************************************************************************
// @brief 스프라이트 파일 로드
// @param ptszDirectory 디렉터리 경로 (끝 구분자 포함)
// @param ptszFileName 파일명
// @return 0 성공, -1 파일 열기 실패, -2 데이터 이상, -3 읽기 실패, -4 메모리 부족
long CSprite::Load(const TCHAR* ptszDirectory, const TCHAR* ptszFileName)
{
	Release();

	if( ptszDirectory == NULL || ptszFileName == NULL ) return -1;

	TCHAR	tszFullPath[DIRECTORY_STRLEN];

	// 경로가 버퍼보다 길면 잘라서 엉뚱한 파일을 열지 않고 실패 처리
	if( _sntprintf_s(tszFullPath, _countof(tszFullPath), _TRUNCATE, _T("%s%s"), ptszDirectory, ptszFileName) < 0 )
		return -1;

	FILE* fp = NULL;
	if( _tfopen_s(&fp, tszFullPath, _T("rb")) != 0 || fp == NULL ) return -1;

	SM_SPRFILEHEADER header;
	if( fread(&header, sizeof(header), 1, fp) != 1 )
	{
		fclose(fp);
		return -3;
	}

	const long nCount = header.sprn;
	if( nCount <= 0 )
	{
		fclose(fp);
		return -2;
	}

	long nResult = 0;

	try
	{
		Sprs = new SM_SPRITE[nCount]();		// () : 값 초기화 -> Image = NULL
		Sprn = nCount;

		for( long i = 0; i < nCount && nResult == 0; i++ )
		{
			if( fread(&Sprs[i], kSprRecordSize, 1, fp) != 1 )
			{
				nResult = -3;
				break;
			}

			Sprs[i].Image = NULL;		// 파일 값이 들어오지 않도록 방어

			if( Sprs[i].Size <= 0 || Sprs[i].Size > kMaxSprFrameSize )
			{
				nResult = -2;
				break;
			}

			Sprs[i].Image = new BYTE[Sprs[i].Size];
			if( fread(Sprs[i].Image, Sprs[i].Size, 1, fp) != 1 )
			{
				nResult = -3;
				break;
			}

			if( !ReallocImage(&Sprs[i]) ) nResult = -2;
		}

		if( nResult == 0 )
		{
			BYTE bBuffer[768];

			if( fread(bBuffer, sizeof(bBuffer), 1, fp) != 1 )
			{
				nResult = -3;
			}
			else
			{
				ZeroMemory(Palette, sizeof(RGBQUAD) * 256);

				for( int i = 0; i < 256; i++ )
				{
					Palette[i].rgbRed = bBuffer[i * 3];
					Palette[i].rgbGreen = bBuffer[i * 3 + 1];
					Palette[i].rgbBlue = bBuffer[i * 3 + 2];
					Palette[i].rgbReserved = 0;
				}
			}
		}
	}
	catch( const std::bad_alloc& )
	{
		nResult = -4;
	}

	fclose(fp);

	if( nResult != 0 )
	{
		Release();		// 이미 할당된 프레임 이미지까지 모두 정리
		return nResult;
	}

	return 0;
}

//***************************************************************************
// @brief 파일의 원본 RLE 데이터를 그리기용 RLE 데이터로 변환한다.
// @details 변환 결과 포맷: [skip WORD][count WORD][count bytes] ... , 0xfffe = 줄바꿈, 0xffff = 끝
//          입력/출력 모두 경계를 검사하며, 손상된 데이터는 false를 반환한다.
//          (실패 시 spr->Image는 변경되지 않는다)
// @return 성공 여부
bool CSprite::ReallocImage(SM_SPRITE* spr)
{
	if( spr == NULL || spr->Image == NULL || spr->Size < 2 ) return false;

	try
	{
		const BYTE* data = spr->Image;
		const BYTE* const dataEnd = spr->Image + spr->Size;

		std::vector<BYTE> out;
		out.reserve(static_cast<size_t>(spr->Size) * 3);

		auto putWord = [&out](unsigned short v)
			{
				out.push_back(static_cast<BYTE>(v & 0xff));
				out.push_back(static_cast<BYTE>(v >> 8));
			};

		long pats = SprReadWord(data);
		data += 2;
		long x = 0;

		while( pats-- )
		{
			if( dataEnd - data < 4 ) return false;		// skip(2) + count(2)

			long c1 = SprReadWord(data);
			data += 2;
			x += c1;

			if( x > kSprRowWidth - 1 )
			{
				while( x > kSprRowWidth - 1 )
				{
					putWord(0xfffe);
					x -= kSprRowWidth;
				}
				putWord(static_cast<unsigned short>(x));
			}
			else
			{
				putWord(static_cast<unsigned short>(c1));
			}

			// 원본 코드 그대로 유지: 파일 포맷 명세와 대조가 필요한 부분
			c1 = (data[0] << 2) + data[1];
			data += 2;
			putWord(static_cast<unsigned short>(c1));

			if( dataEnd - data < c1 ) return false;		// 픽셀 데이터가 Size를 넘음

			out.insert(out.end(), data, data + c1);
			data += c1;
			x += c1;
		}

		putWord(0xffff);

		BYTE* pNew = new BYTE[out.size()];
		memcpy(pNew, out.data(), out.size());

		delete[] spr->Image;
		spr->Image = pNew;
		spr->Size = static_cast<long>(out.size());
		return true;
	}
	catch( const std::bad_alloc& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 모든 프레임과 배열을 해제한다. 반복 호출해도 안전하다.
void CSprite::Release()
{
	if( Sprs )
	{
		for( long i = 0; i < Sprn; i++ )
		{
			delete[] Sprs[i].Image;
			Sprs[i].Image = NULL;
		}
		delete[] Sprs;
	}

	Sprs = NULL;
	Sprn = 0;
}

//***************************************************************************
//
BOOL CSprite::IsDataNull() const
{
	return (Sprn == 0 || Sprs == NULL) ? TRUE : FALSE;
}

//***************************************************************************
//
BOOL CSprite::IsValidIndex(int nIndex) const
{
	return (Sprs != NULL && nIndex >= 0 && nIndex < Sprn) ? TRUE : FALSE;
}

//***************************************************************************
// @brief 24bpp(BGR) 버퍼에 top-down 방식으로 그린다.
void CSprite::Draw(BYTE* dest, long dw, long dh, long x, long y, long cut)
{
	if( dest == NULL || !IsValidIndex(static_cast<int>(cut)) ) return;

	const SM_SPRITE& spr = Sprs[cut];

	x += spr.AdjustX;
	y += spr.AdjustY;

	if( x + spr.Width < 0 || y + spr.Height < 0 || x >= dw || y >= dh ) return;

	const ptrdiff_t stride = CalcStride24(dw);

	ForEachPixel(static_cast<int>(cut), x, y, dh, 1, [&](long px, long py, BYTE idx)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;
			memcpy(dest + py * stride + px * 3, &Palette[idx], 3);		// RGBQUAD 앞 3바이트 = B,G,R
		});
}

//***************************************************************************
// @brief 24bpp(BGR) 버퍼에 bottom-up 방식(DIB 기본 방향)으로 그린다.
void CSprite::DrawUpset(BYTE* dest, long dw, long dh, long x, long y, long cut)
{
	if( dest == NULL || !IsValidIndex(static_cast<int>(cut)) ) return;

	const SM_SPRITE& spr = Sprs[cut];

	x += spr.AdjustX;
	y += spr.AdjustY;

	if( x + spr.Width < 0 || y + spr.Height < 0 || x >= dw || y >= dh ) return;

	const ptrdiff_t stride = CalcStride24(dw);

	ForEachPixel(static_cast<int>(cut), x, y, dh, 1, [&](long px, long py, BYTE idx)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;
			const long row = dh - py - 1;		// 마지막 행이 y = 0
			memcpy(dest + row * stride + px * 3, &Palette[idx], 3);
		});
}

//***************************************************************************
// @brief 8bpp 마스크 버퍼(행 폭 dw, 패딩 없음)에 불투명 픽셀을 255로 표시한다. bottom-up.
void CSprite::DrawAlpha(BYTE* dest, long dw, long dh, long x, long y, long cut)
{
	if( dest == NULL || !IsValidIndex(static_cast<int>(cut)) ) return;

	const SM_SPRITE& spr = Sprs[cut];

	x += spr.AdjustX;
	y += spr.AdjustY;

	if( x + spr.Width < 0 || y + spr.Height < 0 || x >= dw || y >= dh ) return;

	ForEachPixel(static_cast<int>(cut), x, y, dh, 1, [&](long px, long py, BYTE)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;
			const long row = dh - py - 1;
			dest[static_cast<ptrdiff_t>(row) * dw + px] = 255;
		});
}

//***************************************************************************
// @brief 24bpp(BGR) 버퍼에 좌우 반전하여 top-down 방식으로 그린다.
void CSprite::DrawFlip(BYTE* dest, long dw, long dh, long x, long y, long cut)
{
	if( dest == NULL || !IsValidIndex(static_cast<int>(cut)) ) return;

	const SM_SPRITE& spr = Sprs[cut];

	x += -spr.AdjustX;
	y += spr.AdjustY;

	// 반전 시 스프라이트는 x에서 왼쪽으로 펼쳐진다
	if( x < 0 || y + spr.Height < 0 || x - spr.Width >= dw || y >= dh ) return;

	const ptrdiff_t stride = CalcStride24(dw);

	ForEachPixel(static_cast<int>(cut), x, y, dh, -1, [&](long px, long py, BYTE idx)
		{
			if( px < 0 || px >= dw || py < 0 || py >= dh ) return;
			memcpy(dest + py * stride + px * 3, &Palette[idx], 3);
		});
}

//***************************************************************************
// 아래 getter는 인덱스/로드 상태를 검사하여 잘못된 접근 시 0(NULL)을 반환한다.
//***************************************************************************

//***************************************************************************
//
short CSprite::GetAdjustX(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].AdjustX : 0;
}

//***************************************************************************
//
short CSprite::GetAdjustY(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].AdjustY : 0;
}

//***************************************************************************
//
short CSprite::GetWidth(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].Width : 0;
}

//***************************************************************************
//
short CSprite::GetHeight(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].Height : 0;
}

//***************************************************************************
//
long  CSprite::GetSize(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].Size : 0;
}

//***************************************************************************
//
BYTE* CSprite::GetImage(int nIndex) const
{
	return IsValidIndex(nIndex) ? Sprs[nIndex].Image : NULL;
}