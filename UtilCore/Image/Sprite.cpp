
//***************************************************************************
// Sprite.cpp: implementation of the CSprite class.
//
//***************************************************************************

#include "pch.h"
#include "Sprite.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CSprite::CSprite()
{
	Sprn = 0;
	Sprs = NULL;
}

CSprite::~CSprite()
{
	Release();
}

//***************************************************************************
//
long CSprite::Load(TCHAR *ptszDirectory, TCHAR *ptszFileName)
{
	Release();

	int		nType;
	long	i = 0;
	TCHAR	tszFullPath[DIRECTORY_STRLEN];

	FILE *fp = NULL;

	nType = FM_BINARY;
	_stprintf_s(tszFullPath, _countof(tszFullPath), _T("%s%s"), ptszDirectory, ptszFileName);

	if( nType == FM_BINARY ) _tfopen_s(&fp, tszFullPath, _T("rb"));
	else _tfopen_s(&fp, tszFullPath, _T("rt"));

	if( fp == NULL ) return -1;

	SM_SPRFILEHEADER header;
	fread(&header, sizeof(SM_SPRFILEHEADER), 1, fp);
	Sprn = header.sprn;
	//Colorkey = header.colorkey;
	Sprs = new SM_SPRITE[header.sprn];

	for( i = 0; i < header.sprn; i++ )
	{
		fread(&Sprs[i], sizeof(SM_SPRITE) - 4, 1, fp);

		if( Sprs[i].Size > 0 )
		{
			Sprs[i].Image = new BYTE[Sprs[i].Size];
			fread(Sprs[i].Image, Sprs[i].Size, 1, fp);
			ReallocImage(&Sprs[i]);
		}
		else
		{
			Sprn = 0;
			delete Sprs;
			Sprs = NULL;

			fclose(fp);
			return -2;
		}
	}

	ZeroMemory(Palette, sizeof(RGBQUAD) * 256);

	BYTE bBuffer[768];

	fread(bBuffer, 768, 1, fp);

	for( i = 0; i < 256; i++ )
	{
		Palette[i].rgbRed = bBuffer[i * 3];
		Palette[i].rgbGreen = bBuffer[i * 3 + 1];
		Palette[i].rgbBlue = bBuffer[i * 3 + 2];
		Palette[i].rgbReserved = 0;
	}

	fclose(fp);

	return 0;
}

#define	COPYWORD(d,s,size)		\
	*(WORD *) d = *(WORD *) s;	\
	d += 2;						\
	s += 2;						\
	size += 2;

#define	COPYBYTE(d,s,size)		\
	*(BYTE *) d = *(BYTE *) s;	\
	d ++;						\
	s ++;						\
	size ++;

#define SETWORD(d,v,size)		\
	*(WORD *) d = v;			\
	d += 2;						\
	size += 2;

#define SETBYTE(d,v,size)		\
	*(BYTE *) d = v;			\
	d ++;						\
	size ++;

//***************************************************************************
//
void CSprite::ReallocImage(SM_SPRITE *spr)
{
	BYTE *buf, *obuf, *data;
	long newsize = 0;
	long pats, c1, x, y;

	try {
		buf = new BYTE[spr->Size * 3];
		obuf = buf;
		data = spr->Image;

		pats = *(WORD *)data;
		data += 2;
		x = y = 0;

		while( pats-- )
		{
			c1 = *(WORD *)data;
			data += 2;
			x += c1;
			if( x > 639 )
			{
				while( x > 639 )
				{
					SETWORD(buf, 0xfffe, newsize);
					x -= 640;
				}
				SETWORD(buf, (unsigned short)x, newsize);
			}
			else
			{
				SETWORD(buf, (unsigned short)c1, newsize);
			}
			c1 = (data[0] << 2) + data[1];
			data += 2;
			SETWORD(buf, (unsigned short)c1, newsize);

			while( c1-- )
			{
				COPYBYTE(buf, data, newsize);
				x++;
			}
		}

		SETWORD(buf, 0xffff, newsize);
		delete spr->Image;

		spr->Size = newsize;
		spr->Image = new BYTE[newsize];
		memcpy(spr->Image, obuf, newsize);
		delete obuf;
	}
	catch( ... )
	{
		TCHAR	tszBuffer[MAX_BUFFER_SIZE];

		_stprintf_s(tszBuffer, _countof(tszBuffer), _T(" # CSprite ReallocImage PROCESS ERROR #, SPR SIZE = %d, "), spr->Size);
		//g_SysLog.EventLog(tszBuffer);
	}
}

