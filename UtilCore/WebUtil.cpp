
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

//***************************************************************************
//
BOOL Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int nLength)
{
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	int		nSize = 0;
	BYTE* pbSourceDoc = NULL;
	BYTE* pbDestDoc = NULL;

	if( !pbSource ) return false;
	if( nLength < 1 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	nSize = (4 * (nLength / 3)) + (nLength % 3 ? 4 : 0) + 1;

	ByteDestination.Init(nSize);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( int i = 0; i < nLength; i = i + 3 )
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

		if( (i + 2) > nLength ) *(pbDestDoc + 2) = '=';
		if( (i + 3) > nLength ) *(pbDestDoc + 3) = '=';

		pbDestDoc = pbDestDoc + 4;
	}
	*pbDestDoc = 0;

	return true;
}

//***************************************************************************
//
BOOL Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int nLength)
{
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	int		nSize = 0;
	BYTE* pbSourceDoc = NULL;
	BYTE* pbDestDoc = NULL;

	if( !pbSource ) return false;
	if( nLength < 1 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;

	nSize = (nLength / 4) * 3 + (nLength % 4 ? 3 : 0) + 1;

	ByteDestination.Init(nSize);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( int i = 0; i < nLength; i = i + 4 )
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
BOOL Base64Enc(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nLength = 0;
	int		nSize = 0;
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	char* pszSourceData = NULL;
	char* pszSourceDoc = NULL;
	TCHAR* ptszDestDoc = NULL;

	CMemBuffer<char>	SrcBuffer;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

#ifdef _UNICODE
	if( !WideCharToMultiByteStr(SrcBuffer, ptszSource) ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
	nLength = (int)SrcBuffer.GetBufSize();
#else
	pszSourceData = (TCHAR*)ptszSource;
	nLength = _tcslen(ptszSource);
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
BOOL Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nLength = 0;
	int		nSize = 0;
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	TCHAR* ptszSourceDoc = NULL;
	char* pszDestination = NULL;
	char* pszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( (nLength = (int)_tcslen(ptszSource)) < 1 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	nSize = (nLength / 4) * 3 + 1;

	pszDestination = new char[nSize];

	ptszSourceDoc = (TCHAR*)ptszSource;
	pszDestDoc = pszDestination;
	for( int i = 0; i < nLength; i = i + 4 )
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
	if( !MultiByteToWideCharStr(TDestination, pszDestination) ) bResult = false;
#else
	TDestination.Init(strlen(pszDestination) + 1);
	strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = NULL;
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
BOOL UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int nCodePage)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char* pszSourceData = NULL;
	char* pszSourceDoc = NULL;
	char* pszDestDoc = NULL;
	char	szExcept[] = "";
	//char	szExcept[] = "!'()*-._";	

	CMemBuffer<char>	SrcBuffer;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;

	if( nCodePage == CP_ACP )
	{
		if( !WideCharToMultiByteStr(SrcBuffer, ptszSource) ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
	else if( nCodePage == CP_UTF8 )
	{
		if( !UTF8ToUnicode(SrcBuffer, ptszSource) ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
#else
	if( nCodePage == CP_ACP )
	{
		pszSourceData = (char*)ptszSource;
	}
	else if( nCodePage == CP_UTF8 )
	{
		if( !UTF8ToAnsi(SrcBuffer, ptszSource) ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
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
	if( !MultiByteToWideCharStr(TDestination, DestBuffer.GetBuffer()) ) return false;
#endif

	return true;
}

//***************************************************************************
//
BOOL UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int nCodePage)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR* ptszSourceDoc = NULL;
	char* pszDestination = NULL;
	char* pszDestDoc = NULL;

	CMemBuffer<char>	DestBuffer;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
	if( nCodePage == CP_ACP )
	{
		if( !MultiByteToWideCharStr(TDestination, pszDestination) ) bResult = false;
	}
	else if( nCodePage == CP_UTF8 )
	{
		if( !UnicodeToUTF8(TDestination, pszDestination) ) bResult = false;
	}
#else
	if( nCodePage == CP_ACP )
	{
		TDestination.Init(strlen(pszDestination) + 1);
		strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
	}
	else if( nCodePage == CP_UTF8 )
	{
		if( !AnsiToUTF8(TDestination, pszDestination) ) bResult = false;
	}
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = NULL;
	}

	return bResult;
}

//***************************************************************************
// URL의 쿼리스트링('?'문자 뒤)를 제외한 문자열에 대해서 변환
// 공백문자(32)를 뺀 아스키 문자(0 ~ 127)를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
BOOL UrlPathEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	unsigned char cChar = '\0';
	int		nCount = 0;
	char* pszSourceData = NULL;
	char* pszSourceDoc = NULL;
	char* pszDestination = NULL;
	char* pszDestDoc = NULL;

	CMemBuffer<char>	SrcBuffer;
	CMemBuffer<TCHAR>	QueryString;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

	if( _tcschr(ptszSource, URL_QUERYSTRING_CHAR_TOKEN) )
	{
		TCHAR* pszHostUrlFullPath = NULL;

		if( !QueryStringPassing(QueryString, ptszSource) ) return false;

		pszHostUrlFullPath = new TCHAR[_tcslen(ptszSource) - QueryString.GetBufSize() + 1];

		_tcscpy_s(pszHostUrlFullPath, _tcslen(ptszSource) - QueryString.GetBufSize(), ptszSource);

		bResult = true;

#ifdef _UNICODE
		if( !UTF8ToUnicode(SrcBuffer, pszHostUrlFullPath) ) bResult = false;

		pszSourceData = SrcBuffer.GetBuffer();
#else
		if( !UTF8ToAnsi(SrcBuffer, pszHostUrlFullPath) ) bResult = false;

		pszSourceData = SrcBuffer.GetBuffer();
#endif

		if( pszHostUrlFullPath )
		{
			delete[]pszHostUrlFullPath;
			pszHostUrlFullPath = NULL;
		}

		if( !bResult ) return bResult;
	}
	else
	{
#ifdef _UNICODE
		if( !UTF8ToUnicode(SrcBuffer, ptszSource) ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
#else
		if( !UTF8ToAnsi(SrcBuffer, ptszSource) ) return false;

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

	if( !MultiByteToWideCharStr(DestBuffer, pszDestination) ) bResult = false;

	TDestination.Init(DestBuffer.GetBufSize() + QueryString.GetBufSize() + _tcslen(_T("?")) + 1);

	StringCchCopy(TDestination.GetBuffer(), TDestination.GetBufSize(), DestBuffer.GetBuffer());
	if( QueryString.GetBufSize() > 0 )
	{
		StringCchCat(TDestination.GetBuffer(), TDestination.GetBufSize(), _T("?"));
		StringCchCat(TDestination.GetBuffer(), TDestination.GetBufSize(), QueryString.GetBuffer());
	}
#else
	TDestination.Init(_tcslen(pszDestination) + QueryString.GetBufSize() + _tcslen(_T("?")) + 1);

	strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
	if( QueryString.GetBufSize() > 0 )
	{
		strcat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), _T("?"));
		strcat_s(TDestination.GetBuffer(), TDestination.GetBufSize(), QueryString.GetBuffer());
	}
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = NULL;
	}

	return bResult;
}

//***************************************************************************
// '<' -> '&lt;', '>' -> '&gt;', '&' -> '&amp;', '"' -> '&quot;'로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
BOOL HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	TCHAR* ptszSourceDoc = NULL;
	TCHAR* ptszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
BOOL HtmlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	TCHAR* ptszSourceDoc = NULL;
	TCHAR* ptszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
BOOL Escape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	wchar_t wcChar = '\0';
	wchar_t* pwszSource = NULL;
	wchar_t* pwszSourceDoc = NULL;
	TCHAR* ptszDestDoc = NULL;
	wchar_t wszExcept[] = L"*@-_+./";

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

#ifdef _UNICODE
	pwszSource = (TCHAR*)ptszSource;
#else
	CMemBuffer<wchar_t>	WSrcBuffer;

	if( !MultiByteToWideCharStr(WSrcBuffer, ptszSource) ) return false;

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
BOOL UnEscape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR* ptszSourceDoc = NULL;
	wchar_t* pwszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
	if( !WideCharToMultiByteStr(TDestination, WDestBuffer.GetBuffer()) ) return false;
#endif

	return true;
}

//***************************************************************************
// 영문자, 숫자, ',', '/', '?', ':', '@', '&', '=', '+', '$', '#' 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
BOOL EncodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char* pszSourceData = NULL;
	char* pszSourceDoc = NULL;
	char* pszDestDoc = NULL;
	char	szExcept[] = ",/?:@&=+$#";

	CMemBuffer<char>	SrcBuffer;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

#ifdef _UNICODE
	if( !UTF8ToUnicode(SrcBuffer, ptszSource) ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#else
	if( !UTF8ToAnsi(SrcBuffer, ptszSource) ) return false;

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
	if( !MultiByteToWideCharStr(TDestination, DestBuffer.GetBuffer()) ) return false;
#endif

	return true;
}

//***************************************************************************
//
BOOL DecodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR* ptszSourceDoc = NULL;
	char* pszDestination = NULL;
	char* pszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
	if( !UnicodeToUTF8(TDestination, pszDestination) ) bResult = false;
#else
	if( !AnsiToUTF8(TDestination, pszDestination) ) bResult = false;
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = NULL;
	}

	return bResult;
}

//***************************************************************************
// 영문자, 숫자를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
BOOL EncodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	unsigned char cChar = '\0';

	int		nCount = 0;
	char* pszSourceData = NULL;
	char* pszSourceDoc = NULL;
	char* pszDestDoc = NULL;

	CMemBuffer<char>	SrcBuffer;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

#ifdef _UNICODE
	if( !UTF8ToUnicode(SrcBuffer, ptszSource) ) return false;

	pszSourceData = SrcBuffer.GetBuffer();
#else
	if( !UTF8ToAnsi(SrcBuffer, ptszSource) ) return false;

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
	if( !MultiByteToWideCharStr(TDestination, DestBuffer.GetBuffer()) ) return false;
#endif

	return true;
}

//***************************************************************************
//
BOOL DecodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	int		nCount = 0;
	int		nNum = 0;
	int		nRetval = 0;
	TCHAR* ptszSourceDoc = NULL;
	char* pszDestination = NULL;
	char* pszDestDoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 1 ) return false;

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
	if( !UnicodeToUTF8(TDestination, pszDestination) ) bResult = false;
#else
	if( !AnsiToUTF8(TDestination, pszDestination) ) bResult = false;
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = NULL;
	}

	return bResult;
}

