//***************************************************************************
// FileUtil.cpp : implementation of the FileUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "FileUtil.h"
#include "ConvertCharset.h" // 문자셋 변환 함수 헤더 포함

//***************************************************************************
// @brief 바이트 배열이 BOM이 없는 UTF-8 인코딩 형식인지 검증합니다.
// @param pBuffer 검사할 데이터 버퍼 포인터
// @param BuffSize 버퍼 크기
// @return UTF-8 형식이 맞으면 true, 아니면 false
bool IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize)
{
	bool bUTF8 = true;
	unsigned char* start = (unsigned char*)pBuffer;
	unsigned char* end = (unsigned char*)pBuffer + BuffSize;

	// 버퍼 끝까지 순회하며 UTF-8 바이트 규칙 검사
	while( start < end )
	{
		if( *start < 0x80 )			// 1바이트 문자 (0xxxxxxx)
		{
			start++;
		}
		else if( *start < (0xC0) )	// 잘못된 연속 바이트 (10xxxxxx)
		{
			bUTF8 = false;
			break;
		}
		else if( *start < (0xE0) )	// 2바이트 문자 (110xxxxx 10xxxxxx)
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
		else if( *start < (0xF0) )	// 3바이트 문자 (1110xxxx 10xxxxxx 10xxxxxx)
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
		else						// 4바이트 이상 혹은 허용되지 않는 바이트
		{
			bUTF8 = false;
			break;
		}
	}
	return bUTF8;
}

#ifdef _WIN32
//***************************************************************************
// @brief 파일 경로를 받아 Win32 API 기반으로 파일의 인코딩 타입(UTF-16, UTF-8, ANSI 등)을 판별합니다.
// @param ptszFullPath 파일 전체 경로
// @return 판별된 인코딩 타입 (EEncoding 열거형)
EEncoding GetFileEncodingType(const TCHAR* ptszFullPath)
{
	BOOL		bReturn = false;
	DWORD		dwReadSize = 0;
	char		szBuffer[4] = { 0, };
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile;

	// 파일 읽기 전용으로 오픈
	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return eFileType;

	// [수정] Win32 API의 ReadFile을 명확히 호출하기 위해 앞에 '::'를 붙여 충돌을 방지합니다.
	bReturn = ::ReadFile(hFile, szBuffer, 3, &dwReadSize, NULL);
	if( !bReturn || dwReadSize < 2 ) // 최소 2바이트(BOM)는 읽어야 함
	{
		CloseHandle(hFile);
		return eFileType;
	}
	szBuffer[3] = '\0';

	CloseHandle(hFile);

	// BOM 시그니처에 따른 인코딩 종류 분기
	if( (unsigned char)szBuffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		eFileType = EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN)
	}
	else if( (unsigned char)szBuffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		eFileType = EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		if( (unsigned char)szBuffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && (unsigned char)szBuffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
		{
			eFileType = EEncoding::UTF8_BOM;	// UTF8_BOM
		}
		else
		{
			// BOM이 없는 경우 파일 전체를 읽어 UTF-8(Without BOM)인지 판별
			HANDLE hFileWithoutBom = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
			if( hFileWithoutBom != INVALID_HANDLE_VALUE )
			{
				DWORD dwFileSize = GetFileSize(hFileWithoutBom, nullptr);
				if( dwFileSize > 0 )
				{
					std::vector<char> byteDestination(dwFileSize);
					DWORD dwBytesRead = 0;
					if( ::ReadFile(hFileWithoutBom, byteDestination.data(), dwFileSize, &dwBytesRead, NULL) )
					{
						if( IsUTF8WithoutBom((const void*)byteDestination.data(), dwBytesRead) )
							eFileType = EEncoding::UTF8_NOBOM;		// UTF8_NOBOM
						else
							eFileType = EEncoding::ANSI;			// ANSI
					}
				}
				else
				{
					eFileType = EEncoding::ANSI;
				}
				CloseHandle(hFileWithoutBom);
			}
			else
			{
				eFileType = EEncoding::ANSI;
			}
		}
	}

	return eFileType;
}