//***************************************************************************
//
void CSprite::Release()
{
	if( Sprn == 0 || Sprs == NULL ) return;

	for( long i = 0; i < Sprn; i++ )
	{
		delete Sprs[i].Image;
	}
	delete Sprs;
}

//***************************************************************************
//
void CSprite::Draw(BYTE *dest, long dw, long dh, long x, long y, long cut)
{
	if( Sprn == 0 || Sprs == NULL ) return;

	long c1;
	SM_SPRITE	*spr;
	spr = &Sprs[cut];

	x += spr->AdjustX;
	y += spr->AdjustY;

	BYTE *data = spr->Image;
	BYTE *odest = dest;
	dest = odest + (x + y * ((dw + 3) & ~3)) * 3;

	if( x < 0 ) goto clip;
	if( y < 0 ) goto clip;
	if( x + spr->Width >= dw ) goto clip;
	if( y + spr->Height >= dh ) goto clip;

	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			dest = odest + (x + y * ((dw + 3) & ~3)) * 3;
			continue;
		}
		dest += c1 * 3;
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			memcpy(dest, &Palette[*data], 3);
			dest += 3;
			data++;
		}
	}
	return;

clip:
	if( x + spr->Width < 0 ) return;
	if( y + spr->Height < 0 ) return;
	if( x >= dw ) return;
	if( y >= dh ) return;

	long ox = x, right = x + spr->Width;
	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			if( y >= dh ) return;
			dest = odest + (ox + y * ((dw + 3) & ~3)) * 3;
			x = ox;
			continue;
		}
		dest += c1 * 3;
		x += c1;
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			if( y >= 0 && x >= 0 && x < dw )
				memcpy(dest, &Palette[*data], 3);
			dest += 3;
			data++;
			x++;
		}
	}
	return;
}

//***************************************************************************
//
void CSprite::DrawUpset(BYTE *dest, long dw, long dh, long x, long y, long cut)
{
	if( Sprn == 0 || Sprs == NULL ) return;

	long c1;
	SM_SPRITE	*spr;
	spr = &Sprs[cut];

	x += spr->AdjustX;
	y += spr->AdjustY;

	BYTE *data = spr->Image;
	BYTE *odest = dest;
	dest = odest + (x + (dh - y) * ((dw + 3) & ~3)) * 3;

	if( x < 0 ) goto clip;
	if( y < 0 ) goto clip;
	if( x + spr->Width >= dw ) goto clip;
	if( y + spr->Height >= dh ) goto clip;

	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			dest = odest + (x + (dh - y) * ((dw + 3) & ~3)) * 3;
			continue;
		}
		dest += c1 * 3;
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			memcpy(dest, &Palette[*data], 3);
			dest += 3;
			data++;
		}
	}
	return;

clip:
	if( x + spr->Width < 0 ) return;
	if( y + spr->Height < 0 ) return;
	if( x >= dw ) return;
	if( y >= dh ) return;

	long ox = x, right = x + spr->Width;
	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			if( y >= dh ) return;
			dest = odest + (ox + (dh - y) * ((dw + 3) & ~3)) * 3;
			x = ox;
			continue;
		}
		dest += c1 * 3;
		x += c1;
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			if( y >= 0 && x >= 0 && x < dw )
				memcpy(dest, &Palette[*data], 3);
			dest += 3;
			data++;
			x++;
		}
	}
	return;
}

