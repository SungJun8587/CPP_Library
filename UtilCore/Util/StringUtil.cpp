
//***************************************************************************
// StringUtil.cpp: implementation of the StringUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "StringUtil.h"

//***************************************************************************
//Function to passing FolderPath to FullFilePath 
bool FolderPathPassing(TCHAR* ptszFolderPath, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr || ptszFolderPath == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		nCount++;
	}

	if( nLen - nCount > DIRECTORY_STRLEN ) return false;

	for( ptszSourceLoc = ptszFullFilePath, ptszDestLoc = ptszFolderPath; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//Function to passing FileNameExt to FullFilePath 
bool FileNameExtPathPassing(TCHAR* ptszFileNameExt, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr || ptszFileNameExt == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		nCount++;
	}

	if( nCount > FILENAMEEXT_STRLEN ) return false;

	ptszSourceLoc = ptszFullFilePath + nLen - nCount;
	ptszDestLoc = ptszFileNameExt;

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//Function to passing FileName and FileExt to FileNameExt 
bool FileNameExtPassing(TCHAR* ptszFileName, TCHAR* ptszFileExt, const TCHAR* ptszFileNameExt)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFileNameExt == nullptr || ptszFileName == nullptr || ptszFileExt == nullptr ) return false;
	if( (nLen = _tcslen(ptszFileNameExt)) < 1 ) return false;

	for( ptszSourceLoc = ptszFileNameExt + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('.') )
		{
			nCount++;
			break;
		}

		nCount++;
	}

	if( nLen - nCount > FILENAME_STRLEN ) return false;

	for( ptszSourceLoc = ptszFileNameExt, ptszDestLoc = ptszFileName; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	if( nCount > FILEEXT_STRLEN ) return false;

	ptszSourceLoc = ptszFileNameExt + nLen - nCount + 1;
	ptszDestLoc = ptszFileExt;

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//Function to passing FolderPath to FullFilePath 
bool FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		nCount++;
	}

	TFolderPath.Init(nLen - nCount + 1);

	for( ptszSourceLoc = ptszFullFilePath, ptszDestLoc = TFolderPath.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//Function to passing FileNameExt to FullFilePath 
bool FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		nCount++;
	}

	TFileNameExt.Init(nCount + 1);

	ptszSourceLoc = ptszFullFilePath + nLen - nCount;
	ptszDestLoc = TFileNameExt.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//Function to passing FileName and FileExt to FileNameExt 
