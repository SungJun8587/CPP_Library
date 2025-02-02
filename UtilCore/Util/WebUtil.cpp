
//***************************************************************************
// WebUtil.cpp: implementation of the WebUtil Function.
//
//***************************************************************************

#include "pch.h"
#include "WebUtil.h"

//***************************************************************************
static const char g_pcDigits[16] = {
	'0', '1' , '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F'
};

//***************************************************************************
static const char g_pcMimeBase64[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

//***************************************************************************
static int g_pnDecodeMimeBase64[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

#ifdef	__MEMBUFFER_H__
//***************************************************************************
//
bool Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int iLength)
{
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	int		nSize = 0;
	BYTE*	pbSourceDoc = nullptr;
	BYTE*	pbDestDoc = nullptr;

	if( pbSource == nullptr || iLength == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	nSize = (4 * (iLength / 3)) + (iLength % 3 ? 4 : 0) + 1;

	ByteDestination.Init(nSize);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( int i = 0; i < iLength; i = i + 3 )
	{
		c1 = pbSourceDoc[i];
		c2 = pbSourceDoc[i + 1];
		c3 = pbSourceDoc[i + 2];

		e1 = (c1 & 0xFC) >> 2;
		e2 = ((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4);
		e3 = ((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6);
		e4 = c3 & 0x3F;

		*pbDestDoc = g_pcMimeBase64[e1];
		*(pbDestDoc + 1) = g_pcMimeBase64[e2];
		*(pbDestDoc + 2) = g_pcMimeBase64[e3];
		*(pbDestDoc + 3) = g_pcMimeBase64[e4];

		if( (i + 2) > iLength ) *(pbDestDoc + 2) = '=';
		if( (i + 3) > iLength ) *(pbDestDoc + 3) = '=';

		pbDestDoc = pbDestDoc + 4;
	}
	*pbDestDoc = 0;

	return true;
}

//***************************************************************************
//
bool Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int iLength)
{
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	int		nSize = 0;
	BYTE* pbSourceDoc = nullptr;
	BYTE* pbDestDoc = nullptr;

	if( pbSource == nullptr || iLength == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;

	nSize = (iLength / 4) * 3 + (iLength % 4 ? 3 : 0) + 1;

	ByteDestination.Init(nSize);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( int i = 0; i < iLength; i = i + 4 )
	{
		c1 = pbSourceDoc[i];
		c2 = pbSourceDoc[i + 1];
		c3 = pbSourceDoc[i + 2];
		c4 = pbSourceDoc[i + 3];

		e1 = g_pnDecodeMimeBase64[c1];
		e2 = g_pnDecodeMimeBase64[c2];

		if( c3 == '=' )
			e3 = 0;
		else e3 = g_pnDecodeMimeBase64[c3];

		if( c4 == '=' )
			e4 = 0;
		else e4 = g_pnDecodeMimeBase64[c4];

		*pbDestDoc = (e1 << 2) | ((e2 & 0x30) >> 4);
		*(pbDestDoc + 1) = ((e2 & 0xf) << 4) | ((e3 & 0x3c) >> 2);
		*(pbDestDoc + 2) = ((e3 & 0x3) << 6) | e4;

		pbDestDoc = pbDestDoc + 3;
	}
	*pbDestDoc = 0;

	return true;
}

//***************************************************************************
//
bool Base64Enc(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nLength = 0;
	int		nSize = 0;
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	char* pszSourceData = nullptr;
	char* pszSourceDoc = nullptr;
	TCHAR* ptszDestDoc = nullptr;

	CMemBuffer<char>	SrcBuffer;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

#ifdef _UNICODE
	if( UnicodeToAnsi(SrcBuffer, ptszSource, wcslen(ptszSource) + 1) != 0 ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
	nLength = (int)SrcBuffer.GetBufSize();
#else
	pszSourceData = (char*)ptszSource;
	nLength = (int)strlen(ptszSource);
#endif

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	nSize = (4 * (nLength / 3)) + (nLength % 3 ? 4 : 0) + 1;

	TDestination.Init(nSize);

	pszSourceDoc = pszSourceData;
	ptszDestDoc = TDestination.GetBuffer();
	for( int i = 0; i < nLength; i = i + 3 )
	{
		c1 = pszSourceDoc[i];
		c2 = pszSourceDoc[i + 1];
		c3 = pszSourceDoc[i + 2];

		e1 = (c1 & 0xFC) >> 2;
		e2 = ((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4);
		e3 = ((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6);
		e4 = c3 & 0x3F;

		*ptszDestDoc = g_pcMimeBase64[e1];
		*(ptszDestDoc + 1) = g_pcMimeBase64[e2];
		*(ptszDestDoc + 2) = g_pcMimeBase64[e3];
		*(ptszDestDoc + 3) = g_pcMimeBase64[e4];

		if( (i + 2) > nLength ) *(ptszDestDoc + 2) = '=';
		if( (i + 3) > nLength ) *(ptszDestDoc + 3) = '=';

		ptszDestDoc = ptszDestDoc + 4;
	}
	*ptszDestDoc = '\0';

	return true;
}

//***************************************************************************
//
bool Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nSize = 0;
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	TCHAR* ptszSourceDoc = nullptr;
	char* pszDestination = nullptr;
	char* pszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	nSize = (iLength / 4) * 3 + 1;

	pszDestination = new char[nSize];

	ptszSourceDoc = (TCHAR*)ptszSource;
	pszDestDoc = pszDestination;
	for( int i = 0; i < iLength; i = i + 4 )
	{
		c1 = ptszSourceDoc[i];
		c2 = ptszSourceDoc[i + 1];
		c3 = ptszSourceDoc[i + 2];
		c4 = ptszSourceDoc[i + 3];

		e1 = g_pnDecodeMimeBase64[c1];
		e2 = g_pnDecodeMimeBase64[c2];

		if( c3 == '=' )
			e3 = 0;
		else e3 = g_pnDecodeMimeBase64[c3];

		if( c4 == '=' )
			e4 = 0;
		else e4 = g_pnDecodeMimeBase64[c4];

		*pszDestDoc = (e1 << 2) | ((e2 & 0x30) >> 4);
		*(pszDestDoc + 1) = ((e2 & 0xf) << 4) | ((e3 & 0x3c) >> 2);
		*(pszDestDoc + 2) = ((e3 & 0x3) << 6) | e4;

		pszDestDoc = pszDestDoc + 3;
	}
	*pszDestDoc = '\0';

	bResult = true;

#ifdef _UNICODE
	if( AnsiToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0) bResult = false;
#else
	TDestination.Init(strlen(pszDestination) + 1);
	strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
#endif

	if( pszDestination )
	{
		delete [] pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}

//***************************************************************************
// asp, c#, php 처리방법 차이남
// asp : 영문자, 숫자를 제외한 문자를 16진수로 변환
// c# : 영문자, 숫자, '!', '\'', '(', ')', '*', '-', '.', '_' 제외한 문자를 16진수로 변환
// php : 빈칸은 +로 변환하고, 영문자, 숫자, '-', '.', '_' 제외한 문자를 16진수로 변환.
// 아스키 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 다르게 작동
bool UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int iCodePage)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char*	pszSourceData = nullptr;
	char*	pszSourceDoc = nullptr;
	char*	pszDestDoc = nullptr;
	char	szExcept [] = "";
	//char	szExcept[] = "!'()*-._";	

	CMemBuffer<char>	SrcBuffer;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;

	if( iCodePage == CP_ACP )
	{
		if( UnicodeToAnsi(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( UnicodeToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
#else
	if( iCodePage == CP_ACP )
	{
		pszSourceData = (char*)ptszSource;
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( AnsiToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
#endif

	pszSourceDoc = pszSourceData;
	if( nullptr != pszSourceDoc )
	{ 
		while( *pszSourceDoc )
		{
			cChar = (unsigned char)*pszSourceDoc;
			if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar)) )
				nCount += 2;

			pszSourceDoc++;
			nCount++;
		}
	}

#ifdef _UNICODE
	DestBuffer.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = TDestination.GetBuffer();
#endif

	if( nullptr != pszSourceDoc )
	{
		while( *pszSourceDoc )
		{
			cChar = (unsigned char)*pszSourceDoc;
			if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar) )
				*pszDestDoc++ = cChar;
			else if( cChar == ' ' )
				*pszDestDoc++ = '+';
			else
			{
				*pszDestDoc++ = '%';
				*pszDestDoc++ = g_pcDigits[(cChar >> 4) & 0x0F];
				*pszDestDoc++ = g_pcDigits[cChar & 0x0F];
			}
			pszSourceDoc++;
		}
		*pszDestDoc = '\0';
	}
#ifdef _UNICODE
	if( AnsiToUnicode(TDestination, DestBuffer.GetBuffer(), DestBuffer.GetBufLength()) != 0 ) return false;
#endif

	return true;
}

//***************************************************************************
//
bool UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int iCodePage)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR*	ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	CMemBuffer<char>	DestBuffer;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		nCount++;
	}

	pszDestination = new char[nCount + 1];

	ptszSourceDoc = (TCHAR*)ptszSource;
	pszDestDoc = pszDestination;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			nNum = 0;
			nRetval = 0;
			for( int i = 0; i < 2; i++ )
			{
				ptszSourceDoc++;
				if( *ptszSourceDoc < ':' )
					nNum = *ptszSourceDoc - 48;
				else if( *ptszSourceDoc > '@' && *ptszSourceDoc < '[' )
					nNum = (*ptszSourceDoc - 'A') + 10;
				else
					nNum = (*ptszSourceDoc - 'a') + 10;

				if( i == 0 )
					nNum = nNum * 16;

				nRetval += nNum;
			}

			*pszDestDoc++ = nRetval;
		}
		else if( *ptszSourceDoc == '+' )
			*pszDestDoc++ = ' ';
		else
			*pszDestDoc++ = (char)*ptszSourceDoc;

		ptszSourceDoc++;
	}
	*pszDestDoc = '\0';

	bResult = true;

#ifdef _UNICODE
	if( iCodePage == CP_ACP )
	{
		if( AnsiToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( Utf8ToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
	}
#else
	if( iCodePage == CP_ACP )
	{
		TDestination.Init(strlen(pszDestination) + 1);
		strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( Utf8ToAnsi(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
	}
#endif

	if( pszDestination )
	{
		delete []pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}

//***************************************************************************
// URL의 쿼리스트링('?'문자 뒤)를 제외한 문자열에 대해서 변환
// 공백문자(32)를 뺀 아스키 문자(0 ~ 127)를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool UrlPathEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	unsigned char cChar = '\0';
	int		nCount = 0;
	char*	pszSourceData = nullptr;
	char*	pszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	CMemBuffer<char>	SrcBuffer;
	CMemBuffer<TCHAR>	QueryString;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	if( _tcschr(ptszSource, URL_QUERYSTRING_CHAR_TOKEN) )
	{
		TCHAR* ptszHostUrlFullPath = nullptr;

		if( !QueryStringPassing(QueryString, ptszSource) ) return false;

		ptszHostUrlFullPath = new TCHAR[_tcslen(ptszSource) - QueryString.GetBufSize() + 1];

		_tcscpy_s(ptszHostUrlFullPath, _tcslen(ptszSource) - QueryString.GetBufSize(), ptszSource);

		bResult = true;

#ifdef _UNICODE
		if( UnicodeToUtf8(SrcBuffer, ptszHostUrlFullPath, wcslen(ptszHostUrlFullPath) + 1) != 0 ) bResult = false;

		pszSourceData = SrcBuffer.GetBuffer();
#else
		if( AnsiToUtf8(SrcBuffer, ptszHostUrlFullPath, strlen(ptszHostUrlFullPath) + 1) != 0 ) bResult = false;

		pszSourceData = SrcBuffer.GetBuffer();
#endif

		if( ptszHostUrlFullPath )
		{
			delete [] ptszHostUrlFullPath;
			ptszHostUrlFullPath = nullptr;
		}

		if( !bResult ) return bResult;
	}
	else
	{
#ifdef _UNICODE
		if( UnicodeToUtf8(SrcBuffer, ptszSource, wcslen(ptszSource) + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
#else
		if( AnsiToUtf8(SrcBuffer, ptszSource, strlen(ptszSource) + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
#endif
	}

	pszSourceDoc = pszSourceData;
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > -1 && cChar < 32) || (cChar > 32 && cChar < 128)) )
			nCount += 2;

		pszSourceDoc++;
		nCount++;
	}

	pszDestination = new char[nCount + 1];

	pszSourceDoc = pszSourceData;
	pszDestDoc = pszDestination;
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( (cChar > -1 && cChar < 32) || (cChar > 32 && cChar < 128) )
			*pszDestDoc++ = cChar;
		else
		{
			*pszDestDoc++ = '%';
			*pszDestDoc++ = g_pcDigits[(cChar >> 4) & 0x0F];
			*pszDestDoc++ = g_pcDigits[cChar & 0x0F];
		}
		pszSourceDoc++;
	}
	*pszDestDoc = '\0';

	bResult = true;

#ifdef _UNICODE
	CMemBuffer<TCHAR>	DestBuffer;

	if( AnsiToUnicode(DestBuffer, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;

	TDestination.Init(DestBuffer.GetBufSize() + QueryString.GetBufSize() + _tcslen(_T("?")) + 1);

	wcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), DestBuffer.GetBuffer(), _TRUNCATE);
	if( QueryString.GetBufSize() > 0 )
	{
		wcscat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), _T("?"));
		wcscat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), QueryString.GetBuffer());
	}
#else
	TDestination.Init(_tcslen(pszDestination) + QueryString.GetBufSize() + _tcslen(_T("?")) + 1);

	strncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination, _TRUNCATE);
	if( QueryString.GetBufSize() > 0 )
	{
		strcat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), _T("?"));
		strcat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), QueryString.GetBuffer());
	}
