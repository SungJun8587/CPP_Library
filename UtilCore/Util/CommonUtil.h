
//***************************************************************************
// CommonUtil.h : interface for the CommonUtil Functions.
//
//***************************************************************************

#ifndef __COMMONUTIL_H__
#define __COMMONUTIL_H__

#include <functional>
#include <random>

#ifndef __DBENUM_H__
#include <DB/DBEnum.h> 
#endif

#ifndef __ENCODINGCONVERT_H__
#include <Util/EncodingConvert.h> 
#endif

//***************************************************************************
// @namespace SYSTEM
// @brief 시스템 관련 유틸리티 기능을 제공하는 네임스페이스입니다.
//***************************************************************************
namespace SYSTEM
{
	//***************************************************************************
	// @brief 시스템의 프로세서(코어) 개수를 반환합니다.
	// @return 사용 가능한 프로세서 수 + 1 (DWORD)
	//***************************************************************************
	inline DWORD CoreCount(void)
	{
		SYSTEM_INFO	SystemInfo;
		GetSystemInfo(&SystemInfo);
		return SystemInfo.dwNumberOfProcessors + 1;
	}
};

//***************************************************************************
// @namespace SECURITY
// @brief 데이터 암호화 및 복호화 기능을 제공하는 네임스페이스입니다.
//***************************************************************************
namespace SECURITY
{
	//***************************************************************************
	// @brief 버퍼 데이터를 암호화합니다.
	// @param pBuf 암호화할 데이터 버퍼 포인터
	// @param refKey 암호화/복호화에 사용되는 키 (참조 전달되며 연산 후 갱신됨)
	// @param nLen 버퍼의 길이
	//***************************************************************************
	inline void Encrypt(char* pBuf, __int64& refKey, __int32 nLen)
	{
		if( nLen <= 0 )
			return;

		char* pKey = (char*)(&refKey);

		pBuf[0] = pBuf[0] ^ pKey[0];
		for( __int32 i = 1; i < nLen; ++i )
		{
			pBuf[i] = pBuf[i] ^ pBuf[i - 1] ^ pKey[i & 7];
		}

		refKey += nLen;
	}

	//***************************************************************************
	// @brief 암호화된 버퍼 데이터를 복호화합니다.
	// @param pBuf 복호화할 데이터 버퍼 포인터
	// @param refKey 암호화/복호화에 사용되는 키 (참조 전달되며 연산 후 갱신됨)
	// @param nLen 버퍼의 길이
	//***************************************************************************
	inline void Decrypt(char* pBuf, __int64& refKey, __int32 nLen)
	{
		if( nLen <= 0 )
			return;

		char* pKey = (char*)(&refKey);
		char source;
		char next_source;

		source = pBuf[0];
		pBuf[0] = pBuf[0] ^ pKey[0];
		for( __int32 i = 1; i < nLen; ++i )
		{
			next_source = pBuf[i];
			pBuf[i] = pBuf[i] ^ source ^ pKey[i & 7];
			source = next_source;
		}

		refKey += nLen;
	}
}

//***************************************************************************
// @brief _tstring과 멀티바이트 문자열(char*) 간의 덧셈 연산자 오버로드
//***************************************************************************
inline _tstring operator+ (const _tstring& s, const char* psz)
{
	return s + StringToTString(psz);
}

//***************************************************************************
// @brief _tstring과 와이드 문자열(wchar_t*) 간의 덧셈 연산자 오버로드
//***************************************************************************
inline _tstring operator+ (const _tstring& s, const wchar_t* pwsz)
{
	return s + WStringToTString(pwsz);
}

//***************************************************************************
// @brief _tstring과 멀티바이트 문자열(char*) 간의 동등 비교 연산자 오버로드
//***************************************************************************
inline bool operator== (const _tstring& s, const char* psz)
{
	return (s == StringToTString(psz));
}

//***************************************************************************
// @brief _tstring과 와이드 문자열(wchar_t*) 간의 동등 비교 연산자 오버로드
//***************************************************************************
inline bool operator== (const _tstring& s, const wchar_t* pwsz)
{
	return (s == WStringToTString(pwsz));
}

//***************************************************************************
// @brief _tstring과 멀티바이트 문자열(char*) 간의 비동등 비교 연산자 오버로드
//***************************************************************************
inline bool operator!= (const _tstring& s, const char* psz)
{
	return (s != StringToTString(psz));
}

//***************************************************************************
// @brief _tstring과 와이드 문자열(wchar_t*) 간의 비동등 비교 연산자 오버로드
//***************************************************************************
inline bool operator!= (const _tstring& s, const wchar_t* pwsz)
{
	return (s != WStringToTString(pwsz));
}