bool FileNameExtPassing(CMemBuffer<TCHAR>& TFileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFileNameExt == nullptr ) return false;
	if( (nLen = _tcslen(ptszFileNameExt)) < 1 ) return false;

	for( ptszSourceLoc = ptszFileNameExt + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('.') )
		{
			nCount++;
			break;
		}

		nCount++;
	}

	TFileName.Init(nLen - nCount + 1);

	for( ptszSourceLoc = ptszFileNameExt, ptszDestLoc = TFileName.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	TFileExt.Init(nCount);

	ptszSourceLoc = ptszFileNameExt + nLen - nCount + 1;
	ptszDestLoc = TFileExt.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
// 
bool ParseURL(TCHAR* ptszProtocol, TCHAR* ptszHostName, TCHAR* ptszRequest, int& nPort, const TCHAR* ptszFullUrl)
{
	size_t	nTotalLen;
	size_t	nProtocolLen;
	size_t	nHostLen;
	size_t	nRequestLen;
	TCHAR* ptszWork = nullptr;
	TCHAR* ptszPoint1 = nullptr;
	TCHAR* ptszPoint2 = nullptr;

	nPort = 80;

	if( ptszFullUrl == nullptr ) return false;
	if( (nTotalLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	ptszWork = _tcsdup(ptszFullUrl);
	if( (ptszPoint1 = _tcsstr(ptszWork, _T("://"))) != NULL )
	{
		nProtocolLen = nTotalLen - _tcslen(ptszPoint1) + 3;
		_tcsncpy_s(ptszProtocol, nProtocolLen + 1, ptszWork, _TRUNCATE);
	}
	else
	{
		nProtocolLen = 0;
		_tcsncpy_s(ptszProtocol, 8, _T("http://"), _TRUNCATE);
		ptszPoint1 = ptszWork;
	}

	if( (*ptszPoint1 == ':') && (*(ptszPoint1 + 1) == '/') && (*(ptszPoint1 + 2) == '/') )				// skip past opening /'s 
		ptszPoint1 += 3;

	nHostLen = 0;
	ptszPoint2 = ptszPoint1;														// find host
	while( (isalpha(*ptszPoint2) || isdigit(*ptszPoint2) || *ptszPoint2 == '-' || *ptszPoint2 == '.' || *ptszPoint2 == ':') && *ptszPoint2 )
	{
		ptszPoint2++;
		nHostLen++;
	}
	*ptszPoint2 = 0;

	_tcsncpy_s(ptszHostName, nHostLen + 1, ptszPoint1, _TRUNCATE);

	nRequestLen = nTotalLen - nProtocolLen - nHostLen;
	_tcsncpy_s(ptszRequest, nRequestLen + 1, ptszFullUrl + (ptszPoint2 - ptszWork), _TRUNCATE);

	ptszPoint1 = _tcschr(ptszHostName, ':');									// find the port number, if any
	if( ptszPoint1 != NULL )
	{
		*ptszPoint1 = 0;
		nPort = _ttoi(ptszPoint1 + 1);
	}

	free(ptszWork);

	return true;
}

//***************************************************************************
//
bool HostNamePassing(TCHAR* ptszHostName, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr || ptszHostName == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		nCount = nLen - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + nCount; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		nCount++;
	}

	if( nCount > HTTP_PROTOCOL_STRLEN + HTTP_HOSTNAME_STRLEN ) return false;

	for( ptszSourceLoc = ptszFullUrl, ptszDestLoc = ptszHostName; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
// 
bool UrlFullPathPassing(TCHAR* ptszUrlFullPath, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		nFirst = nLen - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + nFirst; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		nFirst++;
	}

	for( ptszSourceLoc = ptszFullUrl + nLen; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		nCount++;
	}

	if( nLen - nFirst - nCount > HTTP_URLPATH_STRLEN ) return false;

	for( ptszSourceLoc = ptszFullUrl + nFirst, ptszDestLoc = ptszUrlFullPath; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nFirst - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool QueryStringPassing(TCHAR* ptszQueryString, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullUrl + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		nCount++;
	}

	if( nCount > HTTP_QUERYSTRING_STRLEN ) return false;

	ptszSourceLoc = ptszFullUrl + nLen - nCount;
	ptszDestLoc = ptszQueryString;

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
// 
bool HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		nCount = nLen - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + nCount; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		nCount++;
	}

	THostName.Init(nCount + 1);

	for( ptszSourceLoc = ptszFullUrl, ptszDestLoc = THostName.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		nFirst = nLen - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + nFirst; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		nFirst++;
	}

	for( ptszSourceLoc = ptszFullUrl + nLen; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		nCount++;
	}

	TUrlFullPath.Init(nLen - nFirst - nCount + 1);

	for( ptszSourceLoc = ptszFullUrl + nFirst, ptszDestLoc = TUrlFullPath.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nFirst - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
// 
bool QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (nLen = _tcslen(ptszFullUrl)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullUrl + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		nCount++;
	}

	TQueryString.Init(nCount + 1);

	ptszSourceLoc = ptszFullUrl + nLen - nCount;
	ptszDestLoc = TQueryString.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
size_t TokenCount(const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszNextToken = nullptr;

	if( ptszSource == nullptr || ptszToken == nullptr ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;
	if( !_tcsstr(ptszSource, ptszToken) ) return -1;

	ptszSourceLoc = new TCHAR[nLen + 1];

	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);

		nCount++;
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return nCount;
}

//***************************************************************************
//
bool Tokenize(TCHAR** pptszDestination, int& nCount, const TCHAR* ptszSource, const TCHAR* ptszToken, const int nSize)
{
	size_t	nLen = 0;
	int     nTempCount = 0;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszNextToken = nullptr;

	if( ptszSource == nullptr || ptszToken == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = new TCHAR[nLen + 1];
	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);

	for( int i = 0; ptszTokenize != NULL; i++ )
	{
		_tcscpy_s(*(pptszDestination + i), MAX_BUFFER_SIZE, ptszTokenize);
		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);
		nTempCount++;
	}
	nCount = nTempCount;

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return true;
}

//***************************************************************************
//
bool Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszNextToken = nullptr;

	if( ptszSource == nullptr || ptszToken == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = new TCHAR[nLen + 1];
	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ppTDestination[nIndex].Init(_tcslen(ptszTokenize) + 1);

		_tcsncpy_s(ppTDestination[nIndex].GetBuffer(), _tcslen(ptszTokenize) + 1, ptszTokenize, _TRUNCATE);

		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);

		nIndex++;
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return true;
}

//***************************************************************************
//
bool Tokenize(std::vector<_tstring>& destination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszNextToken = nullptr;

	if( ptszSource == nullptr || ptszToken == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = new TCHAR[nLen + 1];
	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	for( int i = 0; ptszTokenize != NULL; i++ )
	{
		_tstring dest(ptszTokenize);
		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);
		destination.push_back(dest);
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return true;
}

//***************************************************************************
//
void StrUpper(TCHAR* ptszDestination, const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return;

	for( ptszSourceLoc = ptszSource, ptszDestLoc = ptszDestination; *ptszSourceLoc != '\0'; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc >= 'a' && *ptszSourceLoc <= 'z' )
			*ptszDestLoc = *ptszSourceLoc - 'a' + 'A';
		else *ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = '\0';
}

//***************************************************************************
//
void StrLower(TCHAR* ptszDestination, const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return;

	for( ptszSourceLoc = ptszSource, ptszDestLoc = ptszDestination; *ptszSourceLoc != '\0'; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc >= 'A' && *ptszSourceLoc <= 'Z' )
			*ptszDestLoc = *ptszSourceLoc + 'a' - 'A';
		else *ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = '\0';
}

//***************************************************************************
//
void StrReverse(TCHAR* ptszDestination, const TCHAR* ptszSource)
{
	size_t	nIndex = 0;
	size_t	nSrcLen = 0;
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return;

	nSrcLen = _tcslen(ptszSource);
	for( ptszSourceLoc = ptszSource + _tcslen(ptszSource) - 1, ptszDestLoc = ptszDestination; nIndex < nSrcLen; ptszSourceLoc--, ptszDestLoc++ )
	{
		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = '\0';
}

//***************************************************************************
//
void StrAppend(TCHAR* ptszDestination, const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;
	TCHAR* ptszTempLoc = nullptr;

	if( ptszSource == nullptr ) return;

	ptszTempLoc = new TCHAR[_tcslen(ptszSource) + _tcslen(ptszDestination) + 1];

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc != '\0'; ptszSourceLoc++, ptszTempLoc++ )
	{
		*ptszTempLoc = *ptszSourceLoc;
	}

	for( ptszDestLoc = ptszDestination; *ptszDestLoc != '\0'; ptszDestLoc++, ptszTempLoc++ )
	{
		*ptszTempLoc = *ptszDestLoc;
	}
	*ptszTempLoc = '\0';

	for( ptszSourceLoc = ptszTempLoc, ptszDestLoc = ptszDestination; *ptszSourceLoc != '\0'; ptszSourceLoc++, ptszDestLoc++ )
	{
		*ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = '\0';

	if( ptszTempLoc )
	{
		delete[] ptszTempLoc;
		ptszTempLoc = nullptr;
	}
}

//***************************************************************************
//
bool StrUpper(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(nLen + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc >= 'a' && *ptszSourceLoc <= 'z' )
			*ptszDestLoc = *ptszSourceLoc - 'a' + 'A';
		else *ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrLower(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(nLen + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc >= 'A' && *ptszSourceLoc <= 'Z' )
			*ptszDestLoc = *ptszSourceLoc + 'a' - 'A';
		else *ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrReverse(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(nLen + 1);

	for( ptszSourceLoc = ptszSource + nLen - 1, ptszDestLoc = TDestination.GetBuffer(); nIndex < nLen; ptszSourceLoc--, ptszDestLoc++ )
	{
		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrAppend(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr || ptszAppend == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 0 ) return false;

	TDestination.Init(nLen + _tcslen(ptszAppend) + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;

	for( ptszSourceLoc = ptszAppend; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;

	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nStart < 0 || nStart > nLen - 1 ) return false;

	nFirst = nStart;

	TDestination.Init(nLen - nFirst + 1);

	ptszSourceLoc = ptszSource + nStart;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
bool StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const size_t nCount)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nStart < 0 || nStart > nLen - 1 ) return false;
	if( nCount < 0 || nStart + nCount > nLen - 1 ) return false;

	nFirst = nStart;

	TDestination.Init(nCount + 1);

	ptszSourceLoc = ptszSource + nFirst;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( nIndex == nCount ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//Function to cut string until special character 
bool StrMidToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const TCHAR tcToken)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nStart < 0 || nStart > nLen - 1 ) return false;
	if( !_tcschr(ptszSource, tcToken) ) return false;

	nFirst = nStart;

	TDestination.Init(nLen - nFirst + 1);

	ptszSourceLoc = ptszSource + nFirst;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( *ptszSourceLoc == tcToken ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nCount < 0 || nCount > nLen ) return false;

	TDestination.Init(nCount + 1);

	ptszSourceLoc = ptszSource;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( nIndex == nCount ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nCount < 0 || nCount > nLen ) return false;

	TDestination.Init(nCount + 1);

	ptszSourceLoc = ptszSource + nLen - nCount;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
bool StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(nLen + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc == tcSrcToken )
			*ptszDestLoc = tcDestToken;
		else
			*ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = '\0';

	return true;
}

//***************************************************************************
//
bool StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszSrcToken, const TCHAR* ptszDestToken)
{
	bool	bResult = false;
	size_t	nLen = 0;
	size_t	nTokenCount = 0;
	size_t	nIndex = 0;
	size_t	nSubLen = 0;
	size_t	nEndIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	const TCHAR* ptszSrcTemp = nullptr;
	const TCHAR* ptszDestTemp = nullptr;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr || ptszSrcToken == nullptr || ptszDestToken == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( *ptszSourceLoc == *ptszSrcToken )
		{
			bResult = true;
			for( nIndex = 0, ptszSrcTemp = ptszSrcToken; *ptszSrcTemp; ptszSrcTemp++, nIndex++ )
			{
				if( *(ptszSourceLoc + nIndex) != *ptszSrcTemp )
				{
					bResult = false;
					break;
				}
			}

			if( bResult )
			{
				nTokenCount++;
				ptszSourceLoc = ptszSourceLoc + _tcslen(ptszSrcToken) - 1;
			}
		}
	}

	if( nTokenCount < 0 )
	{
		nEndIndex = _tcslen(ptszSource);

		TDestination.Init(nEndIndex + 1);

		_tcsncpy_s(TDestination.GetBuffer(), nEndIndex + 1, ptszSource, _TRUNCATE);

		return true;
	}

	nSubLen = _tcslen(ptszDestToken) - _tcslen(ptszSrcToken);
	nEndIndex = nLen + (nSubLen * nTokenCount);

	TDestination.Init(nEndIndex + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc == *ptszSrcToken )
		{
			bResult = true;
			for( nIndex = 0, ptszSrcTemp = ptszSrcToken; *ptszSrcTemp; ptszSrcTemp++, nIndex++ )
			{
				if( *(ptszSourceLoc + nIndex) != *ptszSrcTemp )
				{
					bResult = false;
					break;
				}
			}

			if( bResult )
			{
				for( ptszDestTemp = ptszDestToken; *ptszDestTemp; ptszDestTemp++, ptszDestLoc++ )
					*ptszDestLoc = *ptszDestTemp;

				ptszSourceLoc = ptszSourceLoc + _tcslen(ptszSrcToken) - 1;
				ptszDestLoc = ptszDestLoc - 1;
			}
			else *ptszDestLoc = *ptszSourceLoc;
		}
		else *ptszDestLoc = *ptszSourceLoc;
	}
	*ptszDestLoc = '\0';

	return true;
}

//***************************************************************************
//
size_t StrFind(const TCHAR* ptszSource, const TCHAR tcCompare, const size_t nStart)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;
	if( nStart < 1 || nStart > nLen ) return -1;

	for( ptszSourceLoc = ptszSource + nStart - 1; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( *ptszSourceLoc == tcCompare ) return nIndex;

		nIndex++;
	}

	return -1;
}

//***************************************************************************
//
size_t StrFind(const TCHAR* ptszSource, const TCHAR* ptszCompare, const size_t nStart)
{
	bool	bResult = false;
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	const TCHAR* ptszSrcTemp = nullptr;

	if( ptszSource == nullptr || ptszCompare == nullptr ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;
	if( nStart < 1 || nStart > nLen ) return -1;

	for( ptszSourceLoc = ptszSource + nStart - 1; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( *ptszSourceLoc == *ptszCompare )
		{
			bResult = true;
			nCount = 0;
			for( ptszSrcTemp = ptszCompare; *ptszSrcTemp; ptszSrcTemp++ )
			{
				if( *(ptszSourceLoc + nCount) != *ptszCompare )
				{
					bResult = false;
					break;
				}

				nCount++;
			}

			if( bResult ) return nIndex;
		}

		nIndex++;
	}

	return -1;
}

//***************************************************************************
//
size_t StrReverseFind(const TCHAR* ptszSource, const TCHAR tcCompare)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;

	for( ptszSourceLoc = ptszSource + nLen - 1; nIndex < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == tcCompare ) return nIndex;

		nIndex++;
	}

	return -1;
}

//***************************************************************************
//
size_t StrReverseFind(const TCHAR* ptszSource, const TCHAR* ptszCompare)
{
	bool	bResult = false;
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	const TCHAR* ptszSrcTemp = nullptr;

	if( ptszSource == nullptr || ptszCompare == nullptr ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;

	for( ptszSourceLoc = ptszSource + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == *ptszCompare )
		{
			bResult = true;
			nCount = 0;
			for( ptszSrcTemp = ptszCompare; *ptszSrcTemp; ptszSrcTemp++ )
			{
				if( *(ptszSourceLoc + nCount) != *ptszCompare )
				{
					bResult = false;
					break;
				}

				nCount++;
			}

			if( bResult ) return nIndex;
		}
	}

	return -1;
}

//***************************************************************************
//
void TrimLeft(TCHAR* pszSource)
{
	size_t	nDataLength = 0;
	TCHAR* pszSourceLoc = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	while( _istspace(*pszSourceLoc) )
		pszSourceLoc = _tcsinc(pszSourceLoc);

	if( pszSourceLoc != pszSource )
	{
		// fix up data and length
		nDataLength = _tcslen(pszSource) - (pszSourceLoc - pszSource);
		memmove(pszSource, pszSourceLoc, nDataLength + 1 * sizeof(TCHAR));
	}
}

//***************************************************************************
//Function to delete space from string-startpoint
void TrimLeft(TCHAR* pszSource, size_t nDataLength)
{
	// find first non-space character
	TCHAR* pszSourceLoc = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	for( size_t n = 0; n < nDataLength; n++ )
	{
		if( _istspace(*pszSourceLoc) )
			pszSourceLoc = _tcsinc(pszSourceLoc);
		else break;
	}

	if( pszSourceLoc != pszSource )
	{
		// fix up data and length
		nDataLength = nDataLength - (pszSourceLoc - pszSource);
		memmove(pszSource, pszSourceLoc, (nDataLength + 1) * sizeof(TCHAR));
	}
}

//***************************************************************************
//
void TrimLeft(TCHAR* pszSource, TCHAR cToken)
{
	size_t	nDataLength = 0;
	TCHAR* pszSourceLoc = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	while( *pszSourceLoc == cToken )
		pszSourceLoc = _tcsinc(pszSourceLoc);

	if( pszSourceLoc != pszSource )
	{
		// fix up data and length
		nDataLength = _tcslen(pszSource) - (pszSourceLoc - pszSource);
		memmove(pszSource, pszSourceLoc, (nDataLength + 1) * sizeof(TCHAR));
	}
}

//***************************************************************************
//
void TrimLeft(TCHAR* pszSource, TCHAR* pszToken)
{
	size_t	i = 0;
	size_t	nDataLength = 0;
	TCHAR* pszSourceLoc = nullptr;
	TCHAR* pszTemp = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	while( *pszSourceLoc != '\0' )
	{
		if( _tcschr(pszToken, *pszSourceLoc) == nullptr )
			break;
		pszSourceLoc = _tcsinc(pszSourceLoc);
	}

	if( pszSourceLoc != pszSource )
	{
		// fix up data and length
		nDataLength = _tcslen(pszSource) - (pszSourceLoc - pszSource);
		memmove(pszSource, pszSourceLoc, (nDataLength + 1) * sizeof(TCHAR));
	}
}

//***************************************************************************
//
bool TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	ptszSourceLoc = ptszSource;
	while( _istspace(*ptszSourceLoc) )
		ptszSourceLoc++;

	TDestination.Init(_tcslen(ptszSourceLoc) + 1);

	for( ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR ctToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	ptszSourceLoc = ptszSource;
	while( *ptszSourceLoc == ctToken )
		ptszSourceLoc++;

	TDestination.Init(_tcslen(ptszSourceLoc) + 1);

	for( ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	ptszSourceLoc = ptszSource;
	while( *ptszSourceLoc )
	{
		if( _tcschr(ptszToken, *ptszSourceLoc) == nullptr )
			break;
		ptszSourceLoc++;
	}

	TDestination.Init(_tcslen(ptszSourceLoc) + 1);

	for( ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
void TrimRight(TCHAR* ptszSource)
{
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* lpszLast = nullptr;

	if( ptszSource == nullptr ) return;

	ptszSourceLoc = ptszSource;
	while( *ptszSourceLoc != '\0' )
	{
		if( _istspace(*ptszSourceLoc) )
		{
			if( lpszLast == nullptr ) lpszLast = ptszSourceLoc;
		}
		else lpszLast = nullptr;

		ptszSourceLoc = _tcsinc(ptszSourceLoc);
	}

	if( lpszLast != NULL )
	{
		// truncate at trailing space start
		if( *lpszLast & 0x80 )
			*(lpszLast + 1) = '\0';
		else *lpszLast = '\0';
	}
}

//***************************************************************************
//Function to change first space to Null Terminate
void TrimRight(TCHAR* pszSource, size_t nDataLength)
{
	size_t	nLen = 0;
	TCHAR* pszSourceLoc = nullptr;
	TCHAR* lpszLast = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	nLen = _tcslen(pszSource);

	for( size_t n = 0; n < nDataLength; n++ )
	{
		if( *pszSourceLoc == '\0' ) break;

		if( _istspace(*pszSourceLoc) )
		{
			if( lpszLast == nullptr ) lpszLast = pszSourceLoc;
		}
		else lpszLast = nullptr;

		pszSourceLoc = _tcsinc(pszSourceLoc);
	}

	if( lpszLast != NULL )
	{
		*lpszLast = '\0';
	}
}

//***************************************************************************
//
void TrimRight(TCHAR* pszSource, TCHAR cToken)
{
	TCHAR* pszSourceLoc = nullptr;
	TCHAR* lpszLast = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	while( *pszSourceLoc != '\0' )
	{
		if( *pszSourceLoc == cToken )
		{
			if( lpszLast == nullptr ) lpszLast = pszSourceLoc;
		}
		else lpszLast = nullptr;

		pszSourceLoc = _tcsinc(pszSourceLoc);
	}

	if( lpszLast != NULL )
	{
		// truncate at trailing space start
		if( *lpszLast & 0x80 )
			*(lpszLast + 1) = '\0';
		else *lpszLast = '\0';
	}
}

//***************************************************************************
//
void TrimRight(TCHAR* pszSource, TCHAR* pszToken)
{
	TCHAR* pszSourceLoc = nullptr;
	TCHAR* lpszLast = nullptr;

	if( pszSource == nullptr ) return;

	pszSourceLoc = pszSource;
	while( *pszSourceLoc != '\0' )
	{
		if( _tcschr(pszToken, *pszSourceLoc) != NULL )
		{
			if( lpszLast == nullptr ) lpszLast = pszSourceLoc;
		}
		else lpszLast = nullptr;

		pszSourceLoc = _tcsinc(pszSourceLoc);
	}

	if( lpszLast != NULL )
	{
		// truncate at trailing space start
		if( *lpszLast & 0x80 )
			*(lpszLast + 1) = '\0';
		else *lpszLast = '\0';
	}
}

//***************************************************************************
//
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( _istspace(*ptszSourceLoc) ) nCount++;
		else break;
	}

	TDestination.Init(nLen - nCount + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR ctToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == ctToken ) nCount++;
		else break;
	}

	TDestination.Init(nLen - nCount + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( _tcschr(ptszToken, *ptszSourceLoc) == nullptr )
			break;
		nCount++;
	}

	TDestination.Init(nLen - nCount + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLen - nCount ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCutUnicodeAscii(TCHAR* ptszDestination, const TCHAR* ptszSource, const size_t nSize)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;
	size_t	nEndIndex = 0;
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nSize < 0 || nSize > nLen ) return false;

#ifndef _UNICODE
	size_t  nCount;

	for( nCount = 0, ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !((unsigned)(*ptszSourceLoc) < 0x80) ) nCount++;
	}

	if( nCount % 2 == 1 )
		nEndIndex = nSize + (nCount / 2) + 1;
	else nEndIndex = nSize + (nCount / 2);
#else
	nEndIndex = nSize;
#endif

	for( ptszSourceLoc = ptszSource, ptszDestLoc = ptszDestination; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nEndIndex ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCutUnicodeAscii(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nSize)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;
	size_t	nEndIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nSize < 0 || nSize > nLen ) return false;

#ifndef _UNICODE
	for( nCount = 0, ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !((unsigned)(*ptszSourceLoc) < 0x80) ) nCount++;
	}

	if( nCount % 2 == 1 )
		nEndIndex = nSize + (nCount / 2) + 1;
	else nEndIndex = nSize + (nCount / 2);
#else
	nEndIndex = nSize;
#endif

	TDestination.Init(nEndIndex + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nEndIndex ) break;

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nLocation == 0 ) return false;

	if( (nLen % nLocation) == 0 )
		nCount = nLen / nLocation - 1;
	else
		nCount = nLen / nLocation;	// ((int)nValue / (int)nDiv) 은 항상 소숫점 버림

	TDestination.Init(nLen + nCount + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLocation )
		{
			*ptszDestLoc = tcToken;
			ptszDestLoc++;
			nIndex = 0;
		}

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( nLocation == 0 ) return false;

	if( (nLen % nLocation) == 0 )
		nCount = nLen / nLocation - 1;
	else
		nCount = nLen / nLocation;	// ((int)nValue / (int)nDiv) 은 항상 소숫점 버림

	TDestination.Init(nLen + (nCount * _tcslen(ptszToken)) + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( nIndex == nLocation )
		{
			while( ptszToken )
			{
				*ptszDestLoc = *ptszToken;
				ptszDestLoc++;
				ptszToken++;
			}

			nIndex = 0;
		}

		*ptszDestLoc = *ptszSourceLoc;
		nIndex++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