#endif

	if( pszDestination )
	{
		delete []pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}

//***************************************************************************
// '<' -> '&lt;', '>' -> '&gt;', '&' -> '&amp;', '"' -> '&quot;'로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	TCHAR*	ptszSourceDoc = nullptr;
	TCHAR*	ptszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '<' || *ptszSourceDoc == '>' )
			nCount = nCount + 3;
		else if( *ptszSourceDoc == '&' )
			nCount = nCount + 4;
		else if( *ptszSourceDoc == '"' )
			nCount = nCount + 5;

		ptszSourceDoc++;
		nCount++;
	}

	TDestination.Init(nCount + 1);

	ptszSourceDoc = (TCHAR*)ptszSource;
	ptszDestDoc = TDestination.GetBuffer();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '<' )
		{
			*ptszDestDoc++ = '&';
			*ptszDestDoc++ = 'l';
			*ptszDestDoc++ = 't';
			*ptszDestDoc++ = ';';
		}
		else if( *ptszSourceDoc == '>' )
		{
			*ptszDestDoc++ = '&';
			*ptszDestDoc++ = 'g';
			*ptszDestDoc++ = 't';
			*ptszDestDoc++ = ';';
		}
		else if( *ptszSourceDoc == '&' )
		{
			*ptszDestDoc++ = '&';
			*ptszDestDoc++ = 'a';
			*ptszDestDoc++ = 'm';
			*ptszDestDoc++ = 'p';
			*ptszDestDoc++ = ';';
		}
		else if( *ptszSourceDoc == '"' )
		{
			*ptszDestDoc++ = '&';
			*ptszDestDoc++ = 'q';
			*ptszDestDoc++ = 'u';
			*ptszDestDoc++ = 'o';
			*ptszDestDoc++ = 't';
			*ptszDestDoc++ = ';';
		}
		else *ptszDestDoc++ = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	*ptszDestDoc = '\0';

	return true;
}

