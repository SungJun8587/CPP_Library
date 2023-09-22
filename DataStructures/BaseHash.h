
//***************************************************************************
// BaseHash.h: interface for the CBaseHash class.
//
//***************************************************************************

#ifndef __BASEHASH_H__
#define __BASEHASH_H__

#define MAX_PRIME	500

//***************************************************************************
//
class CBaseHash  
{
public:
	CBaseHash();
	~CBaseHash();

	long	Init( long lMaxSize );
	long	HashF( TCHAR* ptszKey );
	long    GetHashSize(){ return m_lHashSize; }

private:
	long	GetSize( long lMaxSize );
	BOOL	CheckPrime( long lPrime );

public:
	long	m_lMaxCollision;

private:
	long	m_lHashSize;
	
};

#endif __BASEHASH_H__