//***************************************************************************
// @brief 문자열의 왼쪽 공백 및 지정된 문자들을 제거합니다.
// @param s 대상 문자열
// @param t 제거할 문자 목록 (기본값: 공백 및 제어 문자)
// @return 공백이 제거된 새로운 _tstring
//***************************************************************************
inline _tstring ltrim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	size_t start = s.find_first_not_of(t);
	return (start == _tstring::npos) ? _T("") : s.substr(start);
}

//***************************************************************************
// @brief 문자열의 오른쪽 공백 및 지정된 문자들을 제거합니다.
// @param s 대상 문자열
// @param t 제거할 문자 목록 (기본값: 공백 및 제어 문자)
// @return 공백이 제거된 새로운 _tstring
//***************************************************************************
inline _tstring rtrim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	size_t end = s.find_last_not_of(t);
	return (end == _tstring::npos) ? _T("") : s.substr(0, end + 1);
}

//***************************************************************************
// @brief 문자열의 양쪽 공백 및 지정된 문자들을 제거합니다.
// @param s 대상 문자열
// @param t 제거할 문자 목록 (기본값: 공백 및 제어 문자)
// @return 양쪽 공백이 제거된 새로운 _tstring
//***************************************************************************
inline _tstring trim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	return rtrim(ltrim(s, t));
}

//***************************************************************************
// @brief 문자열 내의 특정 패턴을 다른 문자열로 모두 교체합니다.
// @param message 원본 메시지 문자열
// @param pattern 찾을 패턴 문자열
// @param replace 교체할 문자열
// @return 교체가 완료된 새로운 _tstring
//***************************************************************************
__inline _tstring replaceAll(const _tstring& message, const _tstring& pattern, const _tstring& replace)
{
	_tstring result = message;
	size_t pos = 0;
	size_t offset = 0;

	while( (pos = result.find(pattern, offset)) != _tstring::npos )
	{
		result.replace(result.begin() + pos, result.begin() + pos + pattern.size(), replace);
		offset = pos + replace.size();
	}

	return result;
}

//***************************************************************************
// @brief 가변 인자 리스트를 받아 포맷팅된 std::string을 생성합니다.
// @param pszFmt 포맷 문자열 (ANSI)
// @param args va_list 가변 인자 리스트
// @return 포맷팅된 std::string
//***************************************************************************
__inline string string_format_arg_list(const char* pszFmt, va_list args)
{
	if( !pszFmt ) return "";

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	char* pszBuffer = 0;

	while( result == -1 )
	{
		if( pszBuffer ) delete[] pszBuffer;
		pszBuffer = new char[length + 1];
		memset(pszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = vsnprintf(pszBuffer, length, pszFmt, args);
#pragma warning(pop)
		length *= 2;
	}
	string s(pszBuffer);
	delete[] pszBuffer;

	return s;
}

//***************************************************************************
// @brief 가변 인자를 받아 포맷팅된 std::string을 생성합니다.
// @tparam Args 가변 인자 타입 팩
// @param pszFmt 포맷 문자열 (ANSI)
// @param args 포맷에 전달될 인자들
// @return 포맷팅된 std::string
//***************************************************************************
template<typename ... Args>
inline string string_format(const char* pszFmt, Args ... args)
{
	if( !pszFmt ) return "";

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	char* pszBuffer = 0;

	while( result == -1 )
	{
		if( pszBuffer ) delete[] pszBuffer;
		pszBuffer = new char[length + 1];
		memset(pszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _snprintf_s(pszBuffer, length + 1, length, pszFmt, args ...);
#pragma warning(pop)
		length *= 2;
	}
	string s(pszBuffer);
	delete[] pszBuffer;

	return s;
}

//***************************************************************************
// @brief 가변 인자 리스트를 받아 포맷팅된 _tstring을 생성합니다.
// @param ptszFmt 포맷 문자열 (TCHAR)
// @param args va_list 가변 인자 리스트
// @return 포맷팅된 _tstring
//***************************************************************************
inline _tstring tstring_format_arg_list(const TCHAR* ptszFmt, va_list args)
{
	if( !ptszFmt ) return _T("");

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, (length + 1) * sizeof(TCHAR));

		// 루프마다 va_list가 소모되므로 복사해서 사용해야 합니다.
		va_list args_copy;
		va_copy(args_copy, args);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _vsntprintf_s(ptszBuffer, length + 1, length, ptszFmt, args_copy);
#pragma warning(pop)

		va_end(args_copy);
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

//***************************************************************************
// @brief ANSI 포맷 문자열과 가변 인자를 받아 _tstring을 생성합니다.
// @tparam Args 가변 인자 타입 팩
// @param pszFmt 포맷 문자열 (ANSI char*)
// @param args 포맷에 전달될 인자들
// @return 포맷팅된 _tstring
//***************************************************************************
template<typename ... Args>
inline _tstring tstring_cformat(const char* pszFmt, Args ... args)
{
	if( !pszFmt ) return _T("");

	_tstring tszFmt = StringToTString(pszFmt);

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, (length + 1) * sizeof(TCHAR));

#pragma warning(push)
#pragma warning(disable:4996)
		result = _sntprintf_s(ptszBuffer, length + 1, length, tszFmt.c_str(), args ...);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

//***************************************************************************
// @brief Unicode 포맷 문자열과 가변 인자를 받아 _tstring을 생성합니다.
// @tparam Args 가변 인자 타입 팩
// @param pwszFmt 포맷 문자열 (wchar_t*)
// @param args 포맷에 전달될 인자들
// @return 포맷팅된 _tstring
//***************************************************************************
template<typename ... Args>
inline _tstring tstring_wcformat(const wchar_t* pwszFmt, Args ... args)
{
	if( !pwszFmt ) return _T("");

	_tstring tszFmt = WStringToTString(pwszFmt);

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, (length + 1) * sizeof(TCHAR));

#pragma warning(push)
#pragma warning(disable:4996)
		result = _sntprintf_s(ptszBuffer, length + 1, length, tszFmt.c_str(), args ...);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

//***************************************************************************
// @brief TCHAR 포맷 문자열과 가변 인자를 받아 _tstring을 생성합니다.
// @tparam Args 가변 인자 타입 팩
// @param ptszFmt 포맷 문자열 (TCHAR*)
// @param args 포맷에 전달될 인자들
// @return 포맷팅된 _tstring
//***************************************************************************
template<typename ... Args>
inline _tstring tstring_tcformat(const TCHAR* ptszFmt, Args ... args)
{
	if( !ptszFmt ) return _T("");

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, (length + 1) * sizeof(TCHAR));

#pragma warning(push)
#pragma warning(disable:4996)
		result = _sntprintf_s(ptszBuffer, length + 1, length, ptszFmt, args ...);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

//***************************************************************************
// @brief 지정된 범위 내에서 균등 분포를 가지는 임의의 난수를 생성합니다.
// @tparam T 숫자 타입 (정수, 실수 등)
// @param minimum 난수 최솟값
// @param maximum 난수 최댓값
// @return 생성된 임의의 난수 값
//***************************************************************************
template<typename T>
T random(T minimum, T maximum)
{
	std::random_device rd;
	std::mt19937 engine(rd());
	std::uniform_int_distribution<T> distribution(minimum, maximum);
	return distribution(engine);
}

//***************************************************************************
// @brief 임의의 숫자 값을 환경(UNICODE 여부)에 맞는 _tstring으로 변환합니다.
// @tparam T 숫자 타입
// @param value 변환할 값
// @return 변환된 _tstring
//***************************************************************************
template<typename T>
_tstring to_tstring(T value)
{
#ifdef _UNICODE
	return to_wstring(value);
#else
	return to_string(value);
#endif
}

//***************************************************************************
// @brief 숫자 문자열에 3자리마다 쉼표(,)를 추가합니다.
// @param number 대상 64비트 정수형 숫자
// @return 쉼표가 포함된 포맷팅된 _tstring
//***************************************************************************
__inline _tstring addCommas(int64 number)
{
	bool isNegative = (number < 0);
	int64 absNumber = isNegative ? -number : number;

	_tstring numStr = to_tstring(absNumber);
	int insertPosition = static_cast<int>(numStr.length() - 3);

	while( insertPosition > 0 )
	{
		numStr.insert(insertPosition, _T(","));
		insertPosition -= 3;
	}

	return isNegative ? _T("-") + numStr : numStr;
}

// 데이터베이스 연결 및 텍스트 파싱 관련 함수 선언
void		GetDBDSNString(TCHAR* ptszDSN, const EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName);

// ADO 연결 문자열 생성 함수 선언
void		GetADOConnectionString(TCHAR* ptszConnStr, const EDBClass dbClass, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName);

EDBClass	GetInt8ToDBClass(uint8 num);
uint32		GetUInt32(const char* pszText);
uint64		GetUInt64(const char* pszText);
uint32		GetUInt32(const wchar_t* pwszText);
uint64		GetUInt64(const wchar_t* pwszText);

#endif // ndef __COMMONUTIL_H__