//***************************************************************************
//
bool HtmlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	TCHAR* ptszSourceDoc = nullptr;
	TCHAR* ptszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'l' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
			ptszSourceDoc = ptszSourceDoc + 3;
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'g' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
			ptszSourceDoc = ptszSourceDoc + 3;
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'a' && *(ptszSourceDoc + 2) == 'm' && *(ptszSourceDoc + 3) == 'p' && *(ptszSourceDoc + 4) == ';' )
			ptszSourceDoc = ptszSourceDoc + 4;
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'q' && *(ptszSourceDoc + 2) == 'u' && *(ptszSourceDoc + 3) == 'o' && *(ptszSourceDoc + 4) == 't' && *(ptszSourceDoc + 5) == ';' )
			ptszSourceDoc = ptszSourceDoc + 5;

		ptszSourceDoc++;
		nCount++;
	}

	TDestination.Init(nCount + 1);

	ptszSourceDoc = (TCHAR*)ptszSource;
	ptszDestDoc = TDestination.GetBuffer();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'l' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
		{
			*ptszDestDoc++ = '<';
			ptszSourceDoc = ptszSourceDoc + 3;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'g' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
		{
			*ptszDestDoc++ = '>';
			ptszSourceDoc = ptszSourceDoc + 3;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'a' && *(ptszSourceDoc + 2) == 'm' && *(ptszSourceDoc + 3) == 'p' && *(ptszSourceDoc + 4) == ';' )
		{
			*ptszDestDoc++ = '&';
			ptszSourceDoc = ptszSourceDoc + 4;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'q' && *(ptszSourceDoc + 2) == 'u' && *(ptszSourceDoc + 3) == 'o' && *(ptszSourceDoc + 4) == 't' && *(ptszSourceDoc + 5) == ';' )
		{
			*ptszDestDoc++ = '"';
			ptszSourceDoc = ptszSourceDoc + 5;
		}
		else *ptszDestDoc++ = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	*ptszDestDoc = '\0';

	return true;
}

