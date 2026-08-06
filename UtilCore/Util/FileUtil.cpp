
//***************************************************************************
// FileUtil.cpp : implementation of the FileUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "FileUtil.h"

//***************************************************************************
// @brief 바이트 배열이 BOM이 없는 UTF-8 인코딩 조건을 만족하는지 검사합니다.
// @param pBuffer 검사할 데이터 버퍼
// @param BuffSize 버퍼 크기
// @return UTF-8 조건을 만족하면 true, 아니면 false
//***************************************************************************
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
		else if( *start < (0xC0) )	// 잘못된 시작 바이트 (10xxxxxx)
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
		else if( *start < (0xF8) )	// 4바이트 문자 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
		{
			if( start >= end - 3 )
				break;
			if( (start[1] & (0xC0)) != 0x80 || (start[2] & (0xC0)) != 0x80 || (start[3] & (0xC0)) != 0x80 )
			{
				bUTF8 = false;
				break;
			}
			start += 4;
		}
		else						// 5바이트 이상 혹은 지원하지 않는 바이트
		{
			bUTF8 = false;
			break;
		}
	}
	return bUTF8;
}

#ifdef _WIN32
// --- 익명 네임스페이스 시작 ---
// 이 안에 선언된 함수들은 오직 이 FileUtil.cpp 파일 안에서만 접근할 수 있습니다.
namespace {
	//***************************************************************************
	// @brief 이미 열려 있는 파일 핸들로부터 인코딩 타입을 판별하는 공용 헬퍼입니다.
	//        GetFileEncodingType(TCHAR*)와 ReadFileMap(_tstring&, TCHAR*)이 이 함수를
	//        공유하여 BOM/휴리스틱 판별 로직이 두 곳에서 따로 구현되지 않도록 합니다.
	// @param hFile 읽기 권한으로 이미 열려 있는 파일 핸들
	// @return 판별된 인코딩 타입. 판별 실패(읽기 실패 등) 시 EEncoding::DEFAULT
	// @note 호출 후 파일 포인터는 항상 파일 시작(오프셋 0)으로 되돌려 놓습니다.
	//       핸들의 오픈/클로즈는 호출자 책임입니다.
	//***************************************************************************
	EEncoding DetectFileEncoding(HANDLE hFile)
	{
		EEncoding	eFileType = EEncoding::DEFAULT;
		char		szBuffer[4] = { 0, };
		DWORD		dwReadSize = 0;

		// [주의] Win32 API인 ReadFile을 명확히 호출하기 위해 앞에 '::'를 붙여 충돌을 방지합니다.
		if( !::ReadFile(hFile, szBuffer, 3, &dwReadSize, NULL) || dwReadSize < 2 ) // 최소 2바이트(BOM)는 읽어야 함
		{
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
			return eFileType; // 판별 실패: DEFAULT 반환 (호출자가 실패로 처리해야 함)
		}
		szBuffer[3] = '\0';

		// BOM 시그니처로 먼저 인코딩 여부 읽기
		if( (unsigned char)szBuffer[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN)
		}
		else if( (unsigned char)szBuffer[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
		}
		else if( dwReadSize >= 3 && (unsigned char)szBuffer[0] == UTF_FILE_IDENTIFIER_BYTE1 && (unsigned char)szBuffer[1] == UTF_FILE_IDENTIFIER_BYTE2 && (unsigned char)szBuffer[2] == UTF_FILE_IDENTIFIER_BYTE3 )
		{
			eFileType = EEncoding::UTF8_BOM;	// UTF8_BOM
		}
		else
		{
			// BOM이 없는 경우 파일 전체를 읽어 UTF-8(Without BOM)인지 판별
			DWORD dwFileSize = ::GetFileSize(hFile, nullptr);
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

			if( dwFileSize > 0 )
			{
				std::vector<char> byteDestination(dwFileSize);
				DWORD dwBytesRead = 0;
				if( ::ReadFile(hFile, byteDestination.data(), dwFileSize, &dwBytesRead, NULL) )
				{
					eFileType = IsUTF8WithoutBom((const void*)byteDestination.data(), dwBytesRead)
						? EEncoding::UTF8_NOBOM	// UTF8_NOBOM
						: EEncoding::ANSI;			// ANSI
				}
			}
			else
			{
				eFileType = EEncoding::ANSI;
			}
		}

		::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

		return eFileType;
	}

	//***************************************************************************
	// @brief ANSI 형식으로 문자열 데이터를 파일에 저장합니다.
	// @param ptszFullPath 저장할 파일의 전체 경로
	// @param ptszBuffer 저장할 문자열 버퍼 포인터
	// @param BufferSize 버퍼 크기
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
	bool SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
	{
		if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
		if( ptszBuffer == nullptr || BufferSize == 0 ) return false;

		std::string strAnsi;

#ifdef _UNICODE
		// [주의] 널 문자를 포함하여 변환했다면, 아래에서 널 문자를 제거하고 파일에 씁니다.
		if( UnicodeToAnsi_String(strAnsi, ptszBuffer, BufferSize) != 0 ) return false;
#else
		strAnsi.assign(ptszBuffer, BufferSize);
#endif

		// [주의] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제거
		if( !strAnsi.empty() && strAnsi.back() == '\0' )
		{
			strAnsi.pop_back();
		}

		// 파일 쓰기 위한 핸들 오픈
		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return false;

		const char* pszBuffer = strAnsi.data();
		const DWORD	dwTotFileSize = static_cast<DWORD>(strAnsi.size());
		const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
		DWORD		dwWriteOffset = 0;

		// 나누어서 파일 쓰기 수행
		while( dwWriteOffset < dwTotFileSize )
		{
			DWORD dwRemain = dwTotFileSize - dwWriteOffset;
			DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;
			DWORD dwWrittenSize = 0;

			if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
			{
				::CloseHandle(hFile);
				return false;
			}

			dwWriteOffset += dwWrittenSize;
		}

		::CloseHandle(hFile);

		return true;
	}

	//***************************************************************************
	// @brief 유니코드 Big Endian(UTF-16 BE) 형식으로 파일에 저장합니다.
	// @param ptszFullPath 저장할 파일의 전체 경로
	// @param ptszBuffer 저장할 문자열 버퍼 포인터
	// @param BufferSize 버퍼 크기
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
	bool SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
	{
		if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
		if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

		std::wstring strUnicode;

#ifdef _UNICODE
		strUnicode = ptszBuffer;
#else
		if( AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

		// Big Endian으로 바이트 순서 변경
		std::wstring strBE;
		strBE.reserve(strUnicode.size());
		for( wchar_t ch : strUnicode )
		{
			strBE.push_back(SWAP16(ch));
		}

		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return false;

		// UTF-16 BE BOM 작성
		char szBom[2] = { (char)UNICODE_BE_FILE_IDENTIFIER_BYTE1, (char)UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		DWORD dwWrittenSize = 0;
		if( !::WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
		{
			::CloseHandle(hFile);
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

			if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
			{
				::CloseHandle(hFile);
				return false;
			}

			dwWriteOffset += dwWrittenSize;
		}

		::CloseHandle(hFile);

		return true;
	}

	//***************************************************************************
	// @brief 유니코드 Little Endian(UTF-16 LE) 형식으로 파일에 저장합니다.
	// @param ptszFullPath 저장할 파일의 전체 경로
	// @param ptszBuffer 저장할 문자열 버퍼 포인터
	// @param BufferSize 버퍼 크기
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
	bool SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
	{
		if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
		if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

		std::wstring strUnicode;

#ifdef _UNICODE
		strUnicode = ptszBuffer;
#else
		if( AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return false;

		// UTF-16 LE BOM 작성
		char szBom[2] = { (char)UNICODE_LE_FILE_IDENTIFIER_BYTE1, (char)UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		DWORD dwWrittenSize = 0;
		if( !::WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
		{
			::CloseHandle(hFile);
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

			if( !::WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
			{
				::CloseHandle(hFile);
				return false;
			}

			dwWriteOffset += dwWrittenSize;
		}

		::CloseHandle(hFile);

		return true;
	}

	//***************************************************************************
	// @brief BOM이 포함된 UTF-8 형식으로 파일에 저장합니다.
	// @param ptszFullPath 저장할 파일의 전체 경로
	// @param ptszBuffer 저장할 문자열 버퍼 포인터
	// @param BufferSize 버퍼 크기
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
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

		// [주의] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제거
		if( !strUtf8.empty() && strUtf8.back() == '\0' )
		{
			strUtf8.pop_back();
		}

		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return false;

		// UTF-8 BOM 작성
		char szBom[3] = { (char)UTF_FILE_IDENTIFIER_BYTE1, (char)UTF_FILE_IDENTIFIER_BYTE2, (char)UTF_FILE_IDENTIFIER_BYTE3 };
		DWORD dwWrittenSize = 0;
		if( !::WriteFile(hFile, szBom, sizeof(szBom), &dwWrittenSize, NULL) )
		{
			::CloseHandle(hFile);
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
				::CloseHandle(hFile);
				return false;
			}

			dwWriteOffset += dwWrittenSize;
		}

		::CloseHandle(hFile);

		return true;
	}

	//***************************************************************************
	// @brief BOM이 없는 UTF-8 형식으로 파일에 저장합니다.
	// @param ptszFullPath 저장할 파일의 전체 경로
	// @param ptszBuffer 저장할 문자열 버퍼 포인터
	// @param BufferSize 버퍼 크기
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
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

		// [주의] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제거
		if( !strUtf8.empty() && strUtf8.back() == '\0' )
		{
			strUtf8.pop_back();
		}

		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
				::CloseHandle(hFile);
				return false;
			}

			dwWriteOffset += dwWrittenSize;
		}

		::CloseHandle(hFile);

		return true;
	}
}

//***************************************************************************
// @brief 파일 경로를 받아 Win32 API 방식으로 파일의 인코딩 타입(UTF-16, UTF-8, ANSI 등)을 판별합니다.
// @param ptszFullPath 파일 전체 경로
// @return 판별된 인코딩 타입 (EEncoding 열거형)
//***************************************************************************
EEncoding GetFileEncodingType(const TCHAR* ptszFullPath)
{
	EEncoding eFileType = EEncoding::DEFAULT;

	// 파일 읽기 전용으로 오픈
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return eFileType;

	eFileType = DetectFileEncoding(hFile);

	::CloseHandle(hFile);

	return eFileType;
}

//***************************************************************************
// @brief Win32 API를 사용하여 파일을 바이너리 형태로 읽어들입니다.
// @param byteDestination 읽어들인 데이터를 저장할 바이트 벡터 참조
// @param ptszFullPath 읽어들일 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool ReadFile(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	DWORD dwLength = GetFileSize(ptszFullPath);

	// 파일 핸들 오픈
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// 파일 크기만큼 저장 공간 확보
	byteDestination.resize(dwLength);

	const DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD		dwReadOffset = 0;
	BYTE* pbBuffer = byteDestination.data();

	// 최대 크기 단위로 나누어 분할 읽기 수행
	while( dwReadOffset < dwLength )
	{
		DWORD dwRemain = dwLength - dwReadOffset;
		DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
		DWORD dwReadSize = 0;

		if( !::ReadFile(hFile, pbBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
		{
			::CloseHandle(hFile);
			return false;
		}

		dwReadOffset += dwReadSize;	// 실제로 읽은 만큼만 증가 (부분 읽기 대응)
	}

	::CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 메모리 맵(Memory Map) 기법을 이용하여 파일을 전체적으로 읽어 버퍼에 저장합니다.
// @param byteDestination 읽어들인 데이터를 저장할 바이트 벡터 참조
// @param ptszFullPath 읽어들일 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool ReadFileMap(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	dwLength = GetFileSize(ptszFullPath);

	// 파일 매핑 객체 생성
	hFileMap = ::CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 뷰 생성하여 메모리 주소 획득
	lpvFile = ::MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);
	if( lpvFile == nullptr )
	{
		::CloseHandle(hFile);
		::CloseHandle(hFileMap);
		return false;
	}

	// 파일 크기를 맞추고 메모리 복사 수행
	byteDestination.resize(dwLength);
	memcpy(byteDestination.data(), lpvFile, dwLength);

	// 리소스 해제
	::UnmapViewOfFile(lpvFile);
	::CloseHandle(hFile);
	::CloseHandle(hFileMap);

	return true;
}

//***************************************************************************
// @brief 바이트 버퍼 데이터를 지정한 크기만큼 파일로 저장합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param pbBuffer 저장할 바이트 버퍼 포인터
// @param dwLength 저장할 데이터 크기 (바이트 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool WriteFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength)
{
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD		dwWriteOffset = 0;

	while( dwWriteOffset < dwLength )
	{
		DWORD dwRemain = dwLength - dwWriteOffset;
		DWORD dwWriteSize = (dwRemain > dwMaxWriteSize) ? dwMaxWriteSize : dwRemain;
		DWORD dwWrittenSize = 0;

		if( !::WriteFile(hFile, pbBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
		{
			::CloseHandle(hFile);
			return false;
		}

		dwWriteOffset += dwWrittenSize;
	}

	::CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 인코딩 종류에 관계없이 파일을 자동 감지하여 _tstring 형태로 읽어들입니다.
// @param destString 읽어들인 문자열을 저장할 _tstring 참조
// @param ptszFullPath 읽어들일 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool ReadFile(_tstring& destString, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	DWORD	dwReadOffset = 0;
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EEncoding	eFileType = EEncoding::DEFAULT;

	std::string		StrBuffer;
	std::wstring	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	eFileType = GetFileEncodingType(ptszFullPath);

	hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const DWORD dwMaxReadSize = MAX_BUFFER_SIZE;

	// 인코딩 타입에 맞추어 시작 포인터를 이동하고 남은 크기 만큼 읽기 수행
	if( eFileType == EEncoding::UTF16_BE || eFileType == EEncoding::UTF16_LE )
	{
		::SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);
		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.resize(dwLength / sizeof(wchar_t) + 1);
		pwszBuffer = WStrBuffer.data();

		while( dwReadOffset < dwLength )
		{
			DWORD dwRemain = dwLength - dwReadOffset;
			DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
			DWORD dwReadSize = 0;

			if( !::ReadFile(hFile, (char*)pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
			{
				::CloseHandle(hFile);
				return false;
			}

			dwReadOffset += dwReadSize;
		}

		::CloseHandle(hFile);
	}
	else if( eFileType == EEncoding::ANSI || eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM )
	{
		DWORD dwSkip = (eFileType == EEncoding::UTF8_BOM) ? (sizeof(WORD) + sizeof(BYTE)) : 0;

		if( dwSkip > 0 )
		{
			::SetFilePointer(hFile, dwSkip, nullptr, FILE_BEGIN);
			dwLength = dwLength - dwSkip;
		}

		StrBuffer.resize(dwLength + 1);
		pszBuffer = StrBuffer.data();

		while( dwReadOffset < dwLength )
		{
			DWORD dwRemain = dwLength - dwReadOffset;
			DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
			DWORD dwReadSize = 0;

			if( !::ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
			{
				::CloseHandle(hFile);
				return false;
			}

			dwReadOffset += dwReadSize;
		}

		pszBuffer[dwLength] = '\0';

		::CloseHandle(hFile);
	}
	else
	{
		::CloseHandle(hFile);
		return false;
	}

	if( (eFileType == EEncoding::UTF16_LE || eFileType == EEncoding::UTF16_BE) && pwszBuffer == nullptr ) return false;
	if( (eFileType == EEncoding::UTF8_BOM || eFileType == EEncoding::UTF8_NOBOM || eFileType == EEncoding::ANSI) && pszBuffer == nullptr ) return false;

	// 유니코드 혹은 멀티바이트(ANSI) 빌드 환경에 따라 문자열 변환 분기
#ifdef _UNICODE
	if( eFileType == EEncoding::UTF16_LE )
	{
		destString = pwszBuffer;
	}
	else if( eFileType == EEncoding::UTF16_BE )
	{
		wchar_t* p = pwszBuffer;
		while( *p )
		{
			*p = SWAP16(*p);
			p++;
		}

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
		wchar_t* p = pwszBuffer;
		while( *p )
		{
			*p = SWAP16(*p);
			p++;
		}

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
// @brief 파일 매핑을 활용해 다양한 인코딩의 파일을 읽어 _tstring으로 변환합니다.
// @param destString 읽어들인 문자열이 저장될 참조 (_tstring)
// @param ptszFullPath 읽어들일 파일의 전체 경로 (TCHAR*)
// @return 성공 시 true, 실패 시 false
// @note 판별과 매핑에 동일한 파일 핸들을 재사용하여(파일을 두 번 열지 않음) 그 사이의
//       TOCTOU 창을 최소화했습니다. BOM/휴리스틱 판별 로직은 GetFileEncodingType(TCHAR*)와
//       공유하는 DetectFileEncoding() 헬퍼를 사용하며, 판별에 실패(EEncoding::DEFAULT)하면
//       매핑을 시도하지 않고 즉시 실패로 처리합니다.
//***************************************************************************
bool ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath)
{
	bool		bIsProcess = false;
	DWORD		dwLength = 0;
	EEncoding	eFileType = EEncoding::DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	eFileType = DetectFileEncoding(hFile);
	if( eFileType == EEncoding::DEFAULT )
	{
		// 인코딩 판별 실패(최소 BOM 크기를 읽지 못함 등) - 매핑을 시도하지 않고 실패 처리
		::CloseHandle(hFile);
		return false;
	}

	dwLength = ::GetFileSize(hFile, nullptr);

	// [최적화] CreateFileMapping은 파일 포인터 위치와 무관하게 동작하므로
	// 위에서 판별에 사용한 핸들을 재오픈 없이 그대로 매핑에 사용합니다.
	hFileMap = ::CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		::CloseHandle(hFile);
		return false;
	}

	lpvFile = ::MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);
	if( lpvFile == nullptr )
	{
		::CloseHandle(hFile);
		::CloseHandle(hFileMap);
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
		for( wchar_t& ch : temp )
		{
			ch = SWAP16(ch);
		}

		destString = temp;
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
		for( wchar_t& ch : temp )
		{
			ch = SWAP16(ch);
		}

		if( UnicodeToAnsi_String(destString, temp.data(), temp.size() + 1) != 0 ) bIsProcess = false;
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

	::UnmapViewOfFile(lpvFile);
	::CloseHandle(hFile);
	::CloseHandle(hFileMap);

	return bIsProcess;
}

//***************************************************************************
// @brief 지정한 인코딩 타입(EEncoding)에 따라 알맞은 저장 함수를 분기 호출합니다.
// @param ptszFullPath 저장할 파일의 전체 경로
// @param ptszBuffer 저장할 문자열 버퍼 포인터
// @param BufferSize 버퍼 크기
// @param fileType 저장할 인코딩 타입 (EEncoding 열거형)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool WriteFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize, EEncoding fileType)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || BufferSize == 0 ) return false;

	bool bResult = false;

	switch( fileType )
	{
	case EEncoding::ANSI:
		bResult = SaveAnsiFile(ptszFullPath, ptszBuffer, BufferSize);
		break;

	case EEncoding::UTF16_BE:
		bResult = SaveUnicodeBEFile(ptszFullPath, ptszBuffer, BufferSize);
		break;

	case EEncoding::UTF16_LE:
		bResult = SaveUnicodeLEFile(ptszFullPath, ptszBuffer, BufferSize);
		break;

	case EEncoding::UTF8_BOM:
		bResult = SaveUTF8BOMFile(ptszFullPath, ptszBuffer, BufferSize);
		break;

	case EEncoding::UTF8_NOBOM:
		bResult = SaveUTF8NOBOMFile(ptszFullPath, ptszBuffer, BufferSize);
		break;

	case EEncoding::DEFAULT:
	default:
		// 기본값일 경우 프로젝트 환경(ANSI 또는 유니코드)에 맞춰 ANSI 혹은 기본 저장 정책으로 처리
		bResult = SaveAnsiFile(ptszFullPath, ptszBuffer, BufferSize);
		break;
	}

	return bResult;
}

//***************************************************************************
// @brief 파일의 생성, 접근, 마지막 수정 시각 중 지정한 종류를 시스템 타임(SYSTEMTIME) 형태로 가져옵니다.
// @param ptszFullPath 조회할 파일의 전체 경로
// @param nCase 조회할 시각 종류 (생성/접근/수정 시각 식별자)
// @param stLocal 조회한 로컬 시각을 저장할 SYSTEMTIME 구조체 참조
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool GetFileInfoTime(const TCHAR* ptszFullPath, const int nCase, SYSTEMTIME& stLocal)
{
	FILETIME ftCreate, ftAccess, ftWrite;
	SYSTEMTIME stUTC;

	HANDLE hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return false;

	// 파일 시각 정보 획득
	if( !::GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite) )
	{
		::CloseHandle(hFile);
		return false;
	}

	::CloseHandle(hFile);

	// 요청한 케이스에 따라 반환할 타임 선택
	if( nCase == FILEINFO_CREATETIME )
		::FileTimeToSystemTime(&ftCreate, &stUTC);
	else if( nCase == FILEINFO_ACCESSTIME )
		::FileTimeToSystemTime(&ftAccess, &stUTC);
	else if( nCase == FILEINFO_LASTWRITETIME )
		::FileTimeToSystemTime(&ftWrite, &stUTC);
	else return false;

	::SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

	return true;
}

//***************************************************************************
// @brief 지정한 경로에 파일이 존재하는지 확인합니다.
// @param ptszFullPath 존재 여부를 확인할 파일의 전체 경로
// @return 파일이 존재하면 true, 아니면 false
//***************************************************************************
bool IsExistFile(const TCHAR* ptszFullPath)
{
	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	::CloseHandle(hFile);

	return true;
}

//***************************************************************************
// @brief 파일 크기를 32비트 DWORD 값으로 반환합니다.
// @param ptszFullPath 크기를 조회할 파일의 전체 경로
// @return 파일 크기 (바이트), 실패 시 0
//***************************************************************************
DWORD GetFileSize(const TCHAR* ptszFullPath)
{
	DWORD		dwFileSizeLow = 0;
	DWORD		dwFileSizeHigh = 0;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return 0;

	hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return 0;

	dwFileSizeLow = ::GetFileSize(hFile, &dwFileSizeHigh);

	::CloseHandle(hFile);

	return dwFileSizeLow;
}

//***************************************************************************
// @brief 파일 핸들 정보를 사용하여 상세 파일 정보를 조회합니다.
// @param ptszFullPath 조회할 파일의 전체 경로
// @param lpFileInformation 조회 정보를 저장할 BY_HANDLE_FILE_INFORMATION 구조체 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool GetFileInformation(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	bool		bResult = false;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	bResult = ::GetFileInformationByHandle(hFile, lpFileInformation);

	::CloseHandle(hFile);

	return bResult;
}
#endif // _WIN32

//***************************************************************************
// @brief C++ 표준 파일 스트림을 사용하여 파일의 인코딩 타입을 판별합니다.
// @param filepath 판별할 파일의 전체 경로 (_tstring)
// @return 판별된 인코딩 타입 (EEncoding 열거형)
//***************************************************************************
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

	// 파일이 너무 작아 BOM을 확인할 수 없는 경우 (기본은 ANSI 또는 DEFAULT 처리)
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
// @brief C++ 표준 스트림을 활용해 다양한 인코딩의 파일을 읽어 _tstring으로 반환합니다.
// @param filepath 읽어들일 파일의 전체 경로 (_tstring)
// @return 읽어들인 문자열 내용 (_tstring)
//***************************************************************************
_tstring ReadFile(const _tstring& filepath)
{
	std::error_code ec;
	std::uintmax_t fileSize = std::filesystem::file_size(filepath, ec);
	if( ec )
	{
		return _T("");
	}

	std::ifstream file(filepath, std::ios::binary);
	if( !file )
	{
		return _T("");
	}

	std::vector<char> buffer(static_cast<size_t>(fileSize));
	if( fileSize > 0 )
	{
		file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
		buffer.resize(static_cast<size_t>(file.gcount()));
	}

	if( buffer.empty() )
	{
		return _T("");
	}

#ifdef _UNICODE
	std::wstring result;

	if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_LE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		result.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
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
			wchar_t codeUnit = (static_cast<unsigned char>(buffer[i]) << 8) | static_cast<unsigned char>(buffer[i + 1]);
			result.push_back(codeUnit);
		}
	}
	else if( buffer.size() >= 3 && static_cast<unsigned char>(buffer[0]) == UTF_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UTF_FILE_IDENTIFIER_BYTE2
		&& static_cast<unsigned char>(buffer[2]) == UTF_FILE_IDENTIFIER_BYTE3 )
	{
		std::string utf8Str(buffer.begin() + 3, buffer.end());
		result = Utf8ToUnicode(utf8Str);
	}
	else
	{
		std::string rawStr(buffer.begin(), buffer.end());
		if( IsUTF8WithoutBom((const void*)buffer.data(), buffer.size()) )
		{
			result = Utf8ToUnicode(rawStr);
		}
		else
		{
			result = AnsiToUnicode(rawStr);
		}
	}

	if( !result.empty() && result.back() == L'\0' )
	{
		result.pop_back();
	}

#else
	std::string result;

	if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_LE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		std::wstring temp;
		temp.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			wchar_t codeUnit = static_cast<unsigned char>(buffer[i]) | (static_cast<unsigned char>(buffer[i + 1]) << 8);
			temp.push_back(codeUnit);
		}
		result = UnicodeToAnsi(temp);
	}
	else if( buffer.size() >= 2 && static_cast<unsigned char>(buffer[0]) == UNICODE_BE_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		std::wstring temp;
		temp.reserve(buffer.size() / 2);
		for( size_t i = 2; i < buffer.size(); i += 2 )
		{
			wchar_t codeUnit = (static_cast<unsigned char>(buffer[i]) << 8) | static_cast<unsigned char>(buffer[i + 1]);
			temp.push_back(codeUnit);
		}
		result = UnicodeToAnsi(temp);
	}
	else if( buffer.size() >= 3 && static_cast<unsigned char>(buffer[0]) == UTF_FILE_IDENTIFIER_BYTE1
		&& static_cast<unsigned char>(buffer[1]) == UTF_FILE_IDENTIFIER_BYTE2
		&& static_cast<unsigned char>(buffer[2]) == UTF_FILE_IDENTIFIER_BYTE3 )
	{
		std::string utf8Str(buffer.begin() + 3, buffer.end());
		result = Utf8ToAnsi(utf8Str);
	}
	else
	{
		std::string rawStr(buffer.begin(), buffer.end());
		if( IsUTF8WithoutBom((const void*)buffer.data(), buffer.size()) )
		{
			result = Utf8ToAnsi(rawStr);
		}
		else
		{
			result = rawStr;
		}
	}

