
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
bool Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const size_t length)
{
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	size_t	size = 0;
	BYTE*	pbSourceDoc = nullptr;
	BYTE*	pbDestDoc = nullptr;

	if( pbSource == nullptr || length == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (4 * (length / 3)) + (length % 3 ? 4 : 0) + 1;

	ByteDestination.Init(size);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( size_t i = 0; i < length; i = i + 3 )
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

		if( (i + 2) > length ) *(pbDestDoc + 2) = '=';
		if( (i + 3) > length ) *(pbDestDoc + 3) = '=';

		pbDestDoc = pbDestDoc + 4;
	}
	*pbDestDoc = 0;

	return true;
}

//***************************************************************************
//
bool Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const size_t length)
{
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	size_t	size = 0;
	BYTE*	pbSourceDoc = nullptr;
	BYTE*	pbDestDoc = nullptr;

	if( pbSource == nullptr || length == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;

	size = (length / 4) * 3 + (length % 4 ? 3 : 0) + 1;

	ByteDestination.Init(size);

	pbSourceDoc = (BYTE*)pbSource;
	pbDestDoc = ByteDestination.GetBuffer();
	for( size_t i = 0; i < length; i = i + 4 )
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
	size_t	length = 0;
	size_t	size = 0;
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	const char*	pszSourceDoc = nullptr;
	TCHAR* ptszDestDoc = nullptr;

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	SrcBuffer;

	if( UnicodeToAnsi(SrcBuffer, ptszSource, length + 1) != 0 ) return false;

	pszSourceDoc = SrcBuffer.GetBuffer();
	length = SrcBuffer.GetBufLength();
#else
	pszSourceDoc = ptszSource;
	length = length + 1;
#endif

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (4 * (length / 3)) + (length % 3 ? 4 : 0) + 1;

	TDestination.Init(size);

	ptszDestDoc = TDestination.GetBuffer();
	for( size_t i = 0; i < length; i = i + 3 )
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

	return true;
}

//***************************************************************************
//
bool Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	BOOL	bResult = false;
	size_t	length = 0;
	size_t	size = 0;
	int		c1, c2, c3, c4;
	int		e1, e2, e3, e4;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (length / 4) * 3 + 1;

	pszDestination = new char[size];

	ptszSourceDoc = ptszSource;
	pszDestDoc = pszDestination;
	for( size_t i = 0; i < length; i = i + 4 )
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
	if( AnsiToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#else
	TDestination.Init(strlen(pszDestination) + 1);
	strcpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination);
#endif

	if( pszDestination )
	{
		delete[] pszDestination;
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
bool UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const int iCodePage)
{
	unsigned char cChar = '\0';

	size_t	length = 0;
	size_t	size = 0;
	const char* pszSourceData = nullptr;
	const char* pszSourceDoc = nullptr;
	char*	pszDestDoc = nullptr;
	char	szExcept[] = "";
	//char	szExcept[] = "!'()*-._";	

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

	CMemBuffer<char>	SrcBuffer;

#ifdef _UNICODE
	if( iCodePage == CP_ACP )
	{
		if( UnicodeToAnsi(SrcBuffer, ptszSource, length + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( UnicodeToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;

		pszSourceData = SrcBuffer.GetBuffer();
	}
#else
	if( iCodePage == CP_ACP )
	{
		pszSourceData = ptszSource;
	}
	else if( iCodePage == CP_UTF8 )
	{
		if( AnsiToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;

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
				size += 2;

			pszSourceDoc++;
			size++;
		}
	}

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;
	DestBuffer.Init(size + 1);
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(size + 1);
	pszDestDoc = TDestination.GetBuffer();
#endif

	pszSourceDoc = pszSourceData;
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
bool UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const int iCodePage)
{
	BOOL	bResult = false;
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = ptszSource;
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
		delete[]pszDestination;
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
	size_t	length = 0;
	size_t	size = 0;
	const char*	pszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

	CMemBuffer<char>	SrcBuffer;

#ifdef _UNICODE
	if( UnicodeToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
	pszSourceDoc = SrcBuffer.GetBuffer();
#else
	if( AnsiToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
	pszSourceDoc = SrcBuffer.GetBuffer();
#endif

	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > -1 && cChar < 32) || (cChar > 32 && cChar < 128)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	pszSourceDoc = SrcBuffer.GetBuffer();
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
	if( AnsiToUnicode(TDestination, pszDestination, strlen(pszDestination) + 1) != 0 ) bResult = false;
#else
	TDestination.Init(_tcslen(pszDestination) + 1);
	strncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszDestination, _TRUNCATE);
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}

//***************************************************************************
// '<' -> '&lt;', '>' -> '&gt;', '&' -> '&amp;', '"' -> '&quot;'로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
bool HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	size = 0;
	const TCHAR*	ptszSourceDoc = nullptr;
	TCHAR*	ptszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '<' || *ptszSourceDoc == '>' )
			size = size + 3;
		else if( *ptszSourceDoc == '&' )
			size = size + 4;
		else if( *ptszSourceDoc == '"' )
			size = size + 5;

		ptszSourceDoc++;
		size++;
	}

	TDestination.Init(size + 1);

	ptszSourceDoc = ptszSource;
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
	size_t	size = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	TCHAR*	ptszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
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
		size++;
	}

	TDestination.Init(size + 1);

	ptszSourceDoc = ptszSource;
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
	size_t	length = 0;
	size_t	size = 0;
	wchar_t		wcChar = '\0';
	const wchar_t*	pwszSourceData = nullptr;
	const wchar_t*	pwszSourceDoc = nullptr;
	TCHAR*		ptszDestDoc = nullptr;
	wchar_t		wszExcept[] = L"*@-_+./";

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

#ifdef _UNICODE
	pwszSourceData = ptszSource;
#else
	CMemBuffer<wchar_t>	WSrcBuffer;

	if( AnsiToUnicode(WSrcBuffer, ptszSource, length + 1) != 0 ) return false;

	pwszSourceData = WSrcBuffer.GetBuffer();
#endif

	pwszSourceDoc = pwszSourceData;
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
			size = size + 6;
		else if( !((wcChar > 47 && wcChar < 57) || (wcChar > 64 && wcChar < 91) || (wcChar > 96 && wcChar < 123) || wcschr(wszExcept, wcChar)) )
		{
			if( wcChar <= 0xf )
				size++;

			size = size + 3;
		}
		else
			size++;

		pwszSourceDoc++;
	}

	TDestination.Init(size + 1);

	pwszSourceDoc = pwszSourceData;
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
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR*	ptszSourceDoc = nullptr;
	wchar_t* pwszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			if( *(ptszSourceDoc + 1) == 'u' )
				ptszSourceDoc = ptszSourceDoc + 5;
			else ptszSourceDoc = ptszSourceDoc + 2;
		}

		ptszSourceDoc++;
		size++;
	}