//***************************************************************************
// 영문자, 숫자, '*', '@', '-', '_', '+', '.', '/' 제외한 문자를 16진수로 변환
// 유니코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool Escape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int			nCount = 0;
	wchar_t		wcChar = '\0';
	wchar_t*	pwszSource = nullptr;
	wchar_t*	pwszSourceDoc = nullptr;
	TCHAR*		ptszDestDoc = nullptr;
	wchar_t		wszExcept [] = L"*@-_+./";

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

#ifdef _UNICODE
	pwszSource = (TCHAR*)ptszSource;
#else
	CMemBuffer<wchar_t>	WSrcBuffer;

	if( AnsiToUnicode(WSrcBuffer, ptszSource, strlen(ptszSource) + 1) != 0 ) return false;

	pwszSource = WSrcBuffer.GetBuffer();
#endif

	pwszSourceDoc = pwszSource;
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
			nCount = nCount + 6;
		else if( !((wcChar > 47 && wcChar < 57) || (wcChar > 64 && wcChar < 91) || (wcChar > 96 && wcChar < 123) || wcschr(wszExcept, wcChar)) )
		{
			if( wcChar <= 0xf )
				nCount++;

			nCount = nCount + 3;
		}
		else
			nCount++;

		pwszSourceDoc++;
	}

	TDestination.Init(nCount + 1);

	pwszSourceDoc = pwszSource;
	ptszDestDoc = TDestination.GetBuffer();
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
		{
			*ptszDestDoc++ = '%';
			*ptszDestDoc++ = 'u';

			*ptszDestDoc++ = g_pcDigits[(wcChar >> 12) & 0x0F];
			*ptszDestDoc++ = g_pcDigits[(wcChar >> 8) & 0x0F];
			*ptszDestDoc++ = g_pcDigits[(wcChar >> 4) & 0x0F];
			*ptszDestDoc++ = g_pcDigits[wcChar & 0x0F];
		}
		else if( !((wcChar > 47 && wcChar < 57) || (wcChar > 64 && wcChar < 91) || (wcChar > 96 && wcChar < 123) || wcschr(wszExcept, wcChar)) )
		{
			*ptszDestDoc++ = '%';

			*ptszDestDoc++ = g_pcDigits[(wcChar >> 4) & 0x0F];
			*ptszDestDoc++ = g_pcDigits[wcChar & 0x0F];
		}
		else
			*ptszDestDoc++ = (TCHAR)wcChar;

		pwszSourceDoc++;
	}
	*ptszDestDoc = '\0';

	return true;
}