//***************************************************************************
//
void CSprite::DrawAlpha(BYTE *dest, long dw, long dh, long x, long y, long cut)
{
	if( Sprn == 0 || Sprs == NULL ) return;

	int	sw = GetWidth(cut);
	int	sh = GetHeight(cut);

	x += GetAdjustX(cut);
	y += GetAdjustY(cut);

	BYTE*		data = GetImage(cut);
	RGBQUAD*	pal = Palette;

	int		ox = x;
	long	c1;

	if( x < 0 ) goto clip;
	if( y < 0 ) goto clip;
	if( x + sw >= dw ) goto clip;
	if( y + sh >= dh ) goto clip;

	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			x = ox;
			continue;
		}
		x += c1;
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			*(dest + x + dh - y) = 255;
			x++;
			data++;
		}
	}
	return;

clip:
	if( x + sw < 0 ) return;
	if( y + sh < 0 ) return;
	if( x >= dw ) return;
	if( y >= dh ) return;

	long ox2 = x;
	for( ;; )
	{
		c1 = *(WORD *)data;
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
		c1 = *(WORD *)data;
		data += 2;

		while( c1-- )
		{
			if( y >= 0 && x >= 0 && x < dw )
				*(dest + x + dh - y) = 255;
			data++;
			x++;
		}
	}
	return;
}

//***************************************************************************
//
void CSprite::DrawFlip(BYTE *dest, long dw, long dh, long x, long y, long cut)
{
	if( Sprn == 0 || Sprs == NULL ) return;

	long		c1;
	SM_SPRITE	*spr;

	spr = &Sprs[cut];

	x += -spr->AdjustX;
	y += spr->AdjustY;

	BYTE *data = spr->Image;
	BYTE *odest = dest;
	dest = odest + (x + y * dw) * 3;

	if( x - spr->Width <= 0 ) goto clip;
	if( y < 0 ) goto clip;
	if( x > dw ) goto clip;
	if( y + spr->Height >= dh ) goto clip;

	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			dest = odest + (x + y * dw) * 3;
			continue;
		}
		dest -= c1 * 3;
		c1 = *(WORD *)data;
		data += 2;
		while( c1-- )
		{
			memcpy(dest, &Palette[*data], 3);
			dest -= 3;
			data++;
		}
	}
	return;

clip:
	if( x < 0 ) return;
	if( y + spr->Height < 0 ) return;
	if( x - spr->Width >= dw )	return;
	if( y >= dh ) return;

	long ox = x, right = x + spr->Width;
	for( ;; )
	{
		c1 = *(WORD *)data;
		data += 2;
		if( c1 == 0xffff ) break;
		if( c1 == 0xfffe )
		{
			y++;
			if( y >= dh ) return;
			dest = odest + (ox + y * dw) * 3;
			x = ox;
			continue;
		}

		dest -= c1 * 3;
		x -= c1;
		c1 = *(WORD *)data;
		data += 2;
		while( c1-- )
		{
			if( y >= 0 && x >= 0 && x < dw )
				memcpy(dest, &Palette[*data], 3);
			dest -= 3;
			data++;
			x--;
		}
	}
	return;
}

//***************************************************************************
//
short CSprite::GetAdjustX(int nIndex)
{
	return Sprs[nIndex].AdjustX;
}

//***************************************************************************
//
short CSprite::GetAdjustY(int nIndex)
{
	return Sprs[nIndex].AdjustY;
}

//***************************************************************************
//
short CSprite::GetWidth(int nIndex)
{
	return Sprs[nIndex].Width;
}

//***************************************************************************
//
short CSprite::GetHeight(int nIndex)
{
	return Sprs[nIndex].Height;
}

//***************************************************************************
//
long  CSprite::GetSize(int nIndex)
{
	return Sprs[nIndex].Size;
}

//***************************************************************************
//
BYTE* CSprite::GetImage(int nIndex)
{
	return Sprs[nIndex].Image;
}