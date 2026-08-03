
//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"
#include "StringFunc.h"

//***************************************************************************
//
void Print(const char* pszContent, const char* pszBuffer)
{
	printf("%s = %s\r\n", pszContent, pszBuffer);
}

//***************************************************************************
//
void PrintW(const wchar_t* pwszContent, const wchar_t* pwszBuffer)
{
	wprintf(L"%s = %s\r\n", pwszContent, pwszBuffer);
}

//***************************************************************************
//
void PrintT(const TCHAR* ptszContent, const TCHAR* ptszBuffer)
{
	_tprintf(_T("%s = %s\r\n"), ptszContent, ptszBuffer);
}

//***************************************************************************
//
void TestStringFunc()
{
	func_memcpy_s();
	func_strcopy_s();
	func_strncopy_s();
	func_strcat_s();
	func_strncat_s();
	func_strupr_s();
	func_strlwr_s();
	func_strset_s();
	func_strnset_s();
	func_strtok_s();
	func_mbstowcs_s();
	func_wcstombs_s();
	func_strerror_s();
	func_sprintf_s();
	func_snprintf_s();
	func_vsprintf_s(_T("%s %s %s %s %s %s %s"), _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat"));
	func_vsnprintf_s(_T("%s %s %s %s %s %s %s"), _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat"));

	/*
	func_memcpy();
	func_memset();
	func_memmove();
	func_memcmp();
	func_memchr();
	func_sizeof();
	func_countof();
	func_strlen();
	func_strcopy();
	func_strncopy();
	func_strcat();
	func_strncat();
	func_strcmp();
	func_strncmp();
	func_stricmp();
	func_strnicmp();
	func_strcoll();
	func_strchr();
	func_strrchr();
	func_strstr();
	func_strupr();
	func_strlwr();
	func_strset();
	func_strnset();
	func_strrev();
	func_strtok();
	func_strpbrk();
	func_strcspn();
	func_strspn();
	func_strxfrm();
	func_mbstowcs();
	func_wcstombs();
	func_strerror();
	func_printf();
	func_sprintf();
	func_snprintf();
	func_vprintf(_T("%s %s %s %s %s %s %s"), _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat"));
	func_vsprintf(_T("%s %s %s %s %s %s %s"), _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat"));
	func_vsnprintf(_T("%s %s %s %s %s %s %s"), _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat"));
	*/
}

void TestMemBuffer()
{
#ifdef	__MEMBUFFER_H__
	size_t	nTokenCount = 0;

	CMemBuffer<TCHAR> TFolderPath;
	CMemBuffer<TCHAR> TFileNameExt;
	CMemBuffer<TCHAR> TFileName;
	CMemBuffer<TCHAR> TFileExt;

	CMemBuffer<TCHAR> THostName;
	CMemBuffer<TCHAR> TUrlFullPath;
	CMemBuffer<TCHAR> TQueryString;

	CMemBuffer<TCHAR> TDest1;
	CMemBuffer<TCHAR> TDest2;
	CMemBuffer<TCHAR> TDest3;
	CMemBuffer<TCHAR> TDest4;
	CMemBuffer<TCHAR> TDest5;
	CMemBuffer<TCHAR> TDest6;
	CMemBuffer<TCHAR> TDest7;
	CMemBuffer<TCHAR> TDest8;
	CMemBuffer<TCHAR> TDest9;
	CMemBuffer<TCHAR> TDest10;
	CMemBuffer<TCHAR> TDest11;
	CMemBuffer<TCHAR> TDest12;
	CMemBuffer<TCHAR> TDest13;
	CMemBuffer<TCHAR> TDest14;
	CMemBuffer<TCHAR> TDest15;
	CMemBuffer<TCHAR> TDest16;

	CMemBuffer<TCHAR>* pStringBuffer = NULL;

	TCHAR tszFullPath[FULLPATH_STRLEN];
	TCHAR tszUrl[REQUEST_URL_STRLEN];
	TCHAR tszBuffer[MAX_BUFFER_SIZE];

	_tcsncpy_s(tszFullPath, FULLPATH_STRLEN, _T("C:\\ABC\\DDD\\FFF\\aaa.zip"), _TRUNCATE);
	_tcsncpy_s(tszUrl, REQUEST_URL_STRLEN, _T("https://gundam.netmarble.net:8080/community/bbs/view.asp?menu=5_1_1&tbName=Free&idx=1382167"), _TRUNCATE);
	_tcsncpy_s(tszBuffer, MAX_BUFFER_SIZE, _T(".jpg;.png;.bmp;.gif;.doc;.ppt"), _TRUNCATE);

	FolderPathPassing(TFolderPath, tszFullPath);
	FileNameExtPathPassing(TFileNameExt, tszFullPath);
	FileNameExtPassing(TFileName, TFileExt, TFileNameExt.GetBuffer());

	HostNamePassing(THostName, tszUrl);
	UrlFullPathPassing(TUrlFullPath, tszUrl);
	QueryStringPassing(TQueryString, tszUrl);

	nTokenCount = TokenCount(tszBuffer, _T(";"));
	if( nTokenCount > 0 )
	{
		pStringBuffer = new CMemBuffer<TCHAR>[nTokenCount];

		Tokenize(pStringBuffer, tszBuffer, _T(";"));

		if( pStringBuffer )
		{
			delete[]pStringBuffer;
			pStringBuffer = NULL;
		}
	}

	PrintT(_T("- [FullPath]"), tszFullPath);
	PrintT(_T("- [FolderPath]"), TFolderPath.GetBuffer());
	PrintT(_T("- [FileNameExt]"), TFileNameExt.GetBuffer());
	PrintT(_T("- [FileName]"), TFileName.GetBuffer());
	PrintT(_T("- [FileExt]"), TFileExt.GetBuffer());
	_tcout << _T("\r\n");

	PrintT(_T("- [URL]"), tszUrl);
	PrintT(_T("- [HostNamePort]"), THostName.GetBuffer());
	PrintT(_T("- [UrlFullPath]"), TUrlFullPath.GetBuffer());
	PrintT(_T("- [QueryString]"), TQueryString.GetBuffer());

	StrUpper(TDest1, _T("ABCdefDVSDFSdfsdfds"));
	StrLower(TDest2, _T("ABCdefDVSDFSdfsdfds"));
	StrReverse(TDest3, _T("ABCDEFGHIJK12_"));
	StrAppend(TDest4, _T("ABCDEFGHIJK_"), _T("12345"));

	StrMid(TDest5, _T("12345"), 2);
	StrMid(TDest6, _T("123456789"), 1, 3);
	StrLeft(TDest7, _T("123456789"), 5);
	StrRight(TDest8, _T("123456789"), 5);

	StrMid(TDest1, _T("안녕하세요 만나서 반갑습니다"), 2);
	StrMid(TDest2, _T("안녕하세요 만나서 반갑습니다"), 2, 5);
	StrLeft(TDest3, _T("안녕하세요 만나서 반갑습니다"), 5);
	StrRight(TDest4, _T("안녕하세요 만나서 반갑습니다"), 5);
	StrReplace(TDest9, _T("ABCDEFGCDGKCD1C2CDCDC2DC"), _T("CD"), _T("^^^^^^"));
	StrCutUnicodeAscii(TDest10, _T("AB안녕하CC안녕D요DC"), 8);

	TrimLeft(TDest11, _T("      ADB DDD CCC 111 222"));
	TrimLeft(TDest12, _T("&&&ADB DDD FFF 555 666"), '&');
	TrimLeft(TDest13, _T("2&12&12&12 &12 DDD FFF 555 666"), _T("&12"));
	TrimRight(TDest14, _T("ADB DDD CCC 111 222_      "));
	TrimRight(TDest15, _T("ADB DDD CCC 111 222_&&&&&&"), '&');
	TrimRight(TDest16, _T("ADB DDD CCC 111 222_&112&12&12*&21"), _T("&12"));

	PrintT(_T("StrUpper"), TDest1.GetBuffer());
	PrintT(_T("StrLower"), TDest2.GetBuffer());
	PrintT(_T("StrReverse"), TDest3.GetBuffer());
	PrintT(_T("StrAppend"), TDest4.GetBuffer());
	PrintT(_T("StrMid"), TDest5.GetBuffer());
	PrintT(_T("StrMid"), TDest6.GetBuffer());
	PrintT(_T("StrLeft"), TDest7.GetBuffer());
	PrintT(_T("StrRight"), TDest8.GetBuffer());
	PrintT(_T("StrReplace"), TDest9.GetBuffer());
	PrintT(_T("CutLenUnicode아스키 "), TDest10.GetBuffer());
	PrintT(_T("TrimLeft"), TDest11.GetBuffer());
	PrintT(_T("TrimLeft"), TDest12.GetBuffer());
	PrintT(_T("TrimLeft"), TDest13.GetBuffer());
	PrintT(_T("TrimRight"), TDest14.GetBuffer());
	PrintT(_T("TrimRight"), TDest15.GetBuffer());
	PrintT(_T("TrimRight"), TDest16.GetBuffer());

	_tprintf_s(_T("%c\r\n"), 1);
	_tprintf_s(_T("%c\r\n"), 127);
#endif
}

void TestString()
{
#ifdef _STRING_
	TCHAR tszFullPath[FULLPATH_STRLEN];
	TCHAR tszUrl[REQUEST_URL_STRLEN];
	TCHAR tszBuffer[MAX_BUFFER_SIZE];

	_tcsncpy_s(tszFullPath, FULLPATH_STRLEN, _T("C:\\ABC\\DDD\\FFF\\aaa.zip"), _TRUNCATE);
	_tcsncpy_s(tszUrl, REQUEST_URL_STRLEN, _T("https://gundam.netmarble.net:8080/community/bbs/view.asp?menu=5_1_1&tbName=Free&idx=1382167"), _TRUNCATE);
	_tcsncpy_s(tszBuffer, MAX_BUFFER_SIZE, _T(".jpg;.png;.bmp;.gif;.doc;.ppt"), _TRUNCATE);

	_tstring folderPath = FolderPathPassing(tszFullPath);
	_tstring fileNameExt = FileNameExtPathPassing(tszFullPath);

	_tstring fileName, fileExt;
	FileNameExtPassing(fileNameExt, fileName, fileExt);

	_tstring protocol, hostName, request;
	int nPort;
	ParseURL(tszUrl, protocol, hostName, request, nPort);

	_tstring hostNamePort = HostNamePassing(tszUrl);
	_tstring urlFullPath = UrlFullPathPassing(tszUrl);
	_tstring queryString = QueryStringPassing(tszUrl);

	size_t nTokenCount = TokenCount(tszBuffer, _T(";"));
	if( nTokenCount > 0 )
	{
		std::vector<_tstring> dests;
		Tokenize(dests, tszBuffer, _T(";"));
	}

	PrintT(_T("- [FullPath]"), tszFullPath);
	PrintT(_T("- [FolderPath]"), folderPath.c_str());
	PrintT(_T("- [FileNameExt]"), fileNameExt.c_str());
	PrintT(_T("- [FileName]"), fileName.c_str());
	PrintT(_T("- [FileExt]"), fileExt.c_str());
	_tcout << _T("\r\n");

	PrintT(_T("- [URL]"), tszUrl);
	PrintT(_T("- [Protocol]"), protocol.c_str());
	PrintT(_T("- [HostName]"), hostName.c_str());
	PrintT(_T("- [Request]"), request.c_str());
	_tcout << _T("\r\n");

	PrintT(_T("- [URL]"), tszUrl);
	PrintT(_T("- [HostNamePort]"), hostNamePort.c_str());
	PrintT(_T("- [UrlFullPath]"), urlFullPath.c_str());
	PrintT(_T("- [QueryString]"), queryString.c_str());
#endif
}