//***************************************************************************
//
bool UnEscape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int			nCount = 0;
	int			nNum = 0;
	int			nRetval = 0;
	TCHAR*		ptszSourceDoc = nullptr;
	wchar_t*	pwszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			if( *(ptszSourceDoc + 1) == 'u' )
				ptszSourceDoc = ptszSourceDoc + 5;
			else ptszSourceDoc = ptszSourceDoc + 2;
		}

		ptszSourceDoc++;
		nCount++;
	}

	ptszSourceDoc = (TCHAR*)ptszSource;

#ifdef _UNICODE
	TDestination.Init(nCount + 1);

	pwszDestDoc = TDestination.GetBuffer();
#else
	CMemBuffer<wchar_t>	WDestBuffer;

	WDestBuffer.Init(nCount + 1);

	pwszDestDoc = WDestBuffer.GetBuffer();
#endif

	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			if( *(ptszSourceDoc + 1) == 'u' )
			{
				ptszSourceDoc++;

				nNum = 0;
				nRetval = 0;
				for( int i = 0; i < 4; i++ )
				{
					ptszSourceDoc++;
					if( *ptszSourceDoc < ':' )
						nNum = *ptszSourceDoc - 48;
					else if( *ptszSourceDoc > '@' && *ptszSourceDoc < '[' )
						nNum = (*ptszSourceDoc - 'A') + 10;
					else
						nNum = (*ptszSourceDoc - 'a') + 10;

					if( i == 0 )
						nNum = nNum * 16 * 16 * 16;
					else if( i == 1 )
						nNum = nNum * 16 * 16;
					else if( i == 2 )
						nNum = nNum * 16;

					nRetval += nNum;
				}

				*pwszDestDoc++ = nRetval;
			}
			else
			{
				nNum = 0;
				nRetval = 0;
				for( int i = 0; i < 2; i++ )
				{
					ptszSourceDoc++;
					if( *ptszSourceDoc < ':' )
						nNum = *ptszSourceDoc - 48;
					else if( *ptszSourceDoc > '@' && *ptszSourceDoc < '[' )
						nNum = (*ptszSourceDoc - 'A') + 10;
					else
						nNum = (*ptszSourceDoc - 'a') + 10;

					if( i == 0 )
						nNum = nNum * 16;

					nRetval += nNum;
				}

				*pwszDestDoc++ = nRetval;
			}
		}
		else
			*pwszDestDoc++ = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	*pwszDestDoc = '\0';

