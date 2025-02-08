
//***************************************************************************
// StringUtil.cpp: implementation of the StringUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "StringUtil.h"

#ifdef	__MEMBUFFER_H__
//***************************************************************************
//Function to passing FolderPath to FullFilePath 
bool FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr ) return false;
	if( (length = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		size++;
	}

	TFolderPath.Init(length - size + 1);

	for( ptszSourceLoc = ptszFullFilePath, ptszDestLoc = TFolderPath.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - size ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//Function to passing FileNameExt to FullFilePath 
bool FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath)
{
	size_t	length = 0;
	size_t	size = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullFilePath == nullptr ) return false;
	if( (length = _tcslen(ptszFullFilePath)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullFilePath + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		size++;
	}

	TFileNameExt.Init(size + 1);

	ptszSourceLoc = ptszFullFilePath + length - size;
	ptszDestLoc = TFileNameExt.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//Function to passing FileName and FileExt to FileNameExt 
bool FileNameExtPassing(CMemBuffer<TCHAR>& TFileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFileNameExt == nullptr ) return false;
	if( (length = _tcslen(ptszFileNameExt)) < 1 ) return false;

	for( ptszSourceLoc = ptszFileNameExt + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('.') )
		{
			size++;
			break;
		}

		size++;
	}

	TFileName.Init(length - size + 1);

	for( ptszSourceLoc = ptszFileNameExt, ptszDestLoc = TFileName.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - size ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	TFileExt.Init(size);

	ptszSourceLoc = ptszFileNameExt + length - size + 1;
	ptszDestLoc = TFileExt.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
// 
bool HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (length = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		size = length - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + size; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		size++;
	}

	THostName.Init(size + 1);

	for( ptszSourceLoc = ptszFullUrl, ptszDestLoc = THostName.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == size ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl)
{
	size_t	length = 0;
	size_t	first = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (length = _tcslen(ptszFullUrl)) < 1 ) return false;

	if( (ptszSourceLoc = _tcsstr(ptszFullUrl, _T("://"))) != NULL )
		first = length - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = ptszFullUrl + first; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		first++;
	}

	for( ptszSourceLoc = ptszFullUrl + length; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		size++;
	}

	TUrlFullPath.Init(length - first - size + 1);

	for( ptszSourceLoc = ptszFullUrl + first, ptszDestLoc = TUrlFullPath.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - first - size ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
// 
bool QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl)
{
	size_t	length = 0;
	size_t	size = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszFullUrl == nullptr ) return false;
	if( (length = _tcslen(ptszFullUrl)) < 1 ) return false;

	for( ptszSourceLoc = ptszFullUrl + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		size++;
	}

	TQueryString.Init(size + 1);

	ptszSourceLoc = ptszFullUrl + length - size;
	ptszDestLoc = TQueryString.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
bool Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	length = 0;
	size_t	index = 0;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszNextToken = nullptr;

	if( ptszSource == nullptr || ptszToken == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( !_tcsstr(ptszSource, ptszToken) ) return false;

	ptszSourceLoc = new TCHAR[length + 1];
	_tcsncpy_s(ptszSourceLoc, length + 1, ptszSource, _TRUNCATE);

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ppTDestination[index].Init(_tcslen(ptszTokenize) + 1);

		_tcsncpy_s(ppTDestination[index].GetBuffer(), _tcslen(ptszTokenize) + 1, ptszTokenize, _TRUNCATE);

		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);

		index++;
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
bool StrUpper(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(length + 1);

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
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(length + 1);

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
	size_t	length = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(length + 1);

	for( ptszSourceLoc = ptszSource + length - 1, ptszDestLoc = TDestination.GetBuffer(); index < length; ptszSourceLoc--, ptszDestLoc++ )
	{
		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrAppend(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend)
{
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr || ptszAppend == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 0 ) return false;

	TDestination.Init(length + _tcslen(ptszAppend) + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;

	for( ptszSourceLoc = ptszAppend; *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
		*ptszDestLoc = *ptszSourceLoc;

	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t start)
{
	size_t	length = 0;
	size_t	first = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( start < 0 || start > length - 1 ) return false;

	first = start;

	TDestination.Init(length - first + 1);

	ptszSourceLoc = ptszSource + start;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
bool StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t start, const size_t count)
{
	size_t	length = 0;
	size_t	first = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( start < 0 || start > length - 1 ) return false;
	if( count < 0 || start + count > length - 1 ) return false;

	first = start;

	TDestination.Init(count + 1);

	ptszSourceLoc = ptszSource + first;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( index == count ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//Function to cut string until special character 
bool StrMidToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t start, const TCHAR tcToken)
{
	size_t	length = 0;
	size_t	first = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( start < 0 || start > length - 1 ) return false;
	if( !_tcschr(ptszSource, tcToken) ) return false;

	first = start;

	TDestination.Init(length - first + 1);

	ptszSourceLoc = ptszSource + first;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( *ptszSourceLoc == tcToken ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t count)
{
	size_t	length = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( count < 0 || count > length ) return false;

	TDestination.Init(count + 1);

	ptszSourceLoc = ptszSource;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszSourceLoc )
	{
		if( index == count ) break;

		*ptszDestLoc++ = *ptszSourceLoc++;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t count)
{
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( count < 0 || count > length ) return false;

	TDestination.Init(count + 1);

	ptszSourceLoc = ptszSource + length - count;
	ptszDestLoc = TDestination.GetBuffer();

	while( *ptszDestLoc++ = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
//
bool StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken)
{
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	TDestination.Init(length + 1);

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
	size_t	length = 0;
	size_t	nTokecount = 0;
	size_t	index = 0;
	size_t	nSubLen = 0;
	size_t	nEndIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	const TCHAR* ptszSrcTemp = nullptr;
	const TCHAR* ptszDestTemp = nullptr;
	TCHAR* ptszTokenize = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr || ptszSrcToken == nullptr || ptszDestToken == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( *ptszSourceLoc == *ptszSrcToken )
		{
			bResult = true;
			for( index = 0, ptszSrcTemp = ptszSrcToken; *ptszSrcTemp; ptszSrcTemp++, index++ )
			{
				if( *(ptszSourceLoc + index) != *ptszSrcTemp )
				{
					bResult = false;
					break;
				}
			}

			if( bResult )
			{
				nTokecount++;
				ptszSourceLoc = ptszSourceLoc + _tcslen(ptszSrcToken) - 1;
			}
		}
	}

	if( nTokecount < 0 )
	{
		nEndIndex = _tcslen(ptszSource);

		TDestination.Init(nEndIndex + 1);

		_tcsncpy_s(TDestination.GetBuffer(), nEndIndex + 1, ptszSource, _TRUNCATE);

		return true;
	}

	nSubLen = _tcslen(ptszDestToken) - _tcslen(ptszSrcToken);
	nEndIndex = length + (nSubLen * nTokecount);

	TDestination.Init(nEndIndex + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( *ptszSourceLoc == *ptszSrcToken )
		{
			bResult = true;
			for( index = 0, ptszSrcTemp = ptszSrcToken; *ptszSrcTemp; ptszSrcTemp++, index++ )
			{
				if( *(ptszSourceLoc + index) != *ptszSrcTemp )
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
bool TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	length = 0;
	size_t	count = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

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
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

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
	size_t	length = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

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
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + length - 1; count < length; ptszSourceLoc-- )
	{
		if( _istspace(*ptszSourceLoc) ) count++;
		else break;
	}

	TDestination.Init(length - count + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - count ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR ctToken)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + length - 1; count < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == ctToken ) count++;
		else break;
	}

	TDestination.Init(length - count + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - count ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;

	for( ptszSourceLoc = ptszSource + length - 1; count < length; ptszSourceLoc-- )
	{
		if( _tcschr(ptszToken, *ptszSourceLoc) == nullptr )
			break;
		count++;
	}

	TDestination.Init(length - count + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == length - count ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCutUnicodeAscii(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nSize)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;
	size_t	nEndIndex = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( nSize < 0 || nSize > length ) return false;

#ifndef _UNICODE
	for( count = 0, ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !((unsigned)(*ptszSourceLoc) < 0x80) ) count++;
	}

	if( count % 2 == 1 )
		nEndIndex = nSize + (count / 2) + 1;
	else nEndIndex = nSize + (count / 2);
#else
	nEndIndex = nSize;
#endif

	TDestination.Init(nEndIndex + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == nEndIndex ) break;

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( nLocation == 0 ) return false;

	if( (length % nLocation) == 0 )
		count = length / nLocation - 1;
	else
		count = length / nLocation;	// ((int)nValue / (int)nDiv) 은 항상 소숫점 버림

	TDestination.Init(length + count + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == nLocation )
		{
			*ptszDestLoc = tcToken;
			ptszDestLoc++;
			index = 0;
		}

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}

//***************************************************************************
//
bool StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken)
{
	size_t	length = 0;
	size_t	count = 0;
	size_t	index = 0;
	const TCHAR* ptszSourceLoc = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	if( ptszSource == nullptr ) return false;
	if( (length = _tcslen(ptszSource)) < 1 ) return false;
	if( nLocation == 0 ) return false;

	if( (length % nLocation) == 0 )
		count = length / nLocation - 1;
	else
		count = length / nLocation;	// ((int)nValue / (int)nDiv) 은 항상 소숫점 버림

	TDestination.Init(length + (count * _tcslen(ptszToken)) + 1);

	for( ptszSourceLoc = ptszSource, ptszDestLoc = TDestination.GetBuffer(); *ptszSourceLoc; ptszSourceLoc++, ptszDestLoc++ )
	{
		if( index == nLocation )
		{
			while( ptszToken )
			{
				*ptszDestLoc = *ptszToken;
				ptszDestLoc++;
				ptszToken++;
			}

			index = 0;
		}

		*ptszDestLoc = *ptszSourceLoc;
		index++;
	}
	*ptszDestLoc = _T('\0');

	return true;
}
#endif


#ifdef _STRING_
//***************************************************************************
//Function to passing FolderPath to FullFilePath 
_tstring FolderPathPassing(const _tstring& fullFilePath)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;

	if( fullFilePath.c_str() == nullptr || fullFilePath.size() == 0 ) return _T("");

	length = fullFilePath.size();
	for( ptszSourceLoc = fullFilePath.c_str() + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;
		size++;
	}

	if( length - size > DIRECTORY_STRLEN ) return _T("");

	_tstring dest(length - size + 1, _T('\0'));

	for( ptszSourceLoc = fullFilePath.c_str(); *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( index == length - size ) break;

		dest[index++] = *ptszSourceLoc;
	}
	dest[index] = _T('\0');

	return dest;
}

//***************************************************************************
//Function to passing FileNameExt to FullFilePath 
_tstring FileNameExtPathPassing(const _tstring& fullFilePath)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;

	if( fullFilePath.c_str() == nullptr || fullFilePath.size() == 0 ) return _T("");

	length = fullFilePath.size();
	for( ptszSourceLoc = fullFilePath.c_str() + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('/') || *ptszSourceLoc == _T('\\') ) break;

		size++;
	}

	if( size > FILENAMEEXT_STRLEN ) return _T("");

	_tstring dest(size + 1, _T('\0'));

	ptszSourceLoc = fullFilePath.c_str() + length - size;

	while( dest[index++] = *ptszSourceLoc++ );

	return dest;
}

//***************************************************************************
//Function to passing FileName and FileExt to FileNameExt 
bool FileNameExtPassing(const _tstring& fileNameExt, _tstring& fileName, _tstring& fileExt)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;
	
	if( fileNameExt.c_str() == nullptr || fileNameExt.size() == 0 ) return false;

	length = fileNameExt.size();
	for( ptszSourceLoc = fileNameExt.c_str() + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == _T('.') )
		{
			size++;
			break;
		}
		size++;
	}

	if( length - size > FILENAME_STRLEN ) return false;

	fileName.resize(length - size + 1);
	for( ptszSourceLoc = fileNameExt.c_str(); *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( index == length - size ) break;

		fileName[index++] = *ptszSourceLoc;
	}
	fileName[index] = _T('\0');

	if( size > FILEEXT_STRLEN ) return false;

	fileExt.resize(size + 1);
	ptszSourceLoc = fileNameExt.c_str() + length - size + 1;

	index = 0;
	while( fileExt[index++] = *ptszSourceLoc++ );

	return true;
}

//***************************************************************************
// 
bool ParseURL(const _tstring& fullUrl, _tstring& protocol, _tstring& hostName, _tstring& request, int& nPort)
{
	size_t	nTotalLen;
	size_t	nProtocolLen;
	size_t	nHostLen;
	size_t	nRequestLen;
	TCHAR*	ptszWork = nullptr;
	TCHAR*	ptszPoint1 = nullptr;
	TCHAR*	ptszPoint2 = nullptr;

	nPort = 80;

	if( fullUrl.c_str() == nullptr || fullUrl.size() == 0 ) return false;
	nTotalLen = fullUrl.size();

	ptszWork = _tcsdup(fullUrl.c_str());
	if( (ptszPoint1 = _tcsstr(ptszWork, _T("://"))) != NULL )
	{
		nProtocolLen = nTotalLen - _tcslen(ptszPoint1) + 3;
		hostName.resize(nProtocolLen + 1);
		_tcsncpy_s(const_cast<TCHAR*>(protocol.c_str()), nProtocolLen + 1, ptszWork, _TRUNCATE);
	}
	else
	{
		nProtocolLen = 0;
		hostName.resize(8);
		_tcsncpy_s(const_cast<TCHAR*>(protocol.c_str()), 8, _T("http://"), _TRUNCATE);
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

	hostName.resize(nHostLen + 1);
	_tcsncpy_s(const_cast<TCHAR*>(hostName.c_str()), nHostLen + 1, ptszPoint1, _TRUNCATE);

	nRequestLen = nTotalLen - nProtocolLen - nHostLen;

	request.resize(nRequestLen + 1);
	_tcsncpy_s(const_cast<TCHAR*>(request.c_str()), nRequestLen + 1, fullUrl.c_str() + (ptszPoint2 - ptszWork), _TRUNCATE);

	ptszPoint1 = _tcschr(const_cast<TCHAR*>(hostName.c_str()), ':');									// find the port number, if any
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
_tstring HostNamePassing(const _tstring& fullUrl)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;

	if( fullUrl.c_str() == nullptr || fullUrl.size() == 0 ) return _T("");
	length = fullUrl.size();

	if( (ptszSourceLoc = _tcsstr(fullUrl.c_str(), _T("://"))) != NULL )
		size = length - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = fullUrl.c_str() + size; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		size++;
	}

	if( size > HTTP_PROTOCOL_STRLEN + HTTP_HOSTNAME_STRLEN ) return _T("");

	_tstring dest(size + 1, _T('\0'));

	for( ptszSourceLoc = fullUrl.c_str(); *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( index == size ) break;

		dest[index++] = *ptszSourceLoc;
	}
	dest[index] = _T('\0');

	return dest;
}

//***************************************************************************
// 
_tstring UrlFullPathPassing(const _tstring& fullUrl)
{
	size_t	length = 0;
	size_t	first = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;

	if( fullUrl.c_str() == nullptr || fullUrl.size() == 0 ) return _T("");
	length = fullUrl.size();

	if( (ptszSourceLoc = _tcsstr(fullUrl.c_str(), _T("://"))) != NULL )
		first = length - _tcslen(ptszSourceLoc) + 3;

	for( ptszSourceLoc = fullUrl.c_str() + first; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( !isalpha(*ptszSourceLoc) && !isdigit(*ptszSourceLoc) && *ptszSourceLoc != _T('-') && *ptszSourceLoc != _T('.') && *ptszSourceLoc != _T(':') )
			break;

		first++;
	}

	for( ptszSourceLoc = fullUrl.c_str() + length; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		size++;
	}

	if( length - first - size > HTTP_URLPATH_STRLEN ) return _T("");

	_tstring dest(length - first - size + 1, _T('\0'));

	for( ptszSourceLoc = fullUrl.c_str() + first; *ptszSourceLoc; ptszSourceLoc++ )
	{
		if( index == length - first - size ) break;

		dest[index++] = *ptszSourceLoc;
	}
	dest[index] = _T('\0');

	return dest;
}

//***************************************************************************
//
_tstring QueryStringPassing(const _tstring& fullUrl)
{
	size_t	length = 0;
	size_t	size = 0;
	size_t	index = 0;

	const TCHAR* ptszSourceLoc = nullptr;

	if( fullUrl.c_str() == nullptr || fullUrl.size() == 0 ) return _T("");
	length = fullUrl.size();

	for( ptszSourceLoc = fullUrl.c_str() + length - 1; size < length; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == URL_QUERYSTRING_CHAR_TOKEN ) break;
		size++;
	}

	if( size > HTTP_QUERYSTRING_STRLEN ) return _T("");

	_tstring dest(size + 1, _T('\0'));

	ptszSourceLoc = fullUrl.c_str() + length - size;
	while( dest[index++] = *ptszSourceLoc++ );

	return dest;
}