	if( !result.empty() && result.back() == '\0' )
	{
		result.pop_back();
	}
#endif

	return result;
}

//***************************************************************************
// @brief 지정한 인코딩 타입에 따라 _tstring 문자열을 파일에 기록합니다.
// @param filepath 저장할 파일의 전체 경로 (_tstring)
// @param content 파일에 쓸 문자열 내용 (_tstring)
// @param encoding 저장할 인코딩 타입 (EEncoding 열거형)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool WriteFile(const _tstring& filepath, const _tstring& content, EEncoding encoding)
{
	std::ofstream file(filepath, std::ios::binary);
	if( !file )
	{
		return false;
	}

#ifdef _UNICODE
	std::wstring cleanContent = content;
	if( !cleanContent.empty() && cleanContent.back() == L'\0' )
	{
		cleanContent.pop_back();
	}

	if( encoding == EEncoding::UTF16_BE )
	{
		unsigned char bom[] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		for( wchar_t ch : cleanContent )
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

		file.write(reinterpret_cast<const char*>(cleanContent.data()), cleanContent.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		std::string dest = UnicodeToUtf8(cleanContent);
		file.write(dest.c_str(), dest.size());
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		std::string dest = UnicodeToUtf8(cleanContent);
		file.write(dest.c_str(), dest.size());
	}
	else
	{
		std::string dest = UnicodeToAnsi(cleanContent);
		file.write(dest.c_str(), dest.size());
	}
#else
	std::string cleanContent = content;
	if( !cleanContent.empty() && cleanContent.back() == '\0' )
	{
		cleanContent.pop_back();
	}

	if( encoding == EEncoding::UTF16_BE )
	{
		unsigned char bom[] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		file.write(reinterpret_cast<const char*>(bom), 2);

		std::wstring temp = AnsiToUnicode(cleanContent);
		for( wchar_t ch : temp )
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

		std::wstring temp = AnsiToUnicode(cleanContent);
		file.write(reinterpret_cast<const char*>(temp.data()), temp.size() * sizeof(wchar_t));
	}
	else if( encoding == EEncoding::UTF8_BOM )
	{
		unsigned char bom[] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		file.write(reinterpret_cast<const char*>(bom), 3);

		std::string dest = AnsiToUtf8(cleanContent);
		file.write(dest.c_str(), dest.size());
	}
	else if( encoding == EEncoding::UTF8_NOBOM )
	{
		std::string dest = AnsiToUtf8(cleanContent);
		file.write(dest.c_str(), dest.size());
	}
	else
	{
		file.write(cleanContent.c_str(), cleanContent.size());
	}
#endif

	file.close();
	return true;
}

//***************************************************************************
// @brief std::filesystem을 사용하여 파일 존재 여부를 확인합니다.
// @param filepath 존재 여부를 확인할 파일의 전체 경로 (_tstring)
// @return 파일이 존재하면 true, 아니면 false
//***************************************************************************
bool IsExistFile(const _tstring& filepath)
{
	std::error_code ec;
	bool bExists = std::filesystem::exists(filepath, ec);

	// ec가 설정된 경우는 "파일 없음"이 아니라 실제 접근 중 오류가 발생한 상황
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
//***************************************************************************
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