#ifndef _UNICODE
	if( UnicodeToAnsi(TDestination, WDestBuffer.GetBuffer(), WDestBuffer.GetBufLength()) != 0 ) return false;
#endif

	return true;
}

//***************************************************************************
// 영문자, 숫자, ',', '/', '?', ':', '@', '&', '=', '+', '$', '#' 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool EncodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char*	pszSourceData = nullptr;
	char*	pszSourceDoc = nullptr;
	char*	pszDestDoc = nullptr;
	char	szExcept [] = ",/?:@&=+$#";

	CMemBuffer<char>	SrcBuffer;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#else
	if( AnsiToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#endif

	pszSourceDoc = pszSourceData;
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar)) )
			nCount += 2;

		pszSourceDoc++;
		nCount++;
	}

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;

	DestBuffer.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = TDestination.GetBuffer();
#endif

	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar) )
			*pszDestDoc++ = cChar;
		else if( cChar == ' ' )
			*pszDestDoc++ = '+';
		else
		{
			*pszDestDoc++ = '%';
			*pszDestDoc++ = g_pcDigits[(cChar >> 4) & 0x0F];
			*pszDestDoc++ = g_pcDigits[cChar & 0x0F];
		}
		pszSourceDoc++;
	}
	*pszDestDoc = '\0';

