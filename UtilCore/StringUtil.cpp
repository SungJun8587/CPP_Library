
//***************************************************************************
// StringUtil.cpp: implementation of the StringUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "StringUtil.h"

//***************************************************************************
// CodePage
//	- CP_ACP : 시스템 기본 Windows ANSI 코드 페이지
//	- CP_MACCP : Macintosh 코드 페이지
//	- CP_OEMCP : OEM 코드 페이지
//	- CP_SYMBOL : 기호 코드 페이지(42)
//	- CP_THREAD_ACP : 현재 스레드에 대한 Windows ANSI 코드 페이지
//	- CP_UTF7 : UTF-7
//	- CP_UTF8 : UTF-8
// 
// sizeof, _countof 비교
//	sizeof : 할당받는 메모리 크기
//  _countof : 배열에 개수
//	Ex)
//		wchar_t wszBuffer[100];
//		sizeof(wszBuffer) : sizeof(wchar_t) * 100 = 200
//		_countof(wszBuffer) : 100
//
// 		char szSource[20] = "안녕123하세요";
//		wchar_t wszSource[20] = L"안녕123하세요";
//  
//		int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szSource, -1, NULL, 0);		// 버퍼에 기록된 문자 수를 반환 : nLength(9) = 문자수(8) + 1('\0')
//		nLength = WideCharToMultiByte(CP_ACP, 0, wszSource, -1, NULL, 0, NULL, NULL);   // 버퍼에 기록된 바이트 수를 반환 : nLength(14) = 바이트 수(13) + 1('\0')
//      nLength = strlen(szSource);     // 멀티바이트 바이트 수                           // 한글은 2바이트, 아스키 문자는 1바이트 : nLength(13)
//      nLength = wcslen(wszSource);	// 와이드바이트 문자 수							// 버퍼에 기록된 문자 수를 반환 : nLength(8) 
size_t GetMultiByteLen(int nCodePage, const TCHAR* ptszSource)
{
	int	nLength = 0;

	if( !ptszSource ) return -1;
	if( (nLength = (int)_tcslen(ptszSource)) < 1 ) return -1;

#ifdef _UNICODE
	nLength = WideCharToMultiByte(nCodePage, 0, ptszSource, nLength + 1, NULL, 0, NULL, NULL);
	if( nLength == 0 ) return -1;
	nLength--;
#endif

	return nLength;
}

//***************************************************************************
// 멀티바이트 문자열을 와이드바이트 문자열로 변환
//	[out] wchar_t* pwszDestination : 대상 문자열
//	[in] size_t count : 대상 문자열 할당한 버퍼 크기(sizeof(pszSource))
//	[in] const char* pszSource : 원본 문자열
BOOL MultiByteToWideCharStr(wchar_t* pwszDestination, size_t count, const char* pszSource)
{
	int nLength;
	int nSrcLen = (int)strlen(pszSource) + 1;

	if( !pszSource ) return false;
	if( nSrcLen <= 1 ) return false;
	if( !pwszDestination ) return false;

	// 버퍼에 기록된 문자 수를 반환 : nLength = 문자수 + 1('\0')
	nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, NULL, 0);
	if( nLength == 0 || count < (size_t)nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, pwszDestination, nLength) == 0 ) return false;

	return true;
}

