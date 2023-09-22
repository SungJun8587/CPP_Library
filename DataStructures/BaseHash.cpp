
//***************************************************************************
// BaseHash.cpp : implementation of the CBaseHash class.
//
//***************************************************************************

#include "pch.h"
#include "BaseHash.h"

long g_lPrime[MAX_PRIME] = { 2, 3, 5, 7, 11, 13, 0, };

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CBaseHash::CBaseHash()
{
	m_lHashSize = 0;
	m_lMaxCollision = 10;
}

CBaseHash::~CBaseHash()
{
}

//***************************************************************************
//
long CBaseHash::Init(long lMaxSize)
{
	if( lMaxSize < 1 ) return false;

	m_lHashSize = GetSize(lMaxSize);

	return m_lHashSize;
}

//***************************************************************************
//
long CBaseHash::HashF(TCHAR* ptszKey)
{
	long lHashKey = -1;
	long lNum = 0, lNum2 = 1;

	if( ptszKey == NULL || m_lHashSize <= 0 ) return -1;

	while( *ptszKey != '\0' )
	{
		lNum = *ptszKey;
		lHashKey += lNum * lNum2;
		lHashKey %= m_lHashSize;
		lNum2 = (lNum2 * 10) % m_lHashSize;
		if( lNum2 == 0 ) lNum2 = 1;
		ptszKey++;
	}

	return abs(lHashKey);
}

//***************************************************************************
//
long CBaseHash::GetSize(long lMaxSize)
{
	long lPrime = 0, lPrePrime = 0;

	for( long lTemp = lMaxSize; lTemp < lMaxSize + 1000; lTemp += 1 )
	{
		lPrime = (lTemp / 4) * 4 + 1;
		if( lPrime == lPrePrime ) continue;

		if( CheckPrime(lPrime) ) return lPrime;
		else lPrePrime = lPrime;
	}

	return (lMaxSize / 4) * 4 + 1;
}

//***************************************************************************
//
BOOL CBaseHash::CheckPrime(long lPrime)
{
	int		iLoc = 0, iLoc2 = 0;
	long	lMiddle = (lPrime / 2) + 1;

	for( iLoc = 0; iLoc < MAX_PRIME && g_lPrime[iLoc] != 0; iLoc++ )
		if( lPrime % g_lPrime[iLoc] == 0 ) return false;

	for( long lNum = g_lPrime[iLoc - 1] + 2; lNum <= lMiddle; lNum += 2 )
	{
		if( iLoc + 1 < MAX_PRIME )
		{
			for( iLoc2 = 0; iLoc2 < iLoc; iLoc2++ )
				if( lNum % g_lPrime[iLoc2] == 0 ) break;

			if( iLoc2 == iLoc )
			{
				g_lPrime[iLoc++] = lNum;
				g_lPrime[iLoc] = 0;
			}
		}

		if( lPrime % lNum == 0 ) return false;
	}

	return true;
}