#ifdef _UNICODE
	TDestination.Init(size + 1);
	pwszDestDoc = TDestination.GetBuffer();
#else
	CMemBuffer<wchar_t>	WDestBuffer;
	WDestBuffer.Init(size + 1);
	pwszDestDoc = WDestBuffer.GetBuffer();
#endif

	ptszSourceDoc = ptszSource;
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
	size_t	length = 0;
	size_t	size = 0;
	const char* pszSourceDoc = nullptr;
	char*	pszDestDoc = nullptr;
	char	szExcept[] = ",/?:@&=+$#";

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

	CMemBuffer<char>	SrcBuffer;

#ifdef _UNICODE
	if( UnicodeToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
#else
	if( AnsiToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
#endif

	pszSourceDoc = SrcBuffer.GetBuffer();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;
	DestBuffer.Init(size + 1);
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(size + 1);
	pszDestDoc = TDestination.GetBuffer();
#endif

	pszSourceDoc = SrcBuffer.GetBuffer();
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
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = ptszSource;
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
		delete[]pszDestination;
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

	size_t	length = 0;
	size_t	size = 0;
	const char*	pszSourceDoc = nullptr;
	char*	pszDestDoc = nullptr;

	length = _tcslen(ptszSource);
	if( ptszSource == nullptr || length == 0 ) return false;

	CMemBuffer<char>	SrcBuffer;

#ifdef _UNICODE
	if( UnicodeToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
#else
	if( AnsiToUtf8(SrcBuffer, ptszSource, length + 1) != 0 ) return false;
#endif

	pszSourceDoc = SrcBuffer.GetBuffer();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

#ifdef _UNICODE
	CMemBuffer<char>	DestBuffer;
	DestBuffer.Init(size + 1);
	pszDestDoc = DestBuffer.GetBuffer();
#else
	TDestination.Init(size + 1);
	pszDestDoc = TDestination.GetBuffer();
#endif

	pszSourceDoc = SrcBuffer.GetBuffer();
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
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char* pszDestination = nullptr;
	char* pszDestDoc = nullptr;

	if( ptszSource == nullptr || _tcslen(ptszSource) == 0 ) return false;

	ptszSourceDoc = ptszSource;
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = ptszSource;
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
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return bResult;
}
#endif

#ifdef _STRING_
//***************************************************************************
//
_tstring Base64Enc(const _tstring& source)
{
	size_t	length = 0;
	size_t	size = 0;
	int		c1, c2, c3;
	int		e1, e2, e3, e4;
	const char*	pszSourceDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

#ifdef _UNICODE
	std::string sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "CP949");
	pszSourceDoc = sourceData.c_str();
	length = sourceData.size() + 1;
#else
	pszSourceDoc = source.c_str();
	length = source.size() + 1;
#endif

	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (4 * (length / 3)) + (length % 3 ? 4 : 0);

	int destIndex = 0;
	_tstring dest(size, '\0');

	for( size_t i = 0; i < length; i = i + 3 )
	{
		c1 = pszSourceDoc[i];
		c2 = pszSourceDoc[i + 1];
		c3 = pszSourceDoc[i + 2];

		e1 = (c1 & 0xFC) >> 2;
		e2 = ((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4);
		e3 = ((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6);
		e4 = c3 & 0x3F;

		dest[destIndex] = g_pcMimeBase64[e1];
		dest[destIndex + 1] = g_pcMimeBase64[e2];
		dest[destIndex + 2] = g_pcMimeBase64[e3];
		dest[destIndex + 3] = g_pcMimeBase64[e4];

		if( (i + 2) > length ) dest[destIndex + 2] = '=';
		if( (i + 3) > length ) dest[destIndex + 3] = '=';
		
		destIndex = destIndex + 4;
	}
	dest[destIndex] = '\0';

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
	const TCHAR*	ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	length = source.size();
	c1 = c2 = c3 = 0;
	e1 = e2 = e3 = e4 = 0;
	size = (length / 4) * 3 + 1;

	pszDestination = new char[size];

	ptszSourceDoc = source.c_str();
	pszDestDoc = pszDestination;
	for( size_t i = 0; i < length; i = i + 4 )
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

	_tstring dest;

#ifdef _UNICODE
	dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "CP949", "WCHAR_T");
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

//***************************************************************************
// asp, c#, php 처리방법 차이남
// asp : 영문자, 숫자를 제외한 문자를 16진수로 변환
// c# : 영문자, 숫자, '!', '\'', '(', ')', '*', '-', '.', '_' 제외한 문자를 16진수로 변환
// php : 빈칸은 +로 변환하고, 영문자, 숫자, '-', '.', '_' 제외한 문자를 16진수로 변환.
// 아스키 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 다르게 작동
_tstring UrlEncode(const _tstring& source, const int iCodePage)
{
	unsigned char cChar = '\0';

	size_t	size = 0;
	const char* pszSourceData = nullptr;
	const char* pszSourceDoc = nullptr;
	char		szExcept[] = "";
	//char		szExcept[] = "!'()*-._";	

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	std::string	sourceData;

#ifdef _UNICODE
	if( iCodePage == CP_ACP )
	{
		sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "CP949");
		pszSourceData = sourceData.c_str();
	}
	else if( iCodePage == CP_UTF8 )
	{
		sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "UTF-8");
		pszSourceData = sourceData.c_str();
	}
#else
	if( iCodePage == CP_ACP )
	{
		pszSourceData = source.c_str();
	}
	else if( iCodePage == CP_UTF8 )
	{
		sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "CP949", "UTF-8");
		pszSourceData = sourceData.c_str();
	}
#endif

	pszSourceDoc = pszSourceData;
	if( nullptr != pszSourceDoc )
	{
		while( *pszSourceDoc )
		{
			cChar = (unsigned char)*pszSourceDoc;
			if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar)) )
				size += 2;

			pszSourceDoc++;
			size++;
		}
	}

	int destIndex = 0;
	std::string destData(size + 1, '\0');

	pszSourceDoc = pszSourceData;
	if( nullptr != pszSourceDoc )
	{
		while( *pszSourceDoc )
		{
			cChar = (unsigned char)*pszSourceDoc;
			if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar) )
				destData[destIndex++] = cChar;
			else if( cChar == ' ' )
				destData[destIndex++] = '+';
			else
			{
				destData[destIndex++] = '%';
				destData[destIndex++] = g_pcDigits[(cChar >> 4) & 0x0F];
				destData[destIndex++] = g_pcDigits[cChar & 0x0F];
			}
			pszSourceDoc++;
		}
		destData[destIndex] = '\0';
	}