//***************************************************************************
// 와이드바이트 문자열을 멀티바이트 문자열로 변환
//	[out] char* pszDestination : 대상 문자열
//	[in] size_t count : 대상 문자열 할당한 버퍼 크기(sizeof(pwszSource))
//	[in] const wchar_t* pwszSource : 원본 문자열
BOOL WideCharToMultiByteStr(char* pszDestination, size_t count, const wchar_t* pwszSource)
{
	int nLength;
	int nSrcLen = (int)wcslen(pwszSource) + 1;

	if( !pwszSource ) return false;
	if( nSrcLen <= 1 ) return false;
	if( !pszDestination ) return false;

	// 버퍼에 기록된 바이트 수를 반환 : nLength = 바이트 수 + 1('\0')
	nLength = WideCharToMultiByte(CP_ACP, 0, pwszSource, nSrcLen, NULL, 0, NULL, NULL);
	if( nLength == 0 || count < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszSource, nSrcLen, (LPSTR)pszDestination, nLength, NULL, NULL) == 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL MultiByteToWideCharStr(CMemBuffer<wchar_t>& WDestination, const char* pszSource)
{
	int		nLength = 0;
	int		nSrcLen = (int)strlen(pszSource) + 1;
	wchar_t* pwszDestination = NULL;

	if( !pszSource ) return false;
	if( strlen(pszSource) < 1 ) return false;

	// 버퍼에 기록된 문자 수를 반환
	//	- nLength은 문자 수 있으면 버퍼 크기를 sizeof(wchar_t) * nLength;
	if( (nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, NULL, 0)) == 0 ) return false;

	WDestination.Init(nLength);

	pwszDestination = WDestination.GetBuffer();

	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, pwszDestination, nLength) == 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL WideCharToMultiByteStr(CMemBuffer<char>& Destination, const wchar_t* pwszSource)
{
	int		nLength = 0;
	int		nSrcLen = (int)wcslen(pwszSource) + 1;
	char* pszDestination = NULL;

	if( !pwszSource ) return false;
	if( wcslen(pwszSource) < 1 ) return false;

	// 버퍼에 기록된 바이트 수를 반환(
	if( (nLength = WideCharToMultiByte(CP_ACP, 0, pwszSource, nSrcLen, NULL, 0, NULL, NULL)) == 0 ) return false;

	Destination.Init(nLength);

	pszDestination = Destination.GetBuffer();

	if( WideCharToMultiByte(CP_ACP, 0, pwszSource, nSrcLen, (LPSTR)pszDestination, nLength, NULL, NULL) == 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL AnsiToUTF8(CMemBuffer<char>& Destination, const char* pszSource)
{
	int		nLength = 0;
	int		nSrcLen = (int)strlen(pszSource) + 1;
	char* pszMessage = NULL;
	char* pszDestination = NULL;
	wchar_t* pwszMessage = NULL;

	if( !pszSource ) return false;
	if( strlen(pszSource) < 1 ) return false;

	if( (nLength = MultiByteToWideChar(CP_UTF8, 0, (LPSTR)pszSource, nSrcLen, NULL, 0)) == 0 ) return false;

	pwszMessage = new wchar_t[nLength];

	if( MultiByteToWideChar(CP_UTF8, 0, (LPSTR)pszSource, nSrcLen, pwszMessage, nLength) == 0 )
	{
		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	nSrcLen = (int)wcslen(pwszMessage) + 1;
	if( (nLength = WideCharToMultiByte(CP_ACP, 0, pwszMessage, nSrcLen, NULL, 0, NULL, NULL)) == 0 )
	{
		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	pszMessage = new char[nLength];

	if( WideCharToMultiByte(CP_ACP, 0, pwszMessage, nSrcLen, (LPSTR)pszMessage, nLength, NULL, NULL) == 0 )
	{
		if( pszMessage )
		{
			delete[]pszMessage;
			pszMessage = NULL;
		}

		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	Destination.Init(nLength);

	pszDestination = Destination.GetBuffer();

	strncpy_s(pszDestination, nLength, pszMessage, _TRUNCATE);

	if( pszMessage )
	{
		delete[]pszMessage;
		pszMessage = NULL;
	}

	if( pwszMessage )
	{
		delete[]pwszMessage;
		pwszMessage = NULL;
	}

	return true;
}

//***************************************************************************
//
BOOL UnicodeToUTF8(CMemBuffer<wchar_t>& WDestination, const char* pszSource)
{
	int		nLength = 0;
	int     nSrcLen = (int)strlen(pszSource) + 1;
	wchar_t* pwszDestination = NULL;

	if( !pszSource ) return false;
	if( nSrcLen <= 1 ) return false;

	if( (nLength = MultiByteToWideChar(CP_UTF8, 0, (LPSTR)pszSource, nSrcLen, NULL, 0)) == 0 ) return false;

	WDestination.Init(nLength);

	pwszDestination = WDestination.GetBuffer();

	if( MultiByteToWideChar(CP_UTF8, 0, (LPSTR)pszSource, nSrcLen, pwszDestination, nLength) == 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL UTF8ToAnsi(CMemBuffer<char>& Destination, const char* pszSource)
{
	int		nLength = 0;
	int     nSrcLen = (int)strlen(pszSource) + 1;
	char* pszMessage = NULL;
	char* pszDestination = NULL;
	wchar_t* pwszMessage = NULL;

	if( !pszSource ) return false;
	if( nSrcLen <= 1 ) return false;

	if( (nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, NULL, 0)) == 0 ) return false;

	pwszMessage = new wchar_t[nLength];

	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, pwszMessage, nLength) == 0 )
	{
		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	nSrcLen = (int)wcslen(pwszMessage) + 1;
	if( (nLength = WideCharToMultiByte(CP_UTF8, 0, pwszMessage, nSrcLen, NULL, 0, NULL, NULL)) == 0 )
	{
		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	pszMessage = new char[nLength];

	if( WideCharToMultiByte(CP_UTF8, 0, pwszMessage, nSrcLen, (LPSTR)pszMessage, nLength, NULL, NULL) == 0 )
	{
		if( pszMessage )
		{
			delete[]pszMessage;
			pszMessage = NULL;
		}

		if( pwszMessage )
		{
			delete[]pwszMessage;
			pwszMessage = NULL;
		}

		return false;
	}

	Destination.Init(nLength);

	pszDestination = Destination.GetBuffer();

	strncpy_s(pszDestination, nLength, pszMessage, _TRUNCATE);

	if( pszMessage )
	{
		delete[]pszMessage;
		pszMessage = NULL;
	}

	if( pwszMessage )
	{
		delete[]pwszMessage;
		pwszMessage = NULL;
	}

	return true;
}

//***************************************************************************
//
BOOL UTF8ToUnicode(CMemBuffer<char>& Destination, const wchar_t* pwszBuffer)
{
	int		nLength = 0;
	int     nSrcLen = (int)wcslen(pwszBuffer) + 1;
	char* pszDestination = NULL;

	if( !pwszBuffer ) return false;
	if( nSrcLen <= 1 ) return false;

	if( (nLength = WideCharToMultiByte(CP_UTF8, 0, pwszBuffer, nSrcLen, NULL, 0, NULL, NULL)) == 0 ) return false;

	Destination.Init(nLength);

	pszDestination = Destination.GetBuffer();

	if( WideCharToMultiByte(CP_UTF8, 0, pwszBuffer, nSrcLen, (LPSTR)pszDestination, nLength, NULL, NULL) == 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL TCharToByte(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer)
{
	int		nLength = 0;
	int     nSrcLen = 0;
	char* pszSource = NULL;

	pszSource = (char*)pbBuffer;
	nSrcLen = (int)strlen(pszSource) + 1;

#ifdef _UNICODE
	if( (nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, NULL, 0)) == 0 ) return false;

	TDestination.Init(nLength);

	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, TDestination.GetBuffer(), nLength) == 0 ) return false;
#else
	nLength = nSrcLen;

	TDestination.Init(nLength);

	_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszSource, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL ByteToTChar(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer)
{
	int		nLength = 0;
	int     nSrcLen = (int)_tcslen(ptszBuffer) + 1;
	char* pszDestination = NULL;

#ifdef _UNICODE
	if( (nLength = WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, (LPSTR)pszDestination, 0, NULL, NULL)) == 0 ) return false;

	Destination.Init(nLength);

	pszDestination = (char*)Destination.GetBuffer();

	if( WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, (LPSTR)pszDestination, (int)nLength, NULL, NULL) == 0 ) return false;
#else
	nLength = nSrcLen;

	Destination.Init(nLength);

	pszDestination = (char*)Destination.GetBuffer();

	_tcsncpy_s(pszDestination, nLength, ptszBuffer, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//Function to passing FolderPath to FullFilePath 
BOOL FolderPathPassing(TCHAR* ptszFolderPath, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullFilePath || !ptszFolderPath ) return false;
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
BOOL FileNameExtPathPassing(TCHAR* ptszFileNameExt, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullFilePath || !ptszFileNameExt ) return false;
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
BOOL FileNameExtPassing(TCHAR* ptszFileName, TCHAR* ptszFileExt, const TCHAR* ptszFileNameExt)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFileNameExt || !ptszFileName || !ptszFileExt ) return false;
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
BOOL FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullFilePath ) return false;
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
BOOL FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullFilePath ) return false;
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
BOOL FileNameExtPassing(CMemBuffer<TCHAR>& TFileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFileNameExt ) return false;
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
BOOL ParseURL(TCHAR* ptszProtocol, TCHAR* ptszHostName, TCHAR* ptszRequest, int& nPort, const TCHAR* ptszFullUrl)
{
	size_t	nTotalLen;
	size_t	nProtocolLen;
	size_t	nHostLen;
	size_t	nRequestLen;
	TCHAR* ptszWork = NULL;
	TCHAR* ptszPoint1 = NULL;
	TCHAR* ptszPoint2 = NULL;

	nPort = 80;

	if( !ptszFullUrl ) return false;
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
BOOL HostNamePassing(TCHAR* ptszHostName, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl || !ptszHostName ) return false;
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
BOOL UrlFullPathPassing(TCHAR* ptszUrlFullPath, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl ) return false;
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
BOOL QueryStringPassing(TCHAR* ptszQueryString, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl ) return false;
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
BOOL HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl ) return false;
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
BOOL UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl ) return false;
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
BOOL QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszFullUrl ) return false;
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
	size_t		nCount = 0;
	TCHAR* ptszTokenize = NULL;
	TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszNextToken = NULL;

	if( !ptszSource || !ptszToken ) return -1;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return -1;
	if( !_tcsstr(ptszSource, ptszToken) ) return -1;

	ptszSourceLoc = new TCHAR[nLen + 1];

	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ptszTokenize = _tcstok_s(NULL, ptszToken, &ptszNextToken);

		nCount++;
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = NULL;
	}

	return nCount;
}

//***************************************************************************
//
BOOL Tokenize(TCHAR** pptszDestination, int& nCount, TCHAR* ptszSource, const TCHAR* ptszToken, const int nSize)
{
	int     nTempCount = 0;
	TCHAR* ptszTokenize = NULL;
	TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszNextToken = NULL;

	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = ptszSource;
	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);

	for( int i = 0; ptszTokenize != NULL; i++ )
	{
		_tcscpy_s(*(pptszDestination + i), MAX_BUFFER_SIZE, ptszTokenize);
		ptszTokenize = _tcstok_s(NULL, ptszToken, &ptszNextToken);
		nTempCount++;
	}
	nCount = nTempCount;

	return true;
}

//***************************************************************************
//
BOOL Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t		nIndex = 0;
	TCHAR* ptszTokenize = NULL;
	TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszNextToken = NULL;

	if( !ptszSource || !ptszToken ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;
	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = new TCHAR[nLen + 1];

	_tcsncpy_s(ptszSourceLoc, nLen + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ppTDestination[nIndex].Init(_tcslen(ptszTokenize) + 1);

		_tcsncpy_s(ppTDestination[nIndex].GetBuffer(), _tcslen(ptszTokenize) + 1, ptszTokenize, _TRUNCATE);

		ptszTokenize = _tcstok_s(NULL, ptszToken, &ptszNextToken);

		nIndex++;
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = NULL;
	}

	return true;
}