//***************************************************************************
// @brief Win32 API를 사용하여 파일을 바이너리 벡터로 읽어옵니다.
// @param byteDestination 파일 내용을 담을 바이트 벡터 참조
// @param ptszFullPath 읽어올 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
bool ReadFile(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	DWORD dwLength = GetFileSize(ptszFullPath);

	// 파일 핸들 생성
	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// 파일 크기만큼 벡터 버퍼 확보
	byteDestination.resize(dwLength);

	const DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD		dwReadOffset = 0;
	BYTE* pbBuffer = byteDestination.data();

	// 버퍼 크기 단위로 나누어 파일 읽기 수행
	while( dwReadOffset < dwLength )
	{
		DWORD dwRemain = dwLength - dwReadOffset;
		DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
		DWORD dwReadSize = 0;

		if( !ReadFile(hFile, pbBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwReadOffset += dwReadSize;	// 실제로 읽은 만큼만 전진 (부분 읽기 대응)
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 메모리 맵(Memory Map) 방식을 이용하여 파일을 고속으로 읽어 벡터에 담습니다.
// @param byteDestination 파일 내용을 담을 바이트 벡터 참조
// @param ptszFullPath 읽어올 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
bool ReadFileMap(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	dwLength = GetFileSize(ptszFullPath);

	// 파일 매핑 객체 생성
	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	// 뷰 생성하여 메모리 주소 획득
	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);
	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	// 벡터 크기를 맞추고 메모리 복사 수행
	byteDestination.resize(dwLength);
	memcpy(byteDestination.data(), lpvFile, dwLength);

	// 리소스 해제
	UnmapViewOfFile(lpvFile);
	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return true;
}

//***************************************************************************
// @brief ANSI 형식으로 문자열 데이터를 파일에 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @return 성공 시 true, 실패 시 false
bool SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || BufferSize == 0 ) return false;

	std::string strAnsi;

#ifdef _UNICODE
	// [수정] 널 문자를 포함하여 변환했다면, 아래에서 널 문자를 제외하고 파일에 씁니다.
	if( UnicodeToAnsi_String(strAnsi, ptszBuffer, BufferSize) != 0 ) return false;
#else
	strAnsi.assign(ptszBuffer, BufferSize);
#endif

	// [수정] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제외
	if( !strAnsi.empty() && strAnsi.back() == '\0' )
	{
		strAnsi.pop_back();
	}

	// 파일 쓰기 전용 핸들 오픈
	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const char* pszBuffer = strAnsi.data();
	const DWORD	dwTotFileSize = static_cast<DWORD>(strAnsi.size());
	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	// 분할하여 파일 쓰기 진행
	while( dwWriteOffset < dwTotFileSize )
	{
		DWORD dwRemain = dwTotFileSize - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;
		DWORD dwWrittenSize = 0;

		if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 유니코드 Big Endian(UTF-16 BE) 형식으로 파일에 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @return 성공 시 true, 실패 시 false
bool SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	std::wstring strUnicode;

#ifdef _UNICODE
	strUnicode = ptszBuffer;
#else
	if( AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

	// Big Endian으로 바이트 스왑 (0xCDCD 디버그 패턴에서 중단하던 기존 동작 유지)
	std::wstring strBE;
	strBE.reserve(strUnicode.size());
	for( wchar_t ch : strUnicode )
	{
		wchar_t wcChar = SWAP16(ch);
		if( wcChar == 0xCDCD ) break;
		strBE.push_back(wcChar);
	}

	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// UTF-16 BE BOM 작성
	char szBom[2] = { (char)UNICODE_BE_FILE_IDENTIFIER_BYTE1, (char)UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
	DWORD dwWrittenSize = 0;
	if( !WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
	{
		CloseHandle(hFile);
		return false;
	}

	const char* pszBuffer = reinterpret_cast<const char*>(strBE.data());
	const DWORD	dwTotFileSize = static_cast<DWORD>(strBE.size() * sizeof(wchar_t));
	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	while( dwWriteOffset < dwTotFileSize )
	{
		DWORD dwRemain = dwTotFileSize - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;

		if( !WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 유니코드 Little Endian(UTF-16 LE) 형식으로 파일에 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @return 성공 시 true, 실패 시 false
bool SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	std::wstring strUnicode;

#ifdef _UNICODE
	strUnicode = ptszBuffer;
#else
	if( AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// UTF-16 LE BOM 작성
	char szBom[2] = { (char)UNICODE_LE_FILE_IDENTIFIER_BYTE1, (char)UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
	DWORD dwWrittenSize = 0;
	if( !WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
	{
		CloseHandle(hFile);
		return false;
	}

	const char* pszBuffer = reinterpret_cast<const char*>(strUnicode.data());
	const DWORD	dwTotFileSize = static_cast<DWORD>(strUnicode.size() * sizeof(wchar_t));
	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	while( dwWriteOffset < dwTotFileSize )
	{
		DWORD dwRemain = dwTotFileSize - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;

		if( !WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief BOM이 포함된 UTF-8 형식으로 파일에 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @return 성공 시 true, 실패 시 false
bool SaveUTF8BOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || BufferSize == 0 ) return false;

	std::string strUtf8;

#ifdef _UNICODE
	if( UnicodeToUtf8_String(strUtf8, ptszBuffer, BufferSize) != 0 ) return false;
#else
	if( AnsiToUtf8_String(strUtf8, ptszBuffer, BufferSize) != 0 ) return false;
#endif

	// [수정] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제외
	if( !strUtf8.empty() && strUtf8.back() == '\0' )
	{
		strUtf8.pop_back();
	}

	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// UTF-8 BOM 작성
	char szBom[3] = { (char)UTF_FILE_IDENTIFIER_BYTE1, (char)UTF_FILE_IDENTIFIER_BYTE2, (char)UTF_FILE_IDENTIFIER_BYTE3 };
	DWORD dwWrittenSize = 0;
	if( !::WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
	{
		CloseHandle(hFile);
		return false;
	}

	const char* pszBuffer = strUtf8.data();
	const DWORD	dwTotFileSize = static_cast<DWORD>(strUtf8.size());
	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	while( dwWriteOffset < dwTotFileSize )
	{
		DWORD dwRemain = dwTotFileSize - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;

		if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief BOM이 없는 UTF-8 형식으로 파일에 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @return 성공 시 true, 실패 시 false
bool SaveUTF8NOBOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || BufferSize == 0 ) return false;

	std::string strUtf8;

#ifdef _UNICODE
	if( UnicodeToUtf8_String(strUtf8, ptszBuffer, BufferSize) != 0 ) return false;
#else
	if( AnsiToUtf8_String(strUtf8, ptszBuffer, BufferSize) != 0 ) return false;
#endif

	// [수정] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제외
	if( !strUtf8.empty() && strUtf8.back() == '\0' )
	{
		strUtf8.pop_back();
	}

	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const char* pszBuffer = strUtf8.data();
	const DWORD	dwTotFileSize = static_cast<DWORD>(strUtf8.size());
	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;
	DWORD		dwWrittenSize = 0;

	while( dwWriteOffset < dwTotFileSize )
	{
		DWORD dwRemain = dwTotFileSize - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;

		if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 인코딩 유형을 자동 감지하여 파일을 _tstring 변수로 읽어옵니다.
// @param destString 읽어온 문자열을 저장할 _tstring 참조
// @param ptszFullPath 읽어올 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
bool ReadFile(_tstring& destString, const TCHAR* ptszFullPath)
{
	int		i = 0;
	DWORD	dwLength = 0;
	DWORD	dwReadOffset = 0;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EEncoding	eFileType = EEncoding::DEFAULT;

	std::string		StrBuffer;
	std::wstring	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	eFileType = GetFileEncodingType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const DWORD dwMaxReadSize = MAX_BUFFER_SIZE;

	// 인코딩 타입에 맞추어 파일 포인터를 이동하고 버퍼 크기 조절 후 읽기 수행
	if( eFileType == EEncoding::UTF16_BE || eFileType == EEncoding::UTF16_LE )
	{
		SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);
		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.resize(dwLength / sizeof(wchar_t) + 1);
		pwszBuffer = WStrBuffer.data();

		while( dwReadOffset < dwLength )
		{
			DWORD dwRemain = dwLength - dwReadOffset;
			DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
			DWORD dwReadSize = 0;

			if( !ReadFile(hFile, (char*)pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
			{
				CloseHandle(hFile);
				return false;
			}

			dwReadOffset += dwReadSize;
		}

		CloseHandle(hFile);
	}
	else if( eFileType == EEncoding::ANSI || eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		DWORD dwSkip = (eFileType == EEncoding::UTF8_BOM) ? (sizeof(WORD) + sizeof(BYTE)) : 0;

		if( dwSkip > 0 )
		{
			SetFilePointer(hFile, dwSkip, nullptr, FILE_BEGIN);
			dwLength = dwLength - dwSkip;
		}

		StrBuffer.resize(dwLength + 1);
		pszBuffer = StrBuffer.data();

		while( dwReadOffset < dwLength )
		{
			DWORD dwRemain = dwLength - dwReadOffset;
			DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
			DWORD dwReadSize = 0;

			if( !ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
			{
				CloseHandle(hFile);
				return false;
			}

			dwReadOffset += dwReadSize;
		}

		pszBuffer[dwLength] = '\0';

		CloseHandle(hFile);
	}
	else
	{
		CloseHandle(hFile);
		return false;
	}

	if( (eFileType == EEncoding::UTF16_LE || eFileType == EEncoding::UTF16_BE) && pwszBuffer == nullptr ) return false;
	if( (eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM || eFileType == EEncoding::ANSI) && pszBuffer == nullptr ) return false;

	// 유니코드 혹은 멀티바이트(ANSI) 빌드 환경에 따른 문자열 변환 분기
#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		destString = pwszBuffer;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = SWAP16(*pwszBuffer);
			if( wcChar == 0xCDCD ) break;
			*pwszBuffer = wcChar;
			pwszBuffer++;
		}
		*pwszBuffer = L'\0';
		pwszBuffer -= i;

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
			wcChar = SWAP16(*pwszBuffer);
			if( wcChar == 0xCDCD ) break;
			*pwszBuffer = wcChar;
			pwszBuffer++;
		}
		*pwszBuffer = L'\0';
		pwszBuffer -= i;

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
// @brief 파일 맵핑을 활용해 다양한 인코딩의 파일을 읽어 _tstring으로 변환합니다.
// @param destString 읽어온 문자열이 저장될 변수 (_tstring)
// @param ptszFullPath 읽어올 파일의 전체 경로 (TCHAR*)
// @return 성공 시 true, 실패 시 false
// @note GetFileEncodingType 내부에서 ReadFileMap을 호출하는 상호 호출(무한 루프)을 
//       방지하기 위해, ReadFileMap 내부에서 직접 파일의 앞부분을 읽어 인코딩을 판별합니다.
bool ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath)
{
	bool		bIsProcess = false;
	DWORD		dwLength = 0;
	int			i = 0;
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	// [수정] 무한 루프(스택 오버플로우)를 방지하기 위해 GetFileEncodingType 대신 
	// 파일 핸들로 직접 BOM 및 인코딩을 판별합니다.
	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	char szBuffer[4] = { 0, };
	DWORD dwReadSize = 0;
	if( ReadFile(hFile, szBuffer, 3, &dwReadSize, NULL) )
	{
		if( (unsigned char)szBuffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_LE;
		}
		else if( (unsigned char)szBuffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_BE;
		}
		else if( (unsigned char)szBuffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && (unsigned char)szBuffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
		{
			eFileType = EEncoding::UTF8_BOM;
		}
		else
		{
			// BOM이 없는 경우 파일 전체를 읽어 판별
			dwLength = GetFileSize(hFile, nullptr);
			if( dwLength > 0 )
			{
				std::vector<char> tempBuffer(dwLength);
				SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
				if( ReadFile(hFile, tempBuffer.data(), dwLength, &dwReadSize, NULL) )
				{
					if( IsUTF8WithoutBom((const void*)tempBuffer.data(), dwReadSize) )
						eFileType = EEncoding::UTF8_NOBOM;
					else
						eFileType = EEncoding::ANSI;
				}
			}
			else
			{
				eFileType = EEncoding::ANSI;
			}
		}
	}
	CloseHandle(hFile);

	// 파일 맵핑 오픈 재수행
	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	dwLength = GetFileSize(hFile, nullptr);

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
		std::wstring temp((wchar_t*)lpvFile + 1);
		wchar_t* pwszBuffer = temp.data();

		for( i = 0; *pwszBuffer; i++ )
		{
			wchar_t wcChar = SWAP16(*pwszBuffer);
			if( wcChar == 0xCDCD ) break;
			*pwszBuffer = wcChar;
			pwszBuffer++;
		}
		*pwszBuffer = L'\0';
		pwszBuffer -= i;

		destString = pwszBuffer;
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
		std::wstring temp((wchar_t*)lpvFile + 1);
		wchar_t* pwszBuffer = temp.data();

		for( i = 0; *pwszBuffer; i++ )
		{
			wchar_t wcChar = SWAP16(*pwszBuffer);
			if( wcChar == 0xCDCD ) break;
			*pwszBuffer = wcChar;
			pwszBuffer++;
		}
		*pwszBuffer = L'\0';
		pwszBuffer -= i;

		if( UnicodeToAnsi_String(destString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) bIsProcess = false;
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
	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}

//***************************************************************************
// @brief 바이트 버퍼의 데이터를 지정된 크기만큼 파일로 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param pbBuffer 저장할 바이트 버퍼 포인터
// @param dwLength 저장할 데이터 크기 (바이트 단위)
// @return 성공 시 true, 실패 시 false
bool SaveFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength)
{
	HANDLE hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	while( dwWriteOffset < dwLength )
	{
		DWORD dwRemain = dwLength - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;
		DWORD dwWrittenSize = 0;

		if( !WriteFile(hFile, pbBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 파일의 생성, 접근, 마지막 수정 시간 중 선택한 정보를 시스템 타임(SYSTEMTIME) 형태로 가져옵니다.
// @param ptszFullPath 조회할 파일의 전체 경로
// @param nCase 조회할 시간 종류 (생성/접근/수정 시간 식별자)
// @param stLocal 조회된 로컬 시간을 저장할 SYSTEMTIME 구조체 참조
// @return 성공 시 true, 실패 시 false
bool GetFileInfoTime(const TCHAR* ptszFullPath, const int nCase, SYSTEMTIME& stLocal)
{
	FILETIME ftCreate, ftAccess, ftWrite;
	SYSTEMTIME stUTC;

	HANDLE hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return false;

	// 파일 시간 정보 획득
	if( !GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite) )
	{
		CloseHandle(hFile);
		return false;
	}

	CloseHandle(hFile);

	// 요청된 케이스에 따라 변환할 타임 선택
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
// @brief 지정한 경로에 파일이 실제로 존재하는지 확인합니다.
// @param ptszFullPath 존재 여부를 확인할 파일의 전체 경로
// @return 파일이 존재하면 true, 아니면 false
bool IsExistFile(const TCHAR* ptszFullPath)
{
	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 파일의 크기를 32비트 DWORD 값으로 반환합니다.
// @param ptszFullPath 크기를 조회할 파일의 전체 경로
// @return 파일 크기 (바이트), 실패 시 0
DWORD GetFileSize(const TCHAR* ptszFullPath)
{
	DWORD		dwFileSizeLow = 0;
	DWORD		dwFileSizeHigh = 0;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return 0;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return 0;

	dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);

	CloseHandle(hFile);

	return dwFileSizeLow;
}

//***************************************************************************
// @brief 파일 핸들 정보를 기반으로 상세 파일 정보를 조회합니다.
// @param ptszFullPath 조회할 파일의 전체 경로
// @param lpFileInformation 상세 정보를 저장할 BY_HANDLE_FILE_INFORMATION 구조체 포인터
// @return 성공 시 true, 실패 시 false
bool GetFileInformation(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	bool		bResult = false;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	bResult = GetFileInformationByHandle(hFile, lpFileInformation);

	CloseHandle(hFile);

	return bResult;
}
#endif // _WIN32

//***************************************************************************
// @brief C++ 표준 파일 스트림을 사용하여 파일의 인코딩 타입을 판별합니다.
// @param filepath 판별할 파일의 전체 경로 (_tstring)
// @return 판별된 인코딩 타입 (EEncoding 열거형)
EEncoding GetFileEncodingType(const _tstring& filepath)
{
	EEncoding	eEncoding = EEncoding::DEFAULT;

	constexpr size_t BufferSize = 4096;
	std::ifstream file(filepath, std::ios::binary);
	if( !file )
	{
		return eEncoding;
	}

	std::vector<unsigned char> buffer(BufferSize);
	file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
	size_t bytesRead = static_cast<size_t>(file.gcount());

	// 파일이 너무 작아 BOM을 확인할 수 없는 경우 (기본값 ANSI 또는 DEFAULT 처리)
	if( bytesRead == 0 )
	{
		return EEncoding::ANSI; // 또는 EEncoding::DEFAULT
	}

	// BOM 및 내용 분석을 통한 인코딩 판별 (bytesRead 크기 검사 추가)
	if( bytesRead >= 2 && buffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && buffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		eEncoding = EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN)
	}
	else if( bytesRead >= 2 && buffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && buffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		eEncoding = EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		if( bytesRead >= 3 && buffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && buffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && buffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
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
// @brief C++ 표준 스트림을 활용해 다양한 인코딩의 파일을 읽어 _tstring으로 변환합니다.
// @param filepath 읽어올 파일의 전체 경로 (_tstring)
// @return 읽어온 문자열 내용 (_tstring)
// @note 변환 과정에서 널 문자('\0')가 포함되어 반환될 경우 텍스트 에디터에서 
//       바이너리로 오인하는 문제가 발생할 수 있으므로, 반환 직전 문자열 끝의 널 문자를 제거합니다
_tstring ReadFile(const _tstring& filepath)
{
	ifstream file(filepath, std::ios::binary);
	if( !file )
	{
		return _T("");
	}

	// 파일 전체를 한 번에 읽기 위해 파일 크기 측정
	file.seekg(0, std::ios::end);
	std::streamoff fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	if( fileSize < 0 )
	{
		return _T("");
	}

	std::vector<char> buffer(static_cast<size_t>(fileSize));
	if( fileSize > 0 )
	{
		file.read(buffer.data(), fileSize);
	}

	if( buffer.size() < 2 )
	{
		throw std::runtime_error("File too small to contain valid UTF-16 data");
	}

#ifdef _UNICODE
	std::wstring result;

	// BOM 확인 후 변환
	if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_LE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		// UTF-16LE BOM
		result.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			// Little Endian: LSB + MSB
			wchar_t codeUnit = static_cast<unsigned char>(buffer[i]) | (static_cast<unsigned char>(buffer[i + 1]) << 8);
			result.push_back(codeUnit);
		}
	}
	else if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_BE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		result.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			// Big Endian -> Little Endian: MSB + LSB
			wchar_t codeUnit = (static_cast<unsigned char>(buffer[i]) << 8) | static_cast<unsigned char>(buffer[i + 1]);
			result.push_back(codeUnit);
		}
	}
	else if( buffer.size() >= 3 && static_cast<unsigned char>(buffer[0]) == UTF_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UTF_FILE_IDENTIFIER_BYTE2
		&& static_cast<unsigned char>(buffer[2]) == UTF_FILE_IDENTIFIER_BYTE3 )
	{
		std::wstring wtemp;
		Utf8ToUnicode_String(wtemp, std::string(buffer.begin() + 3, buffer.end()).c_str(), buffer.size() - 3 + 1);
		result = wtemp;
	}
	else
	{
		// 이미 읽어둔 buffer로 바로 판별 (파일을 다시 열어 재판독하지 않음)
		if( IsUTF8WithoutBom((const void*)buffer.data(), buffer.size()) )
		{
			std::wstring wtemp;
			Utf8ToUnicode_String(wtemp, std::string(buffer.begin(), buffer.end()).c_str(), buffer.size() + 1);
			result = wtemp;
		}
		else
		{
			std::wstring wtemp;
			AnsiToUnicode_String(wtemp, std::string(buffer.begin(), buffer.end()).c_str(), buffer.size() + 1);
			result = wtemp;
		}
	}

	// [수정] 결과 문자열 끝에 널 문자가 포함되어 있다면 제거
	if( !result.empty() && result.back() == L'\0' )
	{
		result.pop_back();
	}

#else
	std::string result;

	// BOM 확인 후 변환
	if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_LE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		// UTF-16LE BOM
		std::wstring temp;
		temp.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			// Little Endian: LSB + MSB
			wchar_t codeUnit = static_cast<unsigned char>(buffer[i]) | (static_cast<unsigned char>(buffer[i + 1]) << 8);
			temp.push_back(codeUnit);
		}

		std::string atemp;
		UnicodeToAnsi_String(atemp, temp.data(), temp.size() + 1);
		result = atemp;
	}
	else if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_BE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		// UTF-16BE BOM
		std::wstring temp;
		temp.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			// Big Endian -> Little Endian: MSB + LSB
			wchar_t codeUnit = (static_cast<unsigned char>(buffer[i]) << 8) | static_cast<unsigned char>(buffer[i + 1]);
			temp.push_back(codeUnit);
		}

		std::string atemp;
		UnicodeToAnsi_String(atemp, temp.data(), temp.size() + 1);
		result = atemp;
	}
	else if( buffer.size() >= 3 && static_cast<unsigned char>(buffer[0]) == UTF_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UTF_FILE_IDENTIFIER_BYTE2
		&& static_cast<unsigned char>(buffer[2]) == UTF_FILE_IDENTIFIER_BYTE3 )
	{
		std::string atemp;
		Utf8ToAnsi_String(atemp, std::string(buffer.begin() + 3, buffer.end()).c_str(), buffer.size() - 3 + 1);
		result = atemp;
	}
	else
	{
		// 이미 읽어둔 buffer로 바로 판별 (파일을 다시 열어 재판독하지 않음)
		if( IsUTF8WithoutBom((const void*)buffer.data(), buffer.size()) )
		{
			std::string atemp;
			Utf8ToAnsi_String(atemp, std::string(buffer.begin(), buffer.end()).c_str(), buffer.size() + 1);
			result = atemp;
		}
		else
		{
			result = std::string(buffer.begin(), buffer.end());
		}
	}

	// [수정] 결과 문자열 끝에 널 문자가 포함되어 있다면 제거
	if( !result.empty() && result.back() == '\0' )
	{
		result.pop_back();
	}
#endif

	return result;
}

//***************************************************************************
// @brief 지정한 인코딩 타입에 맞춰 _tstring 문자열을 파일에 기록합니다.
// @param filepath 저장할 파일의 전체 경로 (_tstring)
// @param content 파일에 쓸 문자열 내용 (_tstring)
// @param encoding 저장할 인코딩 타입 (EEncoding 열거형)
// @return 성공 시 true, 실패 시 false
// @note 문자열 변환 시 널 문자('\0')가 파일에 함께 기록되면 텍스트 에디터에서 
//       "지원하지 않는 텍스트 인코딩" 또는 바이너리 파일 경고가 발생할 수 있으므로 
//       순수 텍스트 크기만큼만 파일에 기록되도록 처리합니다
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
		// UTF-16 LE BOM (content.size()는 널 문자를 포함하지 않으므로 그대로 사용)
		unsigned char bom[] = { UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		file.write(reinterpret_cast<const char*>(content.data()), content.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		// UTF-8 BOM
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		string dest;
		UnicodeToUtf8_String(dest, content.data(), content.size() + 1);

		// [수정] 널 문자를 제외한 순수 바이트 수만큼만 기록
		size_t writeSize = (!dest.empty() && dest.back() == '\0') ? dest.size() - 1 : dest.size();
		file.write(dest.c_str(), writeSize);
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		string dest;
		UnicodeToUtf8_String(dest, content.data(), content.size() + 1);

		// [수정] 널 문자를 제외한 순수 바이트 수만큼만 기록
		size_t writeSize = (!dest.empty() && dest.back() == '\0') ? dest.size() - 1 : dest.size();
		file.write(dest.c_str(), writeSize);
	}
	else
	{
		string dest;
		UnicodeToAnsi_String(dest, content.data(), content.size() + 1);

		// [수정] 널 문자를 제외한 순수 바이트 수만큼만 기록
		size_t writeSize = (!dest.empty() && dest.back() == '\0') ? dest.size() - 1 : dest.size();
		file.write(dest.c_str(), writeSize);
	}
#else
	// 멀티바이트 빌드 영역도 동일하게 적용
	if( encoding == EEncoding::UTF16_BE )
	{
		unsigned char bom[] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		wstring dest;
		AnsiToUnicode_String(dest, content.data(), content.size() + 1);
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
		unsigned char bom[] = { UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		wstring dest;
		AnsiToUnicode_String(dest, content.data(), content.size() + 1);
		file.write(reinterpret_cast<const char*>(dest.data()), dest.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		string dest;
		AnsiToUtf8_String(dest, content.data(), content.size() + 1);
		size_t writeSize = (!dest.empty() && dest.back() == '\0') ? dest.size() - 1 : dest.size();
		file.write(dest.c_str(), writeSize);
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		string dest;
		AnsiToUtf8_String(dest, content.data(), content.size() + 1);
		size_t writeSize = (!dest.empty() && dest.back() == '\0') ? dest.size() - 1 : dest.size();
		file.write(dest.c_str(), dest.size());
	}
	else
	{
		// [수정] ANSI 저장 시에도 content 끝에 널 문자가 포함되어 있다면 제외하고 기록
		size_t writeSize = (!content.empty() && content.back() == _T('\0')) ? content.size() - 1 : content.size();
		file.write(content.c_str(), writeSize);
	}
#endif

	file.close();

	return true;
}

//***************************************************************************
// @brief std::filesystem을 사용하여 파일 존재 여부를 확인합니다.
// @param filepath 존재 여부를 확인할 파일의 전체 경로 (_tstring)
// @return 파일이 존재하면 true, 아니면 false
bool IsExistFile(const _tstring& filepath)
{
	std::error_code ec;
	bool bExists = std::filesystem::exists(filepath, ec);

	// ec가 설정된 경우는 "파일 없음"이 아니라 접근 권한 등 실제 조회 실패 상황
	if( ec )
	{
		std::cerr << "파일 존재 여부 확인 중 오류 발생: " << ec.message() << std::endl;
		return false;
	}

	return bExists;
}

//***************************************************************************
// @brief std::filesystem을 사용하여 파일 크기를 바이트 단위로 반환합니다.
// @param filepath 크기를 조회할 파일의 전체 경로 (_tstring)
// @return 파일 크기 (바이트 단위), 실패 시 static_cast<std::uintmax_t>(-1)
std::uintmax_t GetFileSize(const _tstring& filepath)
{
	std::error_code ec;
	std::uintmax_t size = std::filesystem::file_size(filepath, ec);

	if( ec )
	{
		std::cerr << "파일 크기 확인 중 오류 발생: " << ec.message() << std::endl;
		return static_cast<std::uintmax_t>(-1);
	}

	return size;
}