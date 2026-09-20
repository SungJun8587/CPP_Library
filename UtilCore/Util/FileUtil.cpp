
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
// @note 각 시작 바이트에 대해 이어지는 연속 바이트 개수뿐 아니라 Unicode
//       scalar value 기준의 유효 범위(오버롱 인코딩 제외, U+D800~DFFF 서로게이트
//       제외, U+10FFFF 초과 제외)까지 검사합니다. 0x80 이상의 바이트가 하나도
//       없는 순수 ASCII 버퍼는 ANSI/UTF-8을 구분할 근거가 없으므로 false를
//       반환합니다(호출부에서 ANSI로 분류됨).
//***************************************************************************
bool IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize)
{
	if( pBuffer == nullptr || BuffSize == 0 )
		return false;

	const unsigned char* p = static_cast<const unsigned char*>(pBuffer);
	const unsigned char* end = p + BuffSize;

	bool bHasMultibyte = false;		// 실제로 0x80 이상 바이트가 한 번이라도 나왔는지 추적

	while( p < end )
	{
		const unsigned char c = *p;

		if( c <= 0x7F )					// 1바이트 문자 (0xxxxxxx)
		{
			++p;
			continue;
		}

		if( c >= 0xC2 && c <= 0xDF )		// 2바이트 시퀀스 (오버롱 방지: C0/C1 제외)
		{
			if( end - p < 2 || (p[1] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 2;
			continue;
		}

		if( c == 0xE0 )						// 3바이트 시퀀스, 첫 구간(오버롱 방지: A0~BF)
		{
			if( end - p < 3 || p[1] < 0xA0 || p[1] > 0xBF || (p[2] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 3;
			continue;
		}

		if( c >= 0xE1 && c <= 0xEC )		// 3바이트 시퀀스, 일반 구간
		{
			if( end - p < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 3;
			continue;
		}

		if( c == 0xED )						// 3바이트 시퀀스, UTF-16 서로게이트 영역(U+D800~DFFF) 제외
		{
			if( end - p < 3 || p[1] < 0x80 || p[1] > 0x9F || (p[2] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 3;
			continue;
		}

		if( c >= 0xEE && c <= 0xEF )		// 3바이트 시퀀스, 나머지 구간
		{
			if( end - p < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 3;
			continue;
		}

		if( c == 0xF0 )						// 4바이트 시퀀스, 첫 구간(오버롱 방지: 90~BF)
		{
			if( end - p < 4 || p[1] < 0x90 || p[1] > 0xBF || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 4;
			continue;
		}

		if( c >= 0xF1 && c <= 0xF3 )		// 4바이트 시퀀스, 일반 구간
		{
			if( end - p < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 4;
			continue;
		}

		if( c == 0xF4 )						// 4바이트 시퀀스, U+10FFFF 초과 방지(80~8F)
		{
			if( end - p < 4 || p[1] < 0x80 || p[1] > 0x8F || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80 )
				return false;

			bHasMultibyte = true;
			p += 4;
			continue;
		}

		// 0x80~0xC1(연속 바이트/오버롱 전용 시작 바이트), 0xF5 이상은 유효한 시작 바이트가 아님
		return false;
	}

	return bHasMultibyte;	// 멀티바이트 시퀀스가 실제로 있고 전부 유효할 때만 true
}

#ifdef _WIN32
// --- 익명 네임스페이스 시작 ---
// 이 안에 선언된 함수들은 오직 이 FileUtil.cpp 파일 안에서만 접근할 수 있습니다.
namespace {

	//***************************************************************************
	// @brief 이미 메모리에 있는 바이트 버퍼로부터 BOM/휴리스틱 기반 인코딩을
	//        판별하는 공용 헬퍼입니다. 파일 핸들 기반 판별(DetectFileEncoding)과
	//        메모리 매핑 기반 판별(ReadFileMap)이 이 함수를 공유하여, 이미
	//        메모리에 올라온 데이터에 대해 다시 파일을 읽는 일 없이 그 자리에서
	//        1회 판별이 끝나도록 합니다.
	// @param pData 판별할 데이터 버퍼 시작 주소
	// @param DataSize 버퍼 크기
	// @return 판별된 인코딩 타입. 빈 버퍼는 정책상 EEncoding::ANSI로 취급합니다.
	//***************************************************************************
	EEncoding DetectEncodingFromBuffer(const unsigned char* pData, size_t DataSize)
	{
		if( DataSize == 0 )
			return EEncoding::ANSI;	// 빈 파일은 내용이 없는 ANSI/텍스트 파일로 취급

		if( DataSize >= 2 && pData[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && pData[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
			return EEncoding::UTF16_LE;

		if( DataSize >= 2 && pData[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && pData[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
			return EEncoding::UTF16_BE;

		if( DataSize >= 3 && pData[0] == UTF_FILE_IDENTIFIER_BYTE1 && pData[1] == UTF_FILE_IDENTIFIER_BYTE2 && pData[2] == UTF_FILE_IDENTIFIER_BYTE3 )
			return EEncoding::UTF8_BOM;

		return IsUTF8WithoutBom(pData, DataSize) ? EEncoding::UTF8_NOBOM : EEncoding::ANSI;
	}

	//***************************************************************************
	// @brief 이미 열려 있는 파일 핸들로부터 인코딩 타입을 판별하는 공용 헬퍼입니다.
	//        GetFileEncodingType(TCHAR*)와 ReadFile(_tstring&, TCHAR*)이 이 함수를
	//        공유하여 BOM/휴리스틱 판별 로직이 여러 곳에서 따로 구현되지 않도록 합니다.
	// @param hFile 읽기 권한으로 이미 열려 있는 파일 핸들
	// @return 판별된 인코딩 타입. 판별 실패(읽기 실패 등) 시 EEncoding::DEFAULT
	// @note 호출 후 파일 포인터는 항상 파일 시작(오프셋 0)으로 되돌려 놓습니다.
	//       핸들의 오픈/클로즈는 호출자 책임입니다. BOM 판별에 필요한 최소 바이트만
	//       먼저 읽고, BOM이 없는 경우에만 휴리스틱 판별을 위해 파일 전체를 한 번 더
	//       읽습니다(이 함수 자체 안에서는 최대 2회 읽기이며, 호출부가 이 함수를
	//       호출한 뒤 별도로 파일 내용을 다시 읽는 일은 없습니다).
	//***************************************************************************
	EEncoding DetectFileEncoding(HANDLE hFile)
	{
		unsigned char	szHeader[3] = { 0, };
		DWORD			dwReadSize = 0;

		if( !::ReadFile(hFile, szHeader, 3, &dwReadSize, NULL) )
		{
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
			return EEncoding::DEFAULT;
		}

		if( dwReadSize == 0 )
		{
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
			return EEncoding::ANSI;	// 빈 파일 정책: 다른 판별 경로와 동일하게 ANSI
		}

		EEncoding eFileType = EEncoding::DEFAULT;

		if( dwReadSize >= 2 && szHeader[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && szHeader[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_LE;
		}
		else if( dwReadSize >= 2 && szHeader[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && szHeader[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
		{
			eFileType = EEncoding::UTF16_BE;
		}
		else if( dwReadSize >= 3 && szHeader[0] == UTF_FILE_IDENTIFIER_BYTE1 && szHeader[1] == UTF_FILE_IDENTIFIER_BYTE2 && szHeader[2] == UTF_FILE_IDENTIFIER_BYTE3 )
		{
			eFileType = EEncoding::UTF8_BOM;
		}
		else
		{
			// BOM이 없는 경우, 휴리스틱 판별을 위해 파일 전체를 읽습니다.
			// (이 판별 함수 하나가 이 파일에서 유일하게 "전체 읽기"를 수행하는
			// 지점이며, 호출부는 이 결과를 그대로 사용하고 별도로 다시 읽지 않습니다.)
			::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

			LARGE_INTEGER liFileSize = {};
			if( !::GetFileSizeEx(hFile, &liFileSize) || liFileSize.QuadPart < 0 )
			{
				return EEncoding::DEFAULT;
			}

			if( liFileSize.QuadPart == 0 )
			{
				return EEncoding::ANSI;
			}

			// 이 판별 경로는 레거시 핸들 기반 API(GetFileEncodingType(TCHAR*),
			// GetFileInfoAndEncoding(TCHAR*))를 위한 것으로, 4GiB를 넘는 파일은
			// ReadFileMap 계열(메모리 매핑 기반)을 사용하도록 안내합니다.
			if( static_cast<ULONGLONG>(liFileSize.QuadPart) > MAXDWORD )
			{
				return EEncoding::DEFAULT;
			}

			const DWORD dwFileSize = static_cast<DWORD>(liFileSize.QuadPart);

			std::vector<unsigned char> buffer;

			try
			{
				buffer.resize(dwFileSize);
			}
			catch( const std::bad_alloc& )
			{
				// 이 함수는 hFile을 소유하지 않으므로(호출자 책임), 예외를 여기서
				// 막지 않으면 호출자가 자신의 CloseHandle을 실행하기도 전에 예외가
				// 전파되어 핸들이 누수됩니다.
				::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
				return EEncoding::DEFAULT;
			}

			// 다른 읽기 함수들과 동일하게 MAX_BUFFER_SIZE 단위로 나누어 읽습니다.
			// 단일 대용량 ReadFile 호출은 네트워크 드라이브 등에서 partial read로
			// 끝날 수 있어(=dwBytesRead < dwFileSize), 분할 읽기로 통일합니다.
			const DWORD		dwMaxReadSize = MAX_BUFFER_SIZE;
			DWORD			dwReadOffset = 0;
			bool			bReadOk = true;

			while( dwReadOffset < dwFileSize )
			{
				const DWORD dwRemain = dwFileSize - dwReadOffset;
				const DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : dwRemain;
				DWORD dwReadSize = 0;

				if( !::ReadFile(hFile, buffer.data() + dwReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
				{
					bReadOk = false;
					break;
				}

				dwReadOffset += dwReadSize;
			}

			eFileType = bReadOk ? DetectEncodingFromBuffer(buffer.data(), dwReadOffset) : EEncoding::ANSI;
		}

		::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

		return eFileType;
	}

	//***************************************************************************
	// @brief 이미 인코딩이 판별되어 BOM을 건너뛴 데이터 버퍼를 _tstring으로
	//        디코딩하는 공용 헬퍼입니다. ReadFile(_tstring&, TCHAR*)와
	//        ReadFileMap(_tstring&, TCHAR*)가 이 함수를 공유하여, 두 API의
	//        embedded NUL 처리·UTF-16 홀수바이트 검증·변환 시 사용하는 길이
	//        기준이 항상 동일하게 유지되도록 합니다(널 종료 문자열 가정 없이
	//        항상 명시적 길이만으로 변환합니다).
	// @param pData BOM을 제외한 실제 데이터 시작 주소
	// @param DataLen pData로부터의 데이터 길이(바이트)
	// @param eFileType 이미 판별된 인코딩 타입(BOM 유무 판단은 이미 끝난 상태)
	// @param destString [out] 디코딩된 문자열
	// @return 성공 시 true, 인코딩 자체가 malformed 이거나 변환 실패 시 false
	//***************************************************************************
	bool DecodeToString(const unsigned char* pData, size_t DataLen, EEncoding eFileType, _tstring& destString)
	{
		destString.clear();

		if( DataLen == 0 )
			return true;

		switch( eFileType )
		{
		case EEncoding::UTF16_LE:
		case EEncoding::UTF16_BE:
		{
			// UTF-16은 반드시 2바이트 단위로 구성되어야 합니다. 홀수 바이트로 잘린
			// 데이터는 두 API 모두 동일하게 실패로 처리합니다.
			if( (DataLen & 1) != 0 )
				return false;

			const size_t wcharCount = DataLen / 2;
			const bool bBigEndian = (eFileType == EEncoding::UTF16_BE);

#ifdef _UNICODE
			if( wcharCount > destString.max_size() )
				return false;

			destString.resize(wcharCount);

			for( size_t i = 0; i < wcharCount; ++i )
			{
				const size_t offset = i * 2;
				uint16_t value;

				if( bBigEndian )
					value = static_cast<uint16_t>((static_cast<uint16_t>(pData[offset]) << 8) | static_cast<uint16_t>(pData[offset + 1]));
				else
					value = static_cast<uint16_t>(static_cast<uint16_t>(pData[offset]) | (static_cast<uint16_t>(pData[offset + 1]) << 8));

				destString[i] = static_cast<TCHAR>(value);
			}
#else
			std::wstring wstr;

			if( wcharCount > wstr.max_size() )
				return false;

			wstr.resize(wcharCount);

			for( size_t i = 0; i < wcharCount; ++i )
			{
				const size_t offset = i * 2;
				uint16_t value;

				if( bBigEndian )
					value = static_cast<uint16_t>((static_cast<uint16_t>(pData[offset]) << 8) | static_cast<uint16_t>(pData[offset + 1]));
				else
					value = static_cast<uint16_t>(static_cast<uint16_t>(pData[offset]) | (static_cast<uint16_t>(pData[offset + 1]) << 8));

				wstr[i] = static_cast<wchar_t>(value);
			}

			if( !wstr.empty() )
			{
				// NUL 포함 여부와 무관하게 실제 char 개수(wstr.size())를 그대로 전달합니다.
				if( UnicodeToAnsi_String(destString, wstr.c_str(), wstr.size()) != 0 )
					return false;
			}
#endif
			return true;
		}
		case EEncoding::UTF8_BOM:
		case EEncoding::UTF8_NOBOM:
		{
			const char* pMultibyteData = reinterpret_cast<const char*>(pData);

#ifdef _UNICODE
			// std::string 중간 할당 없이 포인터와 정확한 길이를 직접 전달합니다.
			return Utf8ToUnicode_String(destString, pMultibyteData, DataLen) == 0;
#else
			return Utf8ToAnsi_String(destString, pMultibyteData, DataLen) == 0;
#endif
		}
		case EEncoding::ANSI:
		{
			const char* pMultibyteData = reinterpret_cast<const char*>(pData);

#ifdef _UNICODE
			return AnsiToUnicode_String(destString, pMultibyteData, DataLen) == 0;
#else
			destString.assign(pMultibyteData, DataLen);
			return true;
#endif
		}
		default:
			return false;
		}
	}

	//***************************************************************************
	// @brief 데이터를 지정한 크기만큼 파일 핸들에 나누어 씁니다.
	// @param hFile 쓰기 권한으로 이미 열려 있는 파일 핸들
	// @param pData 기록할 데이터 시작 주소
	// @param DataSize 기록할 데이터 크기(바이트). 64비트이므로 4GiB를 넘는 데이터도
	//        size_t→DWORD 절단 없이 정확하게 기록합니다.
	// @return 성공 시 true, 실패 시 false
	//***************************************************************************
	bool WriteAllChunked(HANDLE hFile, const void* pData, ULONGLONG DataSize)
	{
		const unsigned char* pBuffer = static_cast<const unsigned char*>(pData);
		const DWORD dwMaxWriteSize = MAX_BUFFER_SIZE;
		ULONGLONG	ullWriteOffset = 0;

		while( ullWriteOffset < DataSize )
		{
			const ULONGLONG ullRemain = DataSize - ullWriteOffset;
			const DWORD dwWriteSize = (ullRemain > dwMaxWriteSize) ? dwMaxWriteSize : static_cast<DWORD>(ullRemain);
			DWORD dwWrittenSize = 0;

			if( !::WriteFile(hFile, pBuffer + ullWriteOffset, dwWriteSize, &dwWrittenSize, NULL) || dwWrittenSize == 0 )
				return false;

			ullWriteOffset += dwWrittenSize;
		}

		return true;
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

		const bool bResult = WriteAllChunked(hFile, strAnsi.data(), static_cast<ULONGLONG>(strAnsi.size()));

		::CloseHandle(hFile);

		return bResult;
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
		if( ptszBuffer == nullptr ) return false;

		std::wstring strUnicode;

#ifdef _UNICODE
		if( BufferSize > 0 )
			strUnicode.assign(ptszBuffer, BufferSize);
#else
		if( BufferSize > 0 && AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

		// [주의] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제거
		// (SaveAnsiFile/SaveUTF8*File과 동일한 정책 — BufferSize가 널 종료 문자를
		// 포함해서 넘어온 경우를 대비합니다.)
		if( !strUnicode.empty() && strUnicode.back() == L'\0' )
		{
			strUnicode.pop_back();
		}

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
		const unsigned char szBom[2] = { UNICODE_BE_FILE_IDENTIFIER_BYTE1, UNICODE_BE_FILE_IDENTIFIER_BYTE2 };
		if( !WriteAllChunked(hFile, szBom, sizeof(szBom)) )
		{
			::CloseHandle(hFile);
			return false;
		}

		const bool bResult = WriteAllChunked(hFile, strBE.data(), static_cast<ULONGLONG>(strBE.size()) * sizeof(wchar_t));

		::CloseHandle(hFile);

		return bResult;
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
		if( ptszBuffer == nullptr ) return false;

		std::wstring strUnicode;

#ifdef _UNICODE
		if( BufferSize > 0 )
			strUnicode.assign(ptszBuffer, BufferSize);
#else
		if( BufferSize > 0 && AnsiToUnicode_String(strUnicode, ptszBuffer, BufferSize) != 0 ) return false;
#endif

		// [주의] 변환된 문자열 끝에 널 문자('\0')가 포함되어 있다면 제거
		// (SaveAnsiFile/SaveUTF8*File과 동일한 정책)
		if( !strUnicode.empty() && strUnicode.back() == L'\0' )
		{
			strUnicode.pop_back();
		}

		HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return false;

		// UTF-16 LE BOM 작성
		const unsigned char szBom[2] = { UNICODE_LE_FILE_IDENTIFIER_BYTE1, UNICODE_LE_FILE_IDENTIFIER_BYTE2 };
		if( !WriteAllChunked(hFile, szBom, sizeof(szBom)) )
		{
			::CloseHandle(hFile);
			return false;
		}

		const bool bResult = WriteAllChunked(hFile, strUnicode.data(), static_cast<ULONGLONG>(strUnicode.size()) * sizeof(wchar_t));

		::CloseHandle(hFile);

		return bResult;
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
		const unsigned char szBom[3] = { UTF_FILE_IDENTIFIER_BYTE1, UTF_FILE_IDENTIFIER_BYTE2, UTF_FILE_IDENTIFIER_BYTE3 };
		if( !WriteAllChunked(hFile, szBom, sizeof(szBom)) )
		{
			::CloseHandle(hFile);
			return false;
		}

		const bool bResult = WriteAllChunked(hFile, strUtf8.data(), static_cast<ULONGLONG>(strUtf8.size()));

		::CloseHandle(hFile);

		return bResult;
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

		const bool bResult = WriteAllChunked(hFile, strUtf8.data(), static_cast<ULONGLONG>(strUtf8.size()));

		::CloseHandle(hFile);

		return bResult;
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
// @note 파일 크기는 64비트로 조회하므로 4GiB 이상의 파일도 지원합니다.
//       실제 메모리에 담을 수 있는지 여부는 std::vector<BYTE>의 한계를 따릅니다.
//***************************************************************************
bool ReadFile(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	// 파일 핸들 오픈 (크기 조회와 읽기를 같은 핸들로 수행하여 CreateFile 중복 호출과
	// 그 사이 다른 프로세스가 파일 크기를 바꿔버릴 수 있는 레이스를 없앱니다.)
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	LARGE_INTEGER liFileSize = {};
	if( !::GetFileSizeEx(hFile, &liFileSize) || liFileSize.QuadPart < 0 )
	{
		::CloseHandle(hFile);
		return false;
	}

	const ULONGLONG ullLength = static_cast<ULONGLONG>(liFileSize.QuadPart);

	if( ullLength > static_cast<ULONGLONG>(std::vector<BYTE>{}.max_size()) )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 파일 크기만큼 저장 공간 확보 (메모리 부족 시 hFile을 정리한 뒤 실패로 반환)
	try
	{
		byteDestination.resize(static_cast<size_t>(ullLength));
	}
	catch( const std::bad_alloc& )
	{
		::CloseHandle(hFile);
		return false;
	}

	const DWORD		dwMaxReadSize = MAX_BUFFER_SIZE;
	ULONGLONG		ullReadOffset = 0;
	BYTE* pbBuffer = byteDestination.data();

	// 최대 크기 단위로 나누어 분할 읽기 수행
	while( ullReadOffset < ullLength )
	{
		const ULONGLONG dwRemain = ullLength - ullReadOffset;
		const DWORD dwReadNumSize = (dwRemain > dwMaxReadSize) ? dwMaxReadSize : static_cast<DWORD>(dwRemain);
		DWORD dwReadSize = 0;

		if( !::ReadFile(hFile, pbBuffer + ullReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
		{
			::CloseHandle(hFile);
			return false;
		}

		ullReadOffset += dwReadSize;	// 실제로 읽은 만큼만 증가 (부분 읽기 대응)
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
	byteDestination.clear();

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) == 0 )
		return false;

	// 1. 파일 열기
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// 2. 파일 크기 확인
	LARGE_INTEGER liFileSize = {};
	if( !::GetFileSizeEx(hFile, &liFileSize) || liFileSize.QuadPart < 0 )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 빈 파일 처리
	if( liFileSize.QuadPart == 0 )
	{
		::CloseHandle(hFile);
		return true;
	}

	// SIZE_T 범위 오버플로우 검사
	const ULONGLONG ullFileSize = static_cast<ULONGLONG>(liFileSize.QuadPart);

	if( ullFileSize > static_cast<ULONGLONG>((std::numeric_limits<SIZE_T>::max)()) )
	{
		::CloseHandle(hFile);
		return false;
	}

	const SIZE_T fileSize = static_cast<SIZE_T>(ullFileSize);

	// std::vector의 size_type 범위 확인
	if( fileSize > std::vector<BYTE>{}.max_size() )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 3. 파일 매핑 객체 생성
	//
	// 파일을 읽기만 하므로 PAGE_READONLY / FILE_MAP_READ을 사용합니다.
	// PAGE_WRITECOPY / FILE_MAP_COPY는 이 함수에서는 필요하지 않습니다.
	const DWORD dwSizeHigh = static_cast<DWORD>(ullFileSize >> 32);
	const DWORD dwSizeLow = static_cast<DWORD>(ullFileSize & 0xFFFFFFFFULL);

	HANDLE hFileMap = ::CreateFileMapping(hFile, nullptr, PAGE_READONLY, dwSizeHigh, dwSizeLow, nullptr);
	if( hFileMap == nullptr )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 4. 파일 뷰 생성
	const BYTE* pFileData = static_cast<const BYTE*>(::MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0));
	if( pFileData == nullptr )
	{
		::CloseHandle(hFileMap);
		::CloseHandle(hFile);
		return false;
	}

	// 5. 메모리에서 vector로 전체 파일 복사
	//
	// resize()가 실패하면 std::bad_alloc이 발생할 수 있으므로,
	// 파일 매핑 리소스와 별개로 예외가 발생할 수 있다는 점에 주의합니다.
	try
	{
		byteDestination.resize(fileSize);
		memcpy(byteDestination.data(), pFileData, fileSize);
	}
	catch( ... )
	{
		::UnmapViewOfFile(pFileData);
		::CloseHandle(hFileMap);
		::CloseHandle(hFile);

		byteDestination.clear();
		return false;
	}

	// 6. 리소스 해제
	::UnmapViewOfFile(pFileData);
	::CloseHandle(hFileMap);
	::CloseHandle(hFile);

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
	if( pbBuffer == nullptr && dwLength != 0 ) return false;

	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const bool bResult = WriteAllChunked(hFile, pbBuffer, dwLength);

	::CloseHandle(hFile);

	return bResult;
}

//***************************************************************************
// @brief 인코딩 종류에 관계없이 파일을 자동 감지하여 _tstring 형태로 읽어들입니다.
// @param destString 읽어들인 문자열을 저장할 _tstring 참조
// @param ptszFullPath 읽어들일 파일의 전체 경로
// @return 성공 시 true, 실패 시 false
// @note 파일 전체를 한 번만 읽어 그 버퍼 위에서 인코딩을 판별하고(단, 4GiB
//       초과 파일은 ReadFileMap 계열을 사용하도록 안내), BOM을 제외한 나머지를
//       ReadFileMap(_tstring&, TCHAR*)와 동일한 DecodeToString()으로 디코딩하므로
//       embedded NUL 처리, UTF-16 홀수바이트 검증, 변환 시 사용하는 길이 기준이
//       두 API 사이에서 항상 동일합니다.
//***************************************************************************
bool ReadFile(_tstring& destString, const TCHAR* ptszFullPath)
{
	destString.clear();

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	LARGE_INTEGER liFileSize = {};
	if( !::GetFileSizeEx(hFile, &liFileSize) || liFileSize.QuadPart < 0 )
	{
		::CloseHandle(hFile);
		return false;
	}

	const ULONGLONG ullLength = static_cast<ULONGLONG>(liFileSize.QuadPart);

	if( ullLength > static_cast<ULONGLONG>(std::vector<unsigned char>{}.max_size()) )
	{
		::CloseHandle(hFile);
		return false;
	}

	if( ullLength == 0 )
	{
		::CloseHandle(hFile);
		return true;
	}

	std::vector<unsigned char> buffer;

	try
	{
		buffer.resize(static_cast<size_t>(ullLength));
	}
	catch( const std::bad_alloc& )
	{
		::CloseHandle(hFile);
		return false;
	}

	const DWORD		dwMaxReadSize = MAX_BUFFER_SIZE;
	ULONGLONG		ullReadOffset = 0;
	unsigned char* pBuffer = buffer.data();

	while( ullReadOffset < ullLength )
	{
		const ULONGLONG ullRemain = ullLength - ullReadOffset;
		const DWORD dwReadNumSize = (ullRemain > dwMaxReadSize) ? dwMaxReadSize : static_cast<DWORD>(ullRemain);
		DWORD dwReadSize = 0;

		if( !::ReadFile(hFile, pBuffer + ullReadOffset, dwReadNumSize, &dwReadSize, NULL) || dwReadSize == 0 )
		{
			::CloseHandle(hFile);
			return false;
		}

		ullReadOffset += dwReadSize;
	}

	::CloseHandle(hFile);

	const EEncoding eFileType = DetectEncodingFromBuffer(buffer.data(), buffer.size());

	const unsigned char* pData = buffer.data();
	size_t dataLen = buffer.size();

	switch( eFileType )
	{
	case EEncoding::UTF16_LE:
	case EEncoding::UTF16_BE:
		if( dataLen < 2 ) return false;
		pData += 2;
		dataLen -= 2;
		break;
	case EEncoding::UTF8_BOM:
		if( dataLen < 3 ) return false;
		pData += 3;
		dataLen -= 3;
		break;
	case EEncoding::UTF8_NOBOM:
	case EEncoding::ANSI:
		break;
	default:
		return false;
	}

	try
	{
		return DecodeToString(pData, dataLen, eFileType, destString);
	}
	catch( const std::bad_alloc& )
	{
		destString.clear();
		return false;
	}
}

//***************************************************************************
// @brief 파일 매핑을 활용해 다양한 인코딩의 파일을 읽어 _tstring으로 변환합니다.
// @param destString 읽어들인 문자열이 저장될 참조 (_tstring)
// @param ptszFullPath 읽어들일 파일의 전체 경로 (TCHAR*)
// @return 성공 시 true, 실패 시 false
// @note 빈 파일은 매핑을 시도하지 않고 즉시 성공(빈 문자열)으로 처리합니다.
//       인코딩 판별은 이미 매핑된 메모리 위에서 DetectEncodingFromBuffer()로
//       1회만 수행하므로, 판별을 위해 파일을 별도로 다시 읽지 않습니다.
//       디코딩은 ReadFile(_tstring&, TCHAR*)와 공유하는 DecodeToString()을
//       사용합니다.
//***************************************************************************
bool ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath)
{
	destString.clear();

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) == 0 )
		return false;

	// 1. 파일 열기
	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	// 2. 파일 크기 확인
	LARGE_INTEGER liFileSize = {};
	if( !::GetFileSizeEx(hFile, &liFileSize) || liFileSize.QuadPart < 0 )
	{
		::CloseHandle(hFile);
		return false;
	}

	// 빈 파일은 매핑 없이 바로 성공 처리합니다(이 분기가 항상 여기서 확정되며,
	// 이후의 인코딩 판별/매핑 단계로 넘어가지 않습니다).
	if( liFileSize.QuadPart == 0 )
	{
		::CloseHandle(hFile);
		return true;
	}

	// SIZE_T 범위 오버플로우 검사 (32bit 빌드 대응)
	const ULONGLONG ullFileSize = static_cast<ULONGLONG>(liFileSize.QuadPart);
	if( ullFileSize > static_cast<ULONGLONG>((std::numeric_limits<SIZE_T>::max)()) )
	{
		::CloseHandle(hFile);
		return false;
	}

	const SIZE_T fileSize = static_cast<SIZE_T>(ullFileSize);

	// 3. 파일 매핑 생성 및 뷰 매핑
	const DWORD dwSizeHigh = static_cast<DWORD>(ullFileSize >> 32);
	const DWORD dwSizeLow = static_cast<DWORD>(ullFileSize & 0xFFFFFFFFULL);

	HANDLE hFileMap = ::CreateFileMapping(hFile, nullptr, PAGE_READONLY, dwSizeHigh, dwSizeLow, nullptr);
	if( hFileMap == nullptr )
	{
		::CloseHandle(hFile);
		return false;
	}

	const BYTE* pFileData = static_cast<const BYTE*>(::MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0));
	if( pFileData == nullptr )
	{
		::CloseHandle(hFileMap);
		::CloseHandle(hFile);
		return false;
	}

	// 4. 매핑된 메모리 위에서 바로 인코딩 판별 (별도의 파일 재읽기 없음)
	const EEncoding eFileType = DetectEncodingFromBuffer(pFileData, fileSize);

	// 5. BOM 및 실제 데이터 오프셋 결정
	const BYTE* pData = pFileData;
	SIZE_T dataLen = fileSize;
	bool bIsProcess = true;

	switch( eFileType )
	{
	case EEncoding::UTF16_LE:
	case EEncoding::UTF16_BE:
	{
		if( fileSize < 2 )
		{
			bIsProcess = false;
			break;
		}

		pData += 2;
		dataLen -= 2;
		break;
	}
	case EEncoding::UTF8_BOM:
	{
		if( fileSize < 3 )
		{
			bIsProcess = false;
			break;
		}

		pData += 3;
		dataLen -= 3;
		break;
	}
	case EEncoding::UTF8_NOBOM:
	case EEncoding::ANSI:
		break;
	default:
		bIsProcess = false;
		break;
	}

	// 6. 데이터 변환 처리 (ReadFile(_tstring&, TCHAR*)와 동일한 DecodeToString 공유)
	// DecodeToString 내부의 resize()는 메모리 부족 시 std::bad_alloc을 던질 수 있으므로,
	// 아래 7번 자원 해제(UnmapViewOfFile/CloseHandle)가 반드시 실행되도록 감쌉니다.
	if( bIsProcess )
	{
		try
		{
			bIsProcess = DecodeToString(pData, dataLen, eFileType, destString);
		}
		catch( const std::bad_alloc& )
		{
			bIsProcess = false;
		}
	}

	// 7. 자원 해제
	::UnmapViewOfFile(pFileData);
	::CloseHandle(hFileMap);
	::CloseHandle(hFile);

	// 8. 실패 시 안전하게 Clear
	if( !bIsProcess )
	{
		destString.clear();
		return false;
	}

	return true;
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
// @brief 파일 크기를 64비트 값으로 반환합니다.
// @param ptszFullPath 크기를 조회할 파일의 전체 경로
// @return 파일 크기 (바이트), 실패 시 0
// @note GetFileSizeEx()를 사용하므로 4GiB 이상의 파일도 정확한 크기를 반환합니다.
//***************************************************************************
ULONGLONG GetFileSize(const TCHAR* ptszFullPath)
{
	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return 0;

	HANDLE hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return 0;

	LARGE_INTEGER liFileSize = {};
	const BOOL bResult = ::GetFileSizeEx(hFile, &liFileSize);

	::CloseHandle(hFile);

	if( !bResult || liFileSize.QuadPart < 0 )
		return 0;

	return static_cast<ULONGLONG>(liFileSize.QuadPart);
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
	if( lpFileInformation == nullptr ) return false;

	hFile = ::CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	bResult = ::GetFileInformationByHandle(hFile, lpFileInformation);

	::CloseHandle(hFile);

	return bResult;
}

//***************************************************************************
// @brief 파일 핸들을 한 번만 열어 상세 정보와 인코딩 타입을 동시에 조회합니다.
// @detail GetFileInformation()과 GetFileEncodingType(TCHAR*)를 각각 호출할 때
//         발생하는 CreateFile/CloseHandle 중복 왕복을 제거합니다.
// @param ptszFullPath 조회할 파일의 전체 경로
// @param lpFileInformation 조회 정보를 저장할 BY_HANDLE_FILE_INFORMATION 구조체 포인터
// @param outEncoding [out] 판별된 인코딩 타입
// @return 파일 정보 조회와 인코딩 판별이 모두 성공했을 때만 true
//***************************************************************************
bool GetFileInfoAndEncoding(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation, EEncoding& outEncoding)
{
	outEncoding = EEncoding::DEFAULT;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( lpFileInformation == nullptr ) return false;

	HANDLE hFile = ::CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		return false;

	const bool bInfoResult = ::GetFileInformationByHandle(hFile, lpFileInformation);

	// 정보 조회가 이미 실패했다면 최종 반환값은 어차피 false이므로,
	// 파일 전체를 읽을 수도 있는 인코딩 판별을 굳이 수행하지 않습니다.
	if( !bInfoResult )
	{
		::CloseHandle(hFile);
		return false;
	}

	const EEncoding encoding = DetectFileEncoding(hFile);	// 익명 네임스페이스 공용 헬퍼, 같은 TU라 접근 가능

	::CloseHandle(hFile);

	outEncoding = encoding;

	return encoding != EEncoding::DEFAULT;
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

	// BOM 확인에는 4바이트면 충분하므로, 우선 앞부분만 읽어 BOM 유무를 봅니다.
	constexpr size_t BomProbeSize = 4;
	std::ifstream file(filepath, std::ios::binary);
	if( !file )
	{
		return eEncoding;
	}

	std::vector<unsigned char> probe(BomProbeSize);
	file.read(reinterpret_cast<char*>(probe.data()), probe.size());
	size_t probeRead = static_cast<size_t>(file.gcount());

	// 파일이 너무 작아 BOM을 확인할 수 없는 경우 (기본은 ANSI 또는 DEFAULT 처리)
	if( probeRead == 0 )
	{
		return EEncoding::ANSI; // 또는 EEncoding::DEFAULT
	}

	if( probeRead >= 2 && probe[0] == UNICODE_LE_FILE_IDENTIFIER_BYTE1 && probe[1] == UNICODE_LE_FILE_IDENTIFIER_BYTE2 )
	{
		return EEncoding::UTF16_LE;		// UNICODE(LITTLE ENDIAN)
	}

	if( probeRead >= 2 && probe[0] == UNICODE_BE_FILE_IDENTIFIER_BYTE1 && probe[1] == UNICODE_BE_FILE_IDENTIFIER_BYTE2 )
	{
		return EEncoding::UTF16_BE;		// UNICODE(BIG ENDIAN)
	}

	if( probeRead >= 3 && probe[0] == UTF_FILE_IDENTIFIER_BYTE1 && probe[1] == UTF_FILE_IDENTIFIER_BYTE2 && probe[2] == UTF_FILE_IDENTIFIER_BYTE3 )
	{
		return EEncoding::UTF8_BOM;		// UTF8_BOM
	}

	// BOM이 없는 경우: Win32 경로(DetectFileEncoding)와 동일하게 파일 전체를 대상으로
	// UTF-8 휴리스틱을 수행합니다. 앞부분 몇 KB만 보면 멀티바이트 문자가 뒤쪽에만
	// 있는 UTF-8 파일을 ANSI로 오판할 수 있어, 두 API의 판별 결과가 어긋나게 됩니다.
	std::error_code ec;
	const std::uintmax_t fileSize = std::filesystem::file_size(filepath, ec);
	if( ec )
	{
		return EEncoding::DEFAULT;
	}

	// Win32 경로(DetectFileEncoding)와 동일하게 4GiB를 넘는 파일은 실패(DEFAULT)로
	// 취급합니다. vector::max_size()는 64비트 빌드에서 사실상 무제한이라 상한으로
	// 쓰기에 부적절하고, 이 값에 도달했을 때 "성공(ANSI)"으로 보는 것도 Win32
	// 경로의 "실패(DEFAULT)"와 의미가 어긋나 두 API가 같은 대용량 파일에 대해
	// 서로 다른 결론(성공/실패)을 내리게 됩니다.
	if( fileSize > static_cast<std::uintmax_t>(MAXDWORD) )
	{
		return EEncoding::DEFAULT;
	}

	file.clear();
	file.seekg(0, std::ios::beg);

	std::vector<unsigned char> buffer;

	try
	{
		buffer.resize(static_cast<size_t>(fileSize));
	}
	catch( const std::bad_alloc& )
	{
		return EEncoding::DEFAULT;
	}

	if( fileSize > 0 )
	{
		file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
		buffer.resize(static_cast<size_t>(file.gcount()));
	}

	eEncoding = IsUTF8WithoutBom(buffer.data(), buffer.size()) ? EEncoding::UTF8_NOBOM : EEncoding::ANSI;

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

	// vector<char>가 담을 수 있는 범위를 넘는 크기는 사전에 걸러, 아래 resize()가
	// std::length_error를 던지는 상황 자체를 피합니다.
	if( fileSize > static_cast<std::uintmax_t>(std::vector<char>{}.max_size()) )
	{
		return _T("");
	}

	std::ifstream file(filepath, std::ios::binary);
	if( !file )
	{
		return _T("");
	}

	std::vector<char> buffer;

	try
	{
		buffer.resize(static_cast<size_t>(fileSize));
	}
	catch( const std::bad_alloc& )
	{
		// 크기 자체는 통과했지만 실제 메모리가 부족한 경우: 이 함수의 계약대로
		// 예외를 전파하지 않고 실패를 빈 문자열로 알립니다.
		return _T("");
	}

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

		// 문자당 두 번씩 put()을 호출하는 대신, 바이트 스왑된 버퍼를 미리 만들어
		// UTF16_LE와 동일하게 한 번의 write()로 기록합니다.
		std::vector<char> beBytes(cleanContent.size() * sizeof(wchar_t));
		for( size_t i = 0; i < cleanContent.size(); ++i )
		{
			const uint16_t ch = static_cast<uint16_t>(cleanContent[i]);
			beBytes[i * 2] = static_cast<char>((ch >> 8) & 0xFF);
			beBytes[i * 2 + 1] = static_cast<char>(ch & 0xFF);
		}
		file.write(beBytes.data(), static_cast<std::streamsize>(beBytes.size()));
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

		// 문자당 두 번씩 put()을 호출하는 대신, 바이트 스왑된 버퍼를 미리 만들어
		// UTF16_LE와 동일하게 한 번의 write()로 기록합니다.
		std::vector<char> beBytes(temp.size() * sizeof(wchar_t));
		for( size_t i = 0; i < temp.size(); ++i )
		{
			const uint16_t ch = static_cast<uint16_t>(temp[i]);
			beBytes[i * 2] = static_cast<char>((ch >> 8) & 0xFF);
			beBytes[i * 2 + 1] = static_cast<char>(ch & 0xFF);
		}
		file.write(beBytes.data(), static_cast<std::streamsize>(beBytes.size()));
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

	if( !file )
		return false;

	file.close();

	return !file.fail();
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