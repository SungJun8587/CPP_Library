
//***************************************************************************
// FileUtil.cpp : implementation of the FileUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "FileUtil.h"

//***************************************************************************
//
bool IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize)
{
	bool bUTF8 = true;
	unsigned char* start = (unsigned char*)pBuffer;
	unsigned char* end = (unsigned char*)pBuffer + BuffSize;
	while( start < end )
	{
		if( *start < 0x80 )			// (10000000)[output][/output]
		{
			start++;
		}
		else if( *start < (0xC0) )	// (11000000)
		{
			bUTF8 = false;
			break;
		}
		else if( *start < (0xE0) )	// (11100000)
		{
			if( start >= end - 1 )
				break;
			if( (start[1] & (0xC0)) != 0x80 )
			{
				bUTF8 = false;
				break;
			}
			start += 2;
		}
		else if( *start < (0xF0) )	// (11110000)
		{
			if( start >= end - 2 )
				break;
			if( (start[1] & (0xC0)) != 0x80 || (start[2] & (0xC0)) != 0x80 )
			{
				bUTF8 = false;
				break;
			}
			start += 3;
		}
		else
		{
			bUTF8 = false;
			break;
		}
	}
	return bUTF8;
}

#ifdef _WIN32
//***************************************************************************
//
EEncoding IsFileType(const TCHAR* ptszFullPath)
{
	BOOL		bReturn = false;
	DWORD		dwReadSize = 0;
	char		szBuffer[4];
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return eFileType;
	}

	bReturn = ReadFile(hFile, szBuffer, 3, &dwReadSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);
		return eFileType;
	}
	szBuffer[4] = '\0';

	CloseHandle(hFile);

	if( szBuffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && szBuffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		eFileType = EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN) 
	}
	else if( szBuffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && szBuffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		eFileType = EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		if( szBuffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && szBuffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && szBuffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
			eFileType = EEncoding::UTF8_BOM;	// UTF8_BOM
		else
		{
#ifdef _STRING_
			_tstring byteDestination;
			if( !ReadFileMap(byteDestination, ptszFullPath) ) return eFileType;

			// std::string의 size() 함수는 문자열의 실제 문자 개수를 반환하며, 널 문자('\0')는 포함하지 않으므로
			if( IsUTF8WithoutBom((const void*)byteDestination.c_str(), byteDestination.size() + 1) )
				eFileType = EEncoding::UTF8_NOBOM;		// UTF8_NOBOM
			else
				eFileType = EEncoding::ANSI;			// ANSI
#else
			CMemBuffer<BYTE> ByteDestination;
			if( !ReadFileMap(ByteDestination, ptszFullPath) ) return eFileType;

			if( IsUTF8WithoutBom((const void*)ByteDestination.GetBuffer(), ByteDestination.GetBufSize()) )
				eFileType = EEncoding::UTF8_NOBOM;		// UTF8_NOBOM
			else
				eFileType = EEncoding::ANSI;			// ANSI
#endif
		}
	}

	return eFileType;
}

//***************************************************************************
//
bool ReadFile(CMemBuffer<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	BYTE* pbBuffer = nullptr;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( dwLength > dwMaxReadSize )
		dwReadNumSize = dwMaxReadSize;
	else dwReadNumSize = dwLength;

	byteDestination.Init(dwLength);

	pbBuffer = byteDestination.GetBuffer();
	while( 1 )
	{
		bool bReturn = ReadFile(hFile, pbBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
		if( bReturn == FALSE )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxReadSize > dwReadSize ) break;

		dwReadOffset = dwReadOffset + dwReadNumSize;
		lReadSize = dwLength - dwReadOffset - dwMaxReadSize;
		if( lReadSize < 0 )
			dwReadNumSize = dwLength - dwReadOffset;
		else dwReadNumSize = dwMaxReadSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool ReadFile(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszFullPath)
{
	bool	bReturn = false;
	int		i = 0;
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EEncoding	eFileType = EEncoding::DEFAULT;

	CMemBuffer<char>	StrBuffer;
	CMemBuffer<wchar_t>	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	dwMaxReadSize = dwLength;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( eFileType == EEncoding::UTF16_BE || eFileType == EEncoding::UTF16_LE )
	{
		SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);

		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.Init(dwLength + 1);
		pwszBuffer = WStrBuffer.GetBuffer();

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);
				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		CloseHandle(hFile);
	}
	else if( eFileType == EEncoding::ANSI || eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( eFileType == EEncoding::UTF8_BOM )
		{
			SetFilePointer(hFile, sizeof(WORD) + sizeof(BYTE), nullptr, FILE_BEGIN);

			dwLength = dwLength - (sizeof(WORD) + sizeof(BYTE));

			StrBuffer.Init(dwLength + 1);
			pszBuffer = StrBuffer.GetBuffer();
		}
		else
		{
			StrBuffer.Init(dwLength + 1);
			pszBuffer = StrBuffer.GetBuffer();

			dwMaxReadSize = dwMaxReadSize + 1;
		}

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);
				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		*(pszBuffer + dwReadNumSize - 1) = '\0';

		CloseHandle(hFile);
	}

	if( eFileType == EEncoding::UTF16_LE || eFileType == EEncoding::UTF16_BE )
	{
		if( pwszBuffer == nullptr ) return false;
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( pszBuffer == nullptr ) return false;
	}
	else
	{
		if( pszBuffer == nullptr ) return false;
	}