//***************************************************************************
//
void StrUpper(TCHAR* ptszDestination, const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

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
	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

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
	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

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
	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;
	TCHAR* ptszTempLoc = NULL;

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
		delete ptszTempLoc;
		ptszTempLoc = NULL;
	}
}

//***************************************************************************
//
BOOL StrUpper(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrLower(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrReverse(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrAppend(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource || !ptszAppend ) return false;
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
BOOL StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const size_t nCount)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrMidToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const TCHAR tcToken)
{
	size_t	nLen = 0;
	size_t	nFirst = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszSrcToken, const TCHAR* ptszDestToken)
{
	size_t	nLen = 0;
	size_t	nTokenCount = 0;
	size_t	nIndex = 0;
	size_t	nSubLen = 0;
	size_t	nEndIndex = 0;
	BOOL		bResult;

	const TCHAR* ptszSourceLoc = NULL;
	const TCHAR* ptszSrcTemp = NULL;
	const TCHAR* ptszDestTemp = NULL;
	TCHAR* ptszTokenize = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource || !ptszSrcToken || !ptszDestToken ) return false;
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

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return -1;
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
	BOOL	bResult = false;
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	const TCHAR* ptszSrcTemp = NULL;

	if( !ptszSource || !ptszCompare ) return -1;
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

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return -1;
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
	BOOL	bResult = false;
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	const TCHAR* ptszSrcTemp = NULL;

	if( !ptszSource || !ptszCompare ) return -1;
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
	TCHAR* pszSourceLoc = pszSource;

	if( pszSourceLoc == NULL )	return;

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
	TCHAR* pszSourceLoc = NULL;

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
	TCHAR* pszSourceLoc = pszSource;

	if( pszSourceLoc == NULL )	return;

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
	TCHAR* pszSourceLoc = pszSource;
	TCHAR* pszTemp = NULL;

	if( pszSourceLoc == NULL ) return;

	while( *pszSourceLoc != '\0' )
	{
		if( _tcschr(pszToken, *pszSourceLoc) == NULL )
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
BOOL TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nCount = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR ctToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	ptszSourceLoc = ptszSource;
	while( *ptszSourceLoc )
	{
		if( _tcschr(ptszToken, *ptszSourceLoc) == NULL )
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
void TrimRight(TCHAR* pszSource)
{
	TCHAR* pszSourceLoc = pszSource;
	TCHAR* lpszLast = NULL;

	if( pszSourceLoc == NULL )	return;

	while( *pszSourceLoc != '\0' )
	{
		if( _istspace(*pszSourceLoc) )
		{
			if( lpszLast == NULL )	lpszLast = pszSourceLoc;
		}
		else lpszLast = NULL;

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
//Function to change first space to Null Terminate
void TrimRight(TCHAR* pszSource, size_t nDataLength)
{
	size_t	nLen = 0;
	TCHAR* pszSourceLoc = NULL;
	TCHAR* lpszLast = NULL;

	pszSourceLoc = pszSource;
	nLen = _tcslen(pszSource);

	for( size_t n = 0; n < nDataLength; n++ )
	{
		if( *pszSourceLoc == '\0' ) break;

		if( _istspace(*pszSourceLoc) )
		{
			if( lpszLast == NULL ) lpszLast = pszSourceLoc;
		}
		else lpszLast = NULL;

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
	TCHAR* pszSourceLoc = pszSource;
	TCHAR* lpszLast = NULL;

	if( pszSourceLoc == NULL )	return;

	while( *pszSourceLoc != '\0' )
	{
		if( *pszSourceLoc == cToken )
		{
			if( lpszLast == NULL )	lpszLast = pszSourceLoc;
		}
		else lpszLast = NULL;

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
	TCHAR* pszSourceLoc = pszSource;
	TCHAR* lpszLast = NULL;

	if( pszSourceLoc == NULL )	return;

	while( *pszSourceLoc != '\0' )
	{
		if( _tcschr(pszToken, *pszSourceLoc) != NULL )
		{
			if( lpszLast == NULL ) lpszLast = pszSourceLoc;
		}
		else lpszLast = NULL;

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
BOOL TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR ctToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
	if( (nLen = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + nLen - 1; nCount < nLen; ptszSourceLoc-- )
	{
		if( _tcschr(ptszToken, *ptszSourceLoc) == NULL )
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
BOOL StrCutUnicodeAscii(TCHAR* ptszDestination, const TCHAR* ptszSource, const size_t nSize)
{
	size_t	nLen = 0;
	size_t	nIndex = 0;
	size_t	nEndIndex = 0;
	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrCutUnicodeAscii(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nSize)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;
	size_t	nEndIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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
BOOL StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken)
{
	size_t	nLen = 0;
	size_t	nCount = 0;
	size_t	nIndex = 0;

	const TCHAR* ptszSourceLoc = NULL;
	TCHAR* ptszDestLoc = NULL;

	if( !ptszSource ) return false;
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

