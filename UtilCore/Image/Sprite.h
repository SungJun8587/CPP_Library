
//***************************************************************************
// Sprite.h : interface for the CSprite class.
//
//***************************************************************************

#ifndef	UC_SPRITE_H
#define	UC_SPRITE_H

#include <Image/FileManager.h>

//***************************************************************************
//
typedef struct _SM_SPRITE 
{
	long Size;
	short Width;
	short Height;
	short AdjustX;
	short AdjustY;
	unsigned char *Image;
} SM_SPRITE, *PSM_SPRITE;

//***************************************************************************
//
typedef struct _SM_SPRFILEHEADER 
{
	BYTE reserved1[17];
	BYTE colorkey;
	BYTE reserved2[48];
	WORD sprn;
	BYTE reserved3[60];
} SM_SPRFILEHEADER, *PSM_SPRFILEHEADER;

//***************************************************************************
//
class CSprite
{
public:
	CSprite();
	~CSprite();

	long	Load( TCHAR *pszDirectory, TCHAR *pszFilename );
	long	Load( TCHAR *pszDirectory, TCHAR *pszPkgFilename, TCHAR *pszFilename );
	void	ReallocImage( SM_SPRITE *spr );
	void	Release();

	BOOL	IsDataNull();

	void	Draw( BYTE *dest, long dw, long dh, long x, long y, long cut );
	void	DrawUpset( BYTE *dest, long dw, long dh, long x, long y, long cut );
	void	DrawAlpha( BYTE *dest, long dw, long dh, long x, long y, long cut );
	void	DrawFlip( BYTE *dest, long dw, long dh, long x, long y, long cut );
	
	short	GetAdjustX( int nIndex );
	short	GetAdjustY( int nIndex );
	short	GetWidth( int nIndex );
	short	GetHeight( int nIndex );
	long	GetSize( int nIndex );
	BYTE*	GetImage( int nIndex );

public:
	long Sprn;
	//BYTE Colorkey;
	RGBQUAD	Palette[256];
	SM_SPRITE *Sprs;
};

#endif // ndef UC_SPRITE_H