#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		tDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(tDestination.GetBuffer(), tDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer = pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		tDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(tDestination.GetBuffer(), tDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToUnicode(tDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		if( AnsiToUnicode(tDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
#else
	if( eFileType == EEncoding::UTF16_LE )
	{
		if( UnicodeToAnsi(tDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( UnicodeToAnsi(tDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToAnsi(tDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		tDestination.Init(strlen(pszBuffer) + 1);
		_tcsncpy_s(tDestination.GetBuffer(), tDestination.GetBufSize(), pszBuffer, _TRUNCATE);
	}
#endif

	return true;
}

//***************************************************************************
//
bool ReadFileMap(CMemBuffer<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	byteDestination.Init(dwLength);

	memcpy(byteDestination.GetBuffer(), lpvFile, dwLength);

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return true;
}

//***************************************************************************
//
bool ReadFileMap(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszFullPath)
{
	bool	bIsProcess = false;
	DWORD	dwLength = 0;
	int		i = 0;
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	bIsProcess = true;

#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		tDestination.Init(dwLength + 1);

		_tcsncpy_s(tDestination.GetBuffer(), dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

		pwszBuffer = new wchar_t[dwLength + 1];

		_tcsncpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		tDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(tDestination.GetBuffer(), tDestination.GetBufSize(), pwszBuffer, _TRUNCATE);

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToUnicode(tDestination, (char*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::ANSI )
	{
		if( AnsiToUnicode(tDestination, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
#else
	if( eFileType == EEncoding::UTF16_LE )
	{
		if( UnicodeToAnsi(tDestination, (wchar_t*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

		pwszBuffer = new wchar_t[dwLength + 1];

		wcscpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( UnicodeToAnsi(tDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) bIsProcess = false;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToAnsi(tDestination, (char*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::ANSI )
	{
		tDestination.Init(dwLength + 1);

		_tcsncpy_s(tDestination.GetBuffer(), tDestination.GetBufSize(), (char*)lpvFile, _TRUNCATE);
	}
#endif

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}

#ifdef _STRING_
//***************************************************************************
//
bool ReadFile(_tstring& destString, const TCHAR* ptszFullPath)
{
	bool	bReturn = false;
	int		i = 0;
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EEncoding	eFileType = EEncoding::DEFAULT;

	std::string		StrBuffer;
	std::wstring	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	dwMaxReadSize = dwLength;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( eFileType == EEncoding::UTF16_BE || eFileType == EEncoding::UTF16_LE )
	{
		SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);

		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.resize(dwLength + 1);
		pwszBuffer = (wchar_t*)WStrBuffer.c_str();

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);
				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		CloseHandle(hFile);
	}
	else if( eFileType == EEncoding::ANSI || eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( eFileType == EEncoding::UTF8_BOM )
		{
			SetFilePointer(hFile, sizeof(WORD) + sizeof(BYTE), nullptr, FILE_BEGIN);

			dwLength = dwLength - (sizeof(WORD) + sizeof(BYTE));

			StrBuffer.resize(dwLength + 1);
			pszBuffer = (char *)StrBuffer.c_str();
		}
		else
		{
			StrBuffer.resize(dwLength + 1);
			pszBuffer = (char*)StrBuffer.c_str();

			dwMaxReadSize = dwMaxReadSize + 1;
		}

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);
				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		*(pszBuffer + dwReadNumSize - 1) = '\0';

		CloseHandle(hFile);
	}

	if( eFileType == EEncoding::UTF16_LE || eFileType == EEncoding::UTF16_BE )
	{
		if( pwszBuffer == nullptr ) return false;
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( pszBuffer == nullptr ) return false;
	}
	else
	{
		if( pszBuffer == nullptr ) return false;
	}

#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		destString = pwszBuffer;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer = pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		destString = pwszBuffer;
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToUnicode_String(destString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		if( AnsiToUnicode_String(destString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
#else
	if( eFileType == EEncoding::UTF16_LE )
	{
		if( UnicodeToAnsi_String(destString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( UnicodeToAnsi_String(destString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToAnsi_String(destString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		destString = pszBuffer;
	}
#endif

	return true;
}

//***************************************************************************
//
bool ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath)
{
	bool	bIsProcess = false;
	DWORD	dwLength = 0;
	int		i = 0;
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	bIsProcess = true;

#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		destString = (wchar_t*)lpvFile + 1;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

		pwszBuffer = new wchar_t[dwLength + 1];

		_tcsncpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		destString = pwszBuffer;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == EEncoding::UTF8_BOM )
	{
		if( Utf8ToUnicode_String(destString, (char*)lpvFile + 3, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToUnicode_String(destString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::ANSI )
	{
		if( AnsiToUnicode_String(destString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
#else
	if( eFileType == EEncoding::UTF16_LE )
	{
		if( UnicodeToAnsi_String(destString, (wchar_t*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

		pwszBuffer = new wchar_t[dwLength + 1];

		wcscpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( UnicodeToAnsi_String(destString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) bIsProcess = false;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == EEncoding::UTF8_BOM )
	{
		if( Utf8ToAnsi_String(destString, (char*)lpvFile + 3, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::UTF8_NOBOM )
	{
		if( Utf8ToAnsi_String(destString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == EEncoding::ANSI )
	{
		destString = (char*)lpvFile;
	}
#endif

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}
#endif

//***************************************************************************
//
bool SaveFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength)
{
	long	lWriteSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( dwLength > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwLength;

	while( 1 )
	{
		bool bReturn = WriteFile(hFile, pbBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
		if( bReturn == FALSE )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwLength - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwLength - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	StrBuffer;

	if( UnicodeToAnsi(StrBuffer, ptszBuffer, _tcslen(ptszBuffer) + 1) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	pszBuffer = (char*)ptszBuffer;
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwTotFileSize = (DWORD)strlen(pszBuffer);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pszBuffer + dwWriteOffset, (DWORD)dwWriteSize, (LPDWORD)&dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char	szChar[3];
	wchar_t	wcChar = L'\0';
	wchar_t	wszChar[2];
	wchar_t* pwszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( AnsiToUnicode(WStrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	sprintf_s(szChar, 3, "%c%c", UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2);
	bReturn = WriteFile(hFile, szChar, static_cast<DWORD>(strlen(szChar)), &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);
		return false;
	}

	while( *pwszBuffer )
	{
		wcChar = *pwszBuffer;

		wcChar = SWAP16(wcChar);
		if( wcChar == 0xCDCD ) break;

		swprintf_s(wszChar, 2, L"%c", wcChar);
		bReturn = WriteFile(hFile, wszChar, (DWORD)wcslen(wszChar) + 1, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);
			return false;
		}

		pwszBuffer++;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char	szChar[3];
	wchar_t* pwszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( AnsiToUnicode(WStrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	sprintf_s(szChar, 3, "%c%c", UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2);
	bReturn = WriteFile(hFile, szChar, static_cast<DWORD>(strlen(szChar)), &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);
		return false;
	}

	dwTotFileSize = ((DWORD)wcslen(pwszBuffer) * 2);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pwszBuffer + (dwWriteOffset / 2), dwWriteSize, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool SaveUTF8BOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char	szChar[4];
	char*	pszBuffer = nullptr;

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	if( AnsiToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	sprintf_s(szChar, 4, "%c%c%c", UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3);
	bReturn = WriteFile(hFile, szChar, static_cast<DWORD>(strlen(szChar)), &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);
		return false;
	}

	dwTotFileSize = (DWORD)strlen(pszBuffer);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool SaveUTF8NOBOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = nullptr;

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	if( AnsiToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwTotFileSize = (DWORD)strlen(pszBuffer);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);
			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
bool GetFileInfoTime(const TCHAR* ptszFullPath, const int nCase, SYSTEMTIME& stLocal)
{
	FILETIME ftCreate, ftAccess, ftWrite;
	SYSTEMTIME stUTC;

	HANDLE hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return false;

	// Retrieve the file times for the file.
	if( !GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite) )
	{
		CloseHandle(hFile);
		return false;
	}

	CloseHandle(hFile);

	if( nCase == FILEINFO_CREATETIME )
		FileTimeToSystemTime(&ftCreate, &stUTC);
	else if( nCase == FILEINFO_ACCESSTIME )
		FileTimeToSystemTime(&ftAccess, &stUTC);
	else if( nCase == FILEINFO_LASTWRITETIME )
		FileTimeToSystemTime(&ftWrite, &stUTC);
	else return false;

	SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

	return true;
}

//***************************************************************************
//
bool IsExistFile(const TCHAR* ptszFullPath)
{
	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
DWORD GetFileSize(const TCHAR* ptszFullPath)
{
	DWORD		dwFileSizeLow = 0;
	DWORD		dwFileSizeHigh = 0;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	else dwFileSizeLow = 0;

	CloseHandle(hFile);

	return dwFileSizeLow;
}

//***************************************************************************
//
bool GetFileInformation(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	bool		bResult = false;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		bResult = GetFileInformationByHandle(hFile, lpFileInformation);

	CloseHandle(hFile);

	return bResult;
}

//#else
//***************************************************************************
//
EEncoding GetFileEncodingType(const _tstring& filepath)
{
	EEncoding	eEncoding = EEncoding::DEFAULT;

	constexpr size_t BufferSize = 4096; // 파일 검사 시 읽을 최대 크기
	std::ifstream file(filepath, std::ios::binary);
	if( !file ) 
	{
		return eEncoding;
	}

	// 파일의 처음 몇 바이트 읽기
	std::vector<unsigned char> buffer(BufferSize);
	file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
	size_t bytesRead = file.gcount();

	if( buffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && buffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		eEncoding = EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN) 
	}
	else if( buffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && buffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		eEncoding = EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		if( buffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && buffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && buffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
			eEncoding = EEncoding::UTF8_BOM;	// UTF8_BOM
		else
		{
			if( IsUTF8WithoutBom((const void*)buffer.data(), bytesRead) )
				eEncoding = EEncoding::UTF8_NOBOM;		// UTF8_NOBOM
			else
				eEncoding = EEncoding::ANSI;			// ANSI
		}
	}

	return eEncoding;
}

//***************************************************************************
//
_tstring ReadFile(const _tstring& filepath)
{
	_tifstream file(filepath, std::ios::binary);
	if( !file ) 
	{
		return _T("");
	}
	return { std::istreambuf_iterator<TCHAR>(file), std::istreambuf_iterator<TCHAR>() };
}

//***************************************************************************
//
bool WriteFile(const _tstring& filepath, const _tstring& content, EEncoding encoding)
{
	ofstream file(filepath, std::ios::binary);
	if( !file ) 
	{
		return false;
	}

#ifdef _UNICODE
	if( encoding == EEncoding::UTF16_BE )
	{
		unsigned char bom[] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		for( wchar_t ch : content )
		{
			char high = (ch >> 8) & 0xFF;
			char low = ch & 0xFF;
			file.put(high);
			file.put(low);
		}
	}
	else if( encoding == EEncoding::UTF16_LE )
	{
		// UTF-16 LE BOM
		unsigned char bom[] = { UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		// Write content in UTF-16 LE
		file.write(reinterpret_cast<const char*>(content.data()), content.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		// UTF-8 BOM
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		// Write content in UTF-8 with BOM
		string dest = UnicodeToUtf8(content);
		file.write(dest.c_str(), dest.size());
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		// Write content in UTF-8 without BOM
		string dest = UnicodeToUtf8(content);
		file.write(dest.c_str(), dest.size());
	}
	else
	{
		string dest = WStringToString(content);
		file.write(dest.c_str(), dest.size());
	}
#else
	if( encoding == EEncoding::UTF16_BE )
	{
		unsigned char bom[] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		wstring dest = StringToWString(content);
		for( wchar_t ch : dest )
		{
			char high = (ch >> 8) & 0xFF;
			char low = ch & 0xFF;
			file.put(high);
			file.put(low);
		}
	}
	else if( encoding == EEncoding::UTF16_LE )
	{
		// UTF-16 LE BOM
		unsigned char bom[] = { UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		// Write content in UTF-16 LE
		wstring dest = StringToWString(content);
		file.write(reinterpret_cast<const char*>(dest.data()), dest.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		// UTF-8 BOM
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		// Write content in UTF-8 with BOM
		string dest = AnsiToUtf8(content);
		file.write(dest.c_str(), dest.size());
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		// Write content in UTF-8 without BOM
		string dest = AnsiToUtf8(content);
		file.write(dest.c_str(), dest.size());
	}
	else
	{
		file.write(content.c_str(), content.size());
	}
#endif

	file.close();

	return true;
}

//***************************************************************************
//
std::uintmax_t GetFileSize(const _tstring& filepath)
{
	try
	{
		return std::filesystem::file_size(filepath);
	}
	catch( const std::filesystem::filesystem_error& e )
	{
		std::cerr << "파일 크기 확인 중 오류 발생: " << e.what() << std::endl;
		return static_cast<std::uintmax_t>(-1);
	}
}
#endif