//***************************************************************************
//
size_t TokenCount(const _tstring& source, const _tstring& token)
{
	size_t	length = 0;
	size_t	count = 0;
	TCHAR*	ptszTokenize = nullptr;
	TCHAR*	ptszSourceLoc = nullptr;
	TCHAR*	ptszToken = nullptr;
	TCHAR*	ptszNextToken = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return -1;
	if( token.c_str() == nullptr || token.size() == 0 ) return -1;

	if( !_tcsstr(const_cast<TCHAR*>(source.c_str()), const_cast<TCHAR*>(token.c_str())) ) return -1;

	length = source.size();
	ptszSourceLoc = new TCHAR[length + 1];

	_tcsncpy_s(ptszSourceLoc, length + 1, const_cast<TCHAR*>(source.c_str()), _TRUNCATE);
	ptszToken = const_cast<TCHAR*>(token.c_str());

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	while( ptszTokenize )
	{
		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);
		count++;
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return count;
}

//***************************************************************************
//
bool Tokenize(std::vector<_tstring>& dests, const _tstring& source, const _tstring& token)
{
	size_t	length = 0;
	TCHAR*	ptszTokenize = nullptr;
	TCHAR*	ptszSourceLoc = nullptr;
	TCHAR*	ptszToken = nullptr;
	TCHAR*	ptszNextToken = nullptr;

	if( source.c_str() == nullptr || source.size() == 0 ) return false;
	if( token.c_str() == nullptr || token.size() == 0 ) return false;
	if( !_tcsstr(const_cast<TCHAR*>(source.c_str()), const_cast<TCHAR*>(token.c_str())) ) return false;

	length = source.size();
	ptszSourceLoc = new TCHAR[length + 1];

	_tcsncpy_s(ptszSourceLoc, length + 1, const_cast<TCHAR*>(source.c_str()), _TRUNCATE);
	ptszToken = const_cast<TCHAR*>(token.c_str());

	ptszTokenize = _tcstok_s(ptszSourceLoc, ptszToken, &ptszNextToken);
	for( int i = 0; ptszTokenize != NULL; i++ )
	{
		_tstring dest(ptszTokenize);
		ptszTokenize = _tcstok_s(nullptr, ptszToken, &ptszNextToken);
		dests.push_back(dest);
	}

	if( ptszSourceLoc )
	{
		delete[]ptszSourceLoc;
		ptszSourceLoc = nullptr;
	}

	return true;
}
#endif