#ifdef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncodingW(destData, "CP949", "WCHAR_T");
#else
	_tstring dest = destData;
#endif

	return dest;
}

//***************************************************************************
//
_tstring UrlDecode(const _tstring& source, const int iCodePage)
{
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = source.c_str();
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

	_tstring dest;

#ifdef _UNICODE
	if( iCodePage == CP_ACP )
	{
		dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "CP949", "WCHAR_T");
	}
	else if( iCodePage == CP_UTF8 )
	{
		dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "UTF-8", "WCHAR_T");
	}
#else
	if( iCodePage == CP_ACP )
	{
		dest.assign(pszDestination);
	}
	else if( iCodePage == CP_UTF8 )
	{
		dest = Iconv::CIconvUtil::ConvertEncoding(pszDestination, "UTF-8", "CP949");
	}
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return dest;
}

//***************************************************************************
// URL의 쿼리스트링('?'문자 뒤)를 제외한 문자열에 대해서 변환
// 공백문자(32)를 뺀 아스키 문자(0 ~ 127)를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
_tstring UrlPathEncode(const _tstring& source)
{
	unsigned char cChar = '\0';
	size_t	size = 0;
	const char* pszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	std::string	sourceData;

#ifdef _UNICODE
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "UTF-8");
#else
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "CP949", "UTF-8");
#endif

	pszSourceDoc = sourceData.c_str();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > -1 && cChar < 32) || (cChar > 32 && cChar < 128)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	pszSourceDoc = sourceData.c_str();
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

	_tstring dest;