#ifdef _UNICODE
	if( AnsiToUnicode(TDestination, DestBuffer.GetBuffer(), DestBuffer.GetBufLength()) != 0 ) return false;
#endif

	return true;
}

//***************************************************************************
//
bool DecodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR*	ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		nCount++;
	}

	pszDestination = new char[nCount + 1];

	ptszSourceDoc = (TCHAR*)ptszSource;
	pszDestDoc = pszDestination;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			nNum = 0;
			nRetval = 0;
			for( int i = 0; i < 2; i++ )
			{
				ptszSourceDoc++;
				if( *ptszSourceDoc < ':' )
					nNum = *ptszSourceDoc - 48;
				else if( *ptszSourceDoc > '@' && *ptszSourceDoc < '[' )
					nNum = (*ptszSourceDoc - 'A') + 10;
				else
					nNum = (*ptszSourceDoc - 'a') + 10;

				if( i == 0 )
					nNum = nNum * 16;

				nRetval += nNum;
			}

			*pszDestDoc++ = nRetval;
		}
		else if( *ptszSourceDoc == '+' )
			*pszDestDoc++ = ' ';
		else
			*pszDestDoc++ = (char)*ptszSourceDoc;

		ptszSourceDoc++;
	}
	*pszDestDoc = '\0';

	bResult = true;

#ifdef _UNICODE
	if( Utf8ToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#else
	if( Utf8ToAnsi(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#endif

	if( pszDestination )
	{
		delete []pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}

//***************************************************************************
// 영문자, 숫자를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool EncodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char* pszSourceData = nullptr;
	char* pszSourceDoc = nullptr;
	char* pszDestDoc = nullptr;

	CMemBuffer<char>	SrcBuffer;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#else
	if( AnsiToUtf8(SrcBuffer, ptszSource, iLength + 1) != 0 ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#endif

	pszSourceDoc = pszSourceData;
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123)) )
			nCount += 2;

		pszSourceDoc++;
		nCount++;
	}

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;

	DestBuffer.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(nCount + 1);

	pszSourceDoc = pszSourceData;
	pszDestDoc = TDestination.GetBuffer();
#endif

	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) )
			*pszDestDoc++ = cChar;
		else if( cChar == ' ' )
			*pszDestDoc++ = '+';
		else
		{
			*pszDestDoc++ = '%';
			*pszDestDoc++ = g_pcDigits[(cChar >> 4) & 0x0F];
			*pszDestDoc++ = g_pcDigits[cChar & 0x0F];
		}
		pszSourceDoc++;
	}
	*pszDestDoc = '\0';

#ifdef _UNICODE
	if( AnsiToUnicode(TDestination, DestBuffer.GetBuffer(), DestBuffer.GetBufLength()) != 0 ) return false;
#endif

	return true;
}

//***************************************************************************
//
bool DecodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR* ptszSourceDoc = nullptr;
	char* pszDestination = nullptr;
	char* pszDestDoc = nullptr;

	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return false;

	ptszSourceDoc = (TCHAR*)ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		nCount++;
	}

	pszDestination = new char[nCount + 1];

	ptszSourceDoc = (TCHAR*)ptszSource;
	pszDestDoc = pszDestination;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			nNum = 0;
			nRetval = 0;
			for( int i = 0; i < 2; i++ )
			{
				ptszSourceDoc++;
				if( *ptszSourceDoc < ':' )
					nNum = *ptszSourceDoc - 48;
				else if( *ptszSourceDoc > '@' && *ptszSourceDoc < '[' )
					nNum = (*ptszSourceDoc - 'A') + 10;
				else
					nNum = (*ptszSourceDoc - 'a') + 10;

				if( i == 0 )
					nNum = nNum * 16;

				nRetval += nNum;
			}

			*pszDestDoc++ = nRetval;
		}
		else if( *ptszSourceDoc == '+' )
			*pszDestDoc++ = ' ';
		else
			*pszDestDoc++ = (char)*ptszSourceDoc;

		ptszSourceDoc++;
	}
	*pszDestDoc = '\0';

	bResult = true;

