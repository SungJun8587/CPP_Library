
//***************************************************************************
// FileManager.cpp: implementation of the CFileManager class.
//
//***************************************************************************

#include "pch.h"
#include "FileManager.h"

//***************************************************************************
// CFileManager Construction/Destruction
//***************************************************************************

CFileManager::CFileManager()
{
}

CFileManager::~CFileManager()
{
}

//***************************************************************************
//
FILE* CFileManager::Open(TCHAR *ptszDirectory, TCHAR *ptszFileName, int nType)
{
	FILE *fp = NULL;

	TCHAR	tszFullPath[DIRECTORY_STRLEN];

	_stprintf_s(tszFullPath, _countof(tszFullPath), _T("%s%s"), ptszDirectory, ptszFileName);

	if( nType == FM_BINARY ) _tfopen_s(&fp, tszFullPath, _T("rb"));
	else _tfopen_s(&fp, tszFullPath, _T("rt"));

	if( fp != NULL ) return fp;

	return NULL;
}

//***************************************************************************
//
void CFileManager::Close(FILE* fp)
{
	fclose(fp);
}