#ifdef _UNICODE
	dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "CP949", "WCHAR_T");
#else
	dest.assign(pszDestination);
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return dest;
}

//***************************************************************************
// '<' -> '&lt;', '>' -> '&gt;', '&' -> '&amp;', '"' -> '&quot;'로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
_tstring HtmlEncode(const _tstring& source)
{
	size_t	size = 0;
	const TCHAR* ptszSourceDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '<' || *ptszSourceDoc == '>' )
			size = size + 3;
		else if( *ptszSourceDoc == '&' )
			size = size + 4;
		else if( *ptszSourceDoc == '"' )
			size = size + 5;

		ptszSourceDoc++;
		size++;
	}

	int destIndex = 0;
	_tstring dest(size + 1, '\0');

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '<' )
		{
			dest[destIndex++] = '&';
			dest[destIndex++] = 'l';
			dest[destIndex++] = 't';
			dest[destIndex++] = ';';
		}
		else if( *ptszSourceDoc == '>' )
		{
			dest[destIndex++] = '&';
			dest[destIndex++] = 'g';
			dest[destIndex++] = 't';
			dest[destIndex++] = ';';
		}
		else if( *ptszSourceDoc == '&' )
		{
			dest[destIndex++] = '&';
			dest[destIndex++] = 'a';
			dest[destIndex++] = 'm';
			dest[destIndex++] = 'p';
			dest[destIndex++] = ';';
		}
		else if( *ptszSourceDoc == '"' )
		{
			dest[destIndex++] = '&';
			dest[destIndex++] = 'q';
			dest[destIndex++] = 'u';
			dest[destIndex++] = 'o';
			dest[destIndex++] = 't';
			dest[destIndex++] = ';';
		}
		else dest[destIndex++] = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	dest[destIndex] = '\0';

	return dest;
}