#ifdef _UNICODE
	if( Utf8ToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#else
	if( Utf8ToAnsi(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#endif

	if( pszDestination )
	{
		delete []pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}
//#else
//***************************************************************************
//
_tstring Base64Enc(const _tstring& source)
{
	size_t	length = 0;
	size_t	size = 0;
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	char*	pszSourceData = nullptr;
	char*	pszSourceDoc = nullptr;
	TCHAR*	ptszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

#ifdef _UNICODE
	string sourceData = WStringToString(source);
	pszSourceData = const_cast<char*>(sourceData.c_str());
	length = sourceData.size();
#else
	pszSourceData = const_cast<char*>(source.c_str());
	length = source.size();
#endif

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (4 * (length / 3)) + (length % 3 ? 4 : 0);

	_tstring dest(size, _T('\0'));

	pszSourceDoc = pszSourceData;
	ptszDestDoc = const_cast<TCHAR*>(dest.c_str());
	for( int i = 0; i < length; i = i + 3 )
	{
		c1 = pszSourceDoc[i];
		c2 = pszSourceDoc[i + 1];
		c3 = pszSourceDoc[i + 2];

		e1 = (c1 & 0xFC) >> 2;
		e2 = ((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4);
		e3 = ((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6);
		e4 = c3 & 0x3F;

		*ptszDestDoc = g_pcMimeBase64[e1];
		*(ptszDestDoc + 1) = g_pcMimeBase64[e2];
		*(ptszDestDoc + 2) = g_pcMimeBase64[e3];
		*(ptszDestDoc + 3) = g_pcMimeBase64[e4];

		if( (i + 2) > length ) *(ptszDestDoc + 2) = '=';
		if( (i + 3) > length ) *(ptszDestDoc + 3) = '=';

		ptszDestDoc = ptszDestDoc + 4;
	}
	*ptszDestDoc = '\0';

	return dest;
}

//***************************************************************************
//
_tstring Base64Dec(const _tstring& source)
{
	size_t	length = 0;
	size_t	size = 0;
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	TCHAR*	ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	_tstring dest;

	if( source.c_str() == nullptr || source.size() == 0 ) return dest;

	length = source.size();
	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (length / 4) * 3 + 1;

	pszDestination = new char[size];

	ptszSourceDoc = const_cast<TCHAR*>(source.c_str());
	pszDestDoc = pszDestination;
	for( int i = 0; i < length; i = i + 4 )
	{
		c1 = ptszSourceDoc[i];
		c2 = ptszSourceDoc[i + 1];
		c3 = ptszSourceDoc[i + 2];
		c4 = ptszSourceDoc[i + 3];

		e1 = g_pnDecodeMimeBase64[c1];
		e2 = g_pnDecodeMimeBase64[c2];

		if( c3 == '=' )
			e3 = 0;
		else e3 = g_pnDecodeMimeBase64[c3];

		if( c4 == '=' )
			e4 = 0;
		else e4 = g_pnDecodeMimeBase64[c4];

		*pszDestDoc = (e1 << 2) | ((e2 & 0x30) >> 4);
		*(pszDestDoc + 1) = ((e2 & 0xf) << 4) | ((e3 & 0x3c) >> 2);
		*(pszDestDoc + 2) = ((e3 & 0x3) << 6) | e4;

		pszDestDoc = pszDestDoc + 3;
	}
	*pszDestDoc = '\0';

#ifdef _UNICODE
	dest = StringToWString(pszDestination);
#else
	dest.assign(pszDestination);
#endif

	if( pszDestination )
	{
		delete[] pszDestination;
		pszDestination = nullptr;
	}

	return dest;
}
#endif