//***************************************************************************
//
_tstring HtmlDecode(const _tstring& source)
{
	size_t	size = 0;
	const TCHAR* ptszSourceDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
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
		size++;
	}

	int destIndex = 0;
	_tstring dest(size + 1, '\0');

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'l' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
		{
			dest[destIndex++] = '<';
			ptszSourceDoc = ptszSourceDoc + 3;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'g' && *(ptszSourceDoc + 2) == 't' && *(ptszSourceDoc + 3) == ';' )
		{
			dest[destIndex++] = '>';
			ptszSourceDoc = ptszSourceDoc + 3;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'a' && *(ptszSourceDoc + 2) == 'm' && *(ptszSourceDoc + 3) == 'p' && *(ptszSourceDoc + 4) == ';' )
		{
			dest[destIndex++] = '&';
			ptszSourceDoc = ptszSourceDoc + 4;
		}
		else if( *ptszSourceDoc == '&' && *(ptszSourceDoc + 1) == 'q' && *(ptszSourceDoc + 2) == 'u' && *(ptszSourceDoc + 3) == 'o' && *(ptszSourceDoc + 4) == 't' && *(ptszSourceDoc + 5) == ';' )
		{
			dest[destIndex++] = '"';
			ptszSourceDoc = ptszSourceDoc + 5;
		}
		else dest[destIndex++] = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	dest[destIndex] = '\0';

	return dest;
}

//***************************************************************************
// 영문자, 숫자, '*', '@', '-', '_', '+', '.', '/' 제외한 문자를 16진수로 변환
// 유니코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
_tstring Escape(const _tstring& source)
{
	size_t		size = 0;
	wchar_t		wcChar = '\0';
	const wchar_t*	pwszSourceData = nullptr;
	const wchar_t*	pwszSourceDoc = nullptr;
	wchar_t		wszExcept[] = L"*@-_+./";

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

#ifdef _UNICODE
	pwszSourceData = source.c_str();
#else
	std::wstring sourceData;
	sourceData = Iconv::CIconvUtil::ConvertEncodingW(source, "CP949", "WCHAR_T");

	pwszSourceData = sourceData.c_str();
#endif

	pwszSourceDoc = pwszSourceData;
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
			size = size + 6;
		else if( !((wcChar > 47 && wcChar < 57) || (wcChar > 64 && wcChar < 91) || (wcChar > 96 && wcChar < 123) || wcschr(wszExcept, wcChar)) )
		{
			if( wcChar <= 0xf )
				size++;

			size = size + 3;
		}
		else
			size++;

		pwszSourceDoc++;
	}

	int destIndex = 0;
	_tstring dest(size + 1, '\0');

	pwszSourceDoc = pwszSourceData;
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
		{
			dest[destIndex++] = '%';
			dest[destIndex++] = 'u';

			dest[destIndex++] = g_pcDigits[(wcChar >> 12) & 0x0F];
			dest[destIndex++] = g_pcDigits[(wcChar >> 8) & 0x0F];
			dest[destIndex++] = g_pcDigits[(wcChar >> 4) & 0x0F];
			dest[destIndex++] = g_pcDigits[wcChar & 0x0F];
		}
		else if( !((wcChar > 47 && wcChar < 57) || (wcChar > 64 && wcChar < 91) || (wcChar > 96 && wcChar < 123) || wcschr(wszExcept, wcChar)) )
		{
			dest[destIndex++] = '%';

			dest[destIndex++] = g_pcDigits[(wcChar >> 4) & 0x0F];
			dest[destIndex++] = g_pcDigits[wcChar & 0x0F];
		}
		else
			dest[destIndex++] = (TCHAR)wcChar;

		pwszSourceDoc++;
	}
	dest[destIndex] = '\0';

	return dest;
}

//***************************************************************************
//
_tstring UnEscape(const _tstring& source)
{
	size_t		size = 0;
	int			nNum = 0;
	int			nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
		{
			if( *(ptszSourceDoc + 1) == 'u' )
				ptszSourceDoc = ptszSourceDoc + 5;
			else ptszSourceDoc = ptszSourceDoc + 2;
		}

		ptszSourceDoc++;
		size++;
	}

	int destIndex = 0;
	std::wstring destData(size + 1, '\0');

	ptszSourceDoc = source.c_str();
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

				destData[destIndex++] = nRetval;
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

				destData[destIndex++] = nRetval;
			}
		}
		else
			destData[destIndex++] = *ptszSourceDoc;

		ptszSourceDoc++;
	}
	destData[destIndex] = '\0';

#ifndef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncoding(destData, "WCHAR_T", "CP949");
#else 
	_tstring dest = destData;
#endif

	return dest;
}

//***************************************************************************
// 영문자, 숫자, ',', '/', '?', ':', '@', '&', '=', '+', '$', '#' 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
_tstring EncodeURI(const _tstring& source)
{
	unsigned char cChar = '\0';

	size_t	size = 0;
	const char* pszSourceDoc = nullptr;
	char	szExcept[] = ",/?:@&=+$#";

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	std::string	sourceData;

#ifdef _UNICODE
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "UTF-8");
#else
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "CP949", "UTF-8");
#endif

	pszSourceDoc = sourceData.c_str();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

	int destIndex = 0;
	string destData(size + 1, '\0');

	pszSourceDoc = sourceData.c_str();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) || strchr(szExcept, cChar) )
			destData[destIndex++] = cChar;
		else if( cChar == ' ' )
			destData[destIndex++] = '+';
		else
		{
			destData[destIndex++] = '%';
			destData[destIndex++] = g_pcDigits[(cChar >> 4) & 0x0F];
			destData[destIndex++] = g_pcDigits[cChar & 0x0F];
		}
		pszSourceDoc++;
	}
	destData[destIndex] = '\0';

#ifdef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncodingW(destData, "CP949", "WCHAR_T");
#else
	_tstring dest = destData;
#endif

	return dest;
}

//***************************************************************************
//
_tstring DecodeURI(const _tstring& source)
{
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = source.c_str();
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

#ifdef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "UTF-8", "WCHAR_T");
#else
	_tstring dest = Iconv::CIconvUtil::ConvertEncoding(pszDestination, "UTF-8", "CP949");
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return dest;
}

//***************************************************************************
// 영문자, 숫자를 제외한 문자를 16진수로 변환
// UTF-8 코드를 16진수로 교환
// EUC-KR, UTF-7, UTF-8, UTF-16에서 동일하게 작동
_tstring EncodeURIComponent(const _tstring& source)
{
	unsigned char cChar = '\0';

	size_t	size = 0;
	const char* pszSourceDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	std::string	sourceData;

#ifdef _UNICODE
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "WCHAR_T", "UTF-8");
#else
	sourceData = Iconv::CIconvUtil::ConvertEncoding(source, "CP949", "UTF-8");
#endif

	pszSourceDoc = sourceData.c_str();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( !((cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123)) )
			size += 2;

		pszSourceDoc++;
		size++;
	}

	int destIndex = 0;
	string destData(size + 1, '\0');

	pszSourceDoc = sourceData.c_str();
	while( *pszSourceDoc )
	{
		cChar = (unsigned char)*pszSourceDoc;
		if( (cChar > 47 && cChar < 57) || (cChar > 64 && cChar < 91) || (cChar > 96 && cChar < 123) )
			destData[destIndex++] = cChar;
		else if( cChar == ' ' )
			destData[destIndex++] = '+';
		else
		{
			destData[destIndex++] = '%';
			destData[destIndex++] = g_pcDigits[(cChar >> 4) & 0x0F];
			destData[destIndex++] = g_pcDigits[cChar & 0x0F];
		}
		pszSourceDoc++;
	}
	destData[destIndex] = '\0';

#ifdef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncodingW(destData, "CP949", "WCHAR_T");
#else
	_tstring dest = destData;
#endif

	return dest;
}

//***************************************************************************
//
_tstring DecodeURIComponent(const _tstring& source)
{
	size_t	size = 0;
	int		nNum = 0;
	int		nRetval = 0;
	const TCHAR* ptszSourceDoc = nullptr;
	char*	pszDestination = nullptr;
	char*	pszDestDoc = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return _T("");

	ptszSourceDoc = source.c_str();
	while( *ptszSourceDoc )
	{
		if( *ptszSourceDoc == '%' )
			ptszSourceDoc = ptszSourceDoc + 2;

		ptszSourceDoc++;
		size++;
	}

	pszDestination = new char[size + 1];

	ptszSourceDoc = source.c_str();
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

#ifdef _UNICODE
	_tstring dest = Iconv::CIconvUtil::ConvertEncodingW(pszDestination, "UTF-8", "WCHAR_T");
#else
	_tstring dest = Iconv::CIconvUtil::ConvertEncoding(pszDestination, "UTF-8", "CP949");
#endif

	if( pszDestination )
	{
		delete[]pszDestination;
		pszDestination = nullptr;
	}

	return dest;
}
#endif
