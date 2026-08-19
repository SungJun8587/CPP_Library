
//***************************************************************************
// DateTimeUtil.cpp : implementation of the DateTimeUtil Functions.
//
// #ifdef _WIN32 블록 : SYSTEMTIME/TIMESTAMP_STRUCT/SQL_TIMESTAMP_STRUCT 등
//                      Windows/ODBC 전용 타입에 묶인 함수들.
// ptime 네임스페이스 : 플랫폼 독립 함수들. Windows/POSIX 시그니처 차이가
//                      있는 지점(localtime_s/r, gmtime_s/r, _mkgmtime/timegm)
//                      만 내부적으로 다시 #ifdef _WIN32 로 분기합니다.
//***************************************************************************

#include "pch.h"
#include "DateTimeUtil.h"

#include <sstream>
#include <iomanip>
#include <thread>

//***************************************************************************
// Windows 전용 (SYSTEMTIME / TIMESTAMP_STRUCT / SQL_TIMESTAMP_STRUCT)
//***************************************************************************
#ifdef _WIN32

//***************************************************************************
// @brief TIMESTAMP_STRUCT 데이터를 time_t로 변환하여 할당합니다.
// @param t 대상 time_t 참조
// @param ts 변환할 TIMESTAMP_STRUCT 상수 참조
// @return 변환된 time_t 참조
//***************************************************************************
time_t& operator<<(time_t& t, const TIMESTAMP_STRUCT& ts)
{
	SYSTEMTIME st;
	st << ts;
	t << st;

	return t;
}

//***************************************************************************
// @brief time_t 데이터를 TIMESTAMP_STRUCT로 변환하여 할당합니다.
// @param ts 대상 TIMESTAMP_STRUCT 참조
// @param t 변환할 time_t 상수 참조
// @return 변환된 TIMESTAMP_STRUCT 참조
//***************************************************************************
TIMESTAMP_STRUCT& operator<<(TIMESTAMP_STRUCT& ts, const time_t& t)
{
	SYSTEMTIME st;
	st << t;
	ts << st;

	return ts;
}

//***************************************************************************
// @brief SYSTEMTIME 데이터를 time_t로 변환하여 할당합니다.
// @param t 대상 time_t 참조
// @param stime 변환할 SYSTEMTIME 상수 참조
// @return 변환된 time_t 참조
//***************************************************************************
time_t& operator<<(time_t& t, const SYSTEMTIME& stime)
{
	struct tm tmp;

	memset(&tmp, 0, sizeof(struct tm));

	tmp.tm_year = stime.wYear - 1900;
	tmp.tm_mon = stime.wMonth - 1;
	tmp.tm_mday = stime.wDay;

	tmp.tm_hour = stime.wHour;
	tmp.tm_min = stime.wMinute;
	tmp.tm_sec = stime.wSecond;
	tmp.tm_isdst = 1;

	t = mktime(&tmp);

	return t;
}

//***************************************************************************
// @brief time_t 데이터를 SYSTEMTIME으로 변환하여 할당합니다.
// @param stime 대상 SYSTEMTIME 참조
// @param t 변환할 time_t 상수 참조
// @return 변환된 SYSTEMTIME 참조
//***************************************************************************
SYSTEMTIME& operator<<(SYSTEMTIME& stime, const time_t& t)
{
	struct tm tmp;
	memset(&tmp, 0, sizeof(struct tm));
	memset(&stime, 0, sizeof(SYSTEMTIME));

	localtime_s(&tmp, &t);

	stime.wYear = tmp.tm_year + 1900;
	stime.wMonth = tmp.tm_mon + 1;
	stime.wDay = tmp.tm_mday;

	stime.wHour = tmp.tm_hour;
	stime.wMinute = tmp.tm_min;
	stime.wSecond = tmp.tm_sec;

	// 아래 주석 사유 : local -> GMT 로 변경되어 주석함 
	// (t가 local time 이라면 st 는 GMT TIME 으로 변환한다.)

// 	FILETIME ft;
// 
// 	LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
// 	ft.dwLowDateTime = (DWORD)ll;
// 	ft.dwHighDateTime = ll >> 32;
// 
// 	FileTimeToSystemTime(&ft, &st);

	return stime;
}

//***************************************************************************
// @brief SYSTEMTIME 데이터를 TIMESTAMP_STRUCT로 변환하여 할당합니다.
// @param ts 대상 TIMESTAMP_STRUCT 참조
// @param stime 변환할 SYSTEMTIME 상수 참조
// @return 변환된 TIMESTAMP_STRUCT 참조
//***************************************************************************
TIMESTAMP_STRUCT& operator<<(TIMESTAMP_STRUCT& ts, const SYSTEMTIME& stime)
{
	ts.year = stime.wYear;
	ts.month = stime.wMonth;
	ts.day = stime.wDay;
	ts.hour = stime.wHour;
	ts.minute = stime.wMinute;
	ts.second = stime.wSecond;
	ts.fraction = stime.wMilliseconds * 1000000;
	return ts;
}

//***************************************************************************
// @brief TIMESTAMP_STRUCT 데이터를 SYSTEMTIME으로 변환하여 할당합니다.
// @param stime 대상 SYSTEMTIME 참조
// @param ts 변환할 TIMESTAMP_STRUCT 상수 참조
// @return 변환된 SYSTEMTIME 참조
//***************************************************************************
SYSTEMTIME& operator<<(SYSTEMTIME& stime, const TIMESTAMP_STRUCT& ts)
{
	stime.wYear = ts.year;
	stime.wMonth = ts.month;
	stime.wDay = ts.day;
	stime.wHour = ts.hour;
	stime.wMinute = ts.minute;
	stime.wSecond = ts.second;
	stime.wMilliseconds = static_cast<WORD>(ts.fraction / 1000000);
	return stime;
}

//***************************************************************************
// @brief 두 SYSTEMTIME 값의 동등 여부를 비교합니다.
// @param stime1 비교할 첫 번째 SYSTEMTIME 참조
// @param stime2 비교할 두 번째 SYSTEMTIME 참조
// @return 같으면 true, 다르면 false
//***************************************************************************
bool  operator==(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	FILETIME ft1, ft2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &ft1);
	SystemTimeToFileTime(&stime2, &ft2);

	pnVal1 = (ULONGLONG*)&ft1;
	pnVal2 = (ULONGLONG*)&ft2;

	return (*pnVal1 == *pnVal2);
}

//************************************************Y**************************
// @brief 첫 번째 SYSTEMTIME이 두 번째 SYSTEMTIME보다 큰지 비교합니다.
// @param stime1 비교할 첫 번째 SYSTEMTIME 참조
// @param stime2 비교할 두 번째 SYSTEMTIME 참조
// @return stime1이 더 크면 true, 아니면 false
//***************************************************************************
bool  operator>(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	FILETIME ft1, ft2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &ft1);
	SystemTimeToFileTime(&stime2, &ft2);

	pnVal1 = (ULONGLONG*)&ft1;
	pnVal2 = (ULONGLONG*)&ft2;

	return (*pnVal1 > *pnVal2);
}

//***************************************************************************
// @brief 첫 번째 SYSTEMTIME이 두 번째 SYSTEMTIME보다 크거나 같은지 비교합니다.
// @param stime1 비교할 첫 번째 SYSTEMTIME 참조
// @param stime2 비교할 두 번째 SYSTEMTIME 참조
// @return stime1이 크거나 같으면 true, 아니면 false
//***************************************************************************
bool  operator>=(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	FILETIME ft1, ft2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &ft1);
	SystemTimeToFileTime(&stime2, &ft2);

	pnVal1 = (ULONGLONG*)&ft1;
	pnVal2 = (ULONGLONG*)&ft2;

	return (*pnVal1 >= *pnVal2);
}

//***************************************************************************
// @brief 첫 번째 SYSTEMTIME이 두 번째 SYSTEMTIME보다 작은지 비교합니다.
// @param stime1 비교할 첫 번째 SYSTEMTIME 참조
// @param stime2 비교할 두 번째 SYSTEMTIME 참조
// @return stime1이 더 작으면 true, 아니면 false
//***************************************************************************
bool  operator<(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	FILETIME ft1, ft2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &ft1);
	SystemTimeToFileTime(&stime2, &ft2);

	pnVal1 = (ULONGLONG*)&ft1;
	pnVal2 = (ULONGLONG*)&ft2;

	return (*pnVal1 < *pnVal2);
}

//***************************************************************************
// @brief 첫 번째 SYSTEMTIME이 두 번째 SYSTEMTIME보다 작거나 같은지 비교합니다.
// @param stime1 비교할 첫 번째 SYSTEMTIME 참조
// @param stime2 비교할 두 번째 SYSTEMTIME 참조
// @return stime1이 작거나 같으면 true, 아니면 false
//***************************************************************************
bool  operator<=(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	FILETIME ft1, ft2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &ft1);
	SystemTimeToFileTime(&stime2, &ft2);

	pnVal1 = (ULONGLONG*)&ft1;
	pnVal2 = (ULONGLONG*)&ft2;

	return (*pnVal1 <= *pnVal2);
}

//***************************************************************************
// @brief 두 SYSTEMTIME 간의 시간 차이를 초(sec) 단위로 계산합니다.
// @param stime1 첫 번째 SYSTEMTIME 상수 참조
// @param stime2 두 번째 SYSTEMTIME 상수 참조 (기준)
// @return 초 단위 시간 차이 (stime1이 이전이거나 같으면 0)
//***************************************************************************
uint64 operator-(const SYSTEMTIME& stime1, const SYSTEMTIME& stime2)
{
	FILETIME fTm1, fTm2;
	uint64* pnVal1, * pnVal2;

	SystemTimeToFileTime(&stime1, &fTm1);
	SystemTimeToFileTime(&stime2, &fTm2);

	pnVal1 = (ULONGLONG*)&fTm1;
	pnVal2 = (ULONGLONG*)&fTm2;
	if( *pnVal1 <= *pnVal2 )
		return 0;

	return (*pnVal1 - *pnVal2) / TIME::IN_SEC;
}

//***************************************************************************
// @brief 문자열 형태의 날짜/시간을 파싱하여 SYSTEMTIME 구조체에 할당합니다.
// @param stime 대상 SYSTEMTIME 참조
// @param tszDateTime "YYYY-MM-DD HH:MM:SS" 형식의 문자열
// @return 변환된 SYSTEMTIME 참조
//***************************************************************************
SYSTEMTIME& operator<<(SYSTEMTIME& stime, TCHAR* tszDateTime)
{
	_stscanf_s(tszDateTime, _T("%hu-%hu-%hu %hu:%hu:%hu")
		, &stime.wYear, &stime.wMonth, &stime.wDay
		, &stime.wHour, &stime.wMinute, &stime.wSecond);

	return stime;
}

//***************************************************************************
// @brief SYSTEMTIME 구조체 값을 포맷팅된 날짜/시간 문자열로 변환합니다.
// @param tszDateTime 결과를 저장할 TCHAR 버퍼
// @param stime 변환할 SYSTEMTIME 참조
// @return 변환된 TCHAR 문자열 포인터
//***************************************************************************
TCHAR* operator<<(TCHAR* tszDateTime, SYSTEMTIME& stime)
{
	_sntprintf_s(tszDateTime, STD_DATETIME_STRLEN, _TRUNCATE, _T("%04d-%02d-%02d %02d:%02d:%02d")
		, stime.wYear, stime.wMonth, stime.wDay
		, stime.wHour, stime.wMinute, stime.wSecond);

	return tszDateTime;
}

//***************************************************************************
// @brief SYSTEMTIME 구조체를 time_t 타임스탬프로 변환합니다.
// @param tTime 변환할 SYSTEMTIME 객체
// @return 변환된 time_t 값
//***************************************************************************
time_t GetTimestampToDateTime(const SYSTEMTIME tTime)
{
	struct tm time;

	time.tm_sec = tTime.wSecond;					/* seconds after the minute - [0,59] */
	time.tm_min = tTime.wMinute;					/* minutes after the hour - [0,59] */
	time.tm_hour = tTime.wHour;						/* hours since midnight - [0,23] */
	time.tm_mday = tTime.wDay;						/* day of the month - [1,31] */
	time.tm_mon = tTime.wMonth - 1;					/* months since January - [0,11] */
	time.tm_year = tTime.wYear - 1900;				/* years since 1900 */
	time.tm_wday = 0;								/* days since Sunday - [0,6] */
	time.tm_yday = 0;								/* days since January 1 - [0,365] */
	time.tm_isdst = 0;								/* daylight savings time flag */

	return mktime(&time);
}

//***************************************************************************
// @brief time_t 타임스탬프를 SYSTEMTIME 구조체로 변환합니다.
// @param tTime 결과를 저장할 SYSTEMTIME 참조
// @param timestamp 변환할 time_t 값
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetSystemTimeToTimestamp(SYSTEMTIME& tTime, const time_t timestamp)
{
	struct tm tm_info;

	errno_t err_no = localtime_s(&tm_info, &timestamp);
	if( err_no > 0 ) return false;

	tTime.wYear = tm_info.tm_year + 1900;
	tTime.wMonth = tm_info.tm_mon + 1;
	tTime.wDay = tm_info.tm_mday;
	tTime.wHour = tm_info.tm_hour;
	tTime.wMinute = tm_info.tm_min;
	tTime.wSecond = tm_info.tm_sec;

	return true;
}

//***************************************************************************
// @brief YYYYMMDDhhmmss 형식의 문자열이 유효한 날짜 및 시간인지 검증합니다.
// @param ptszDateTime 검증할 날짜/시간 문자열
// @return 유효하면 true, 아니면 false
//***************************************************************************
bool  IsValidDateTime(const TCHAR* ptszDateTime)
{
	int i = 0;

	for( i = 0; i < 15; i++ )
		if( *(ptszDateTime + i) == NULL ) break;

	if( i < 14 ) return false;

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	int nYear = (ptszDateTime[0] - '0') * 1000 + (ptszDateTime[1] - '0') * 100 + (ptszDateTime[2] - '0') * 10 + (ptszDateTime[3] - '0');
	int nMonth = (ptszDateTime[4] - '0') * 10 + (ptszDateTime[5] - '0');
	int nDay = (ptszDateTime[6] - '0') * 10 + (ptszDateTime[7] - '0');

	if( nYear < 1 || nMonth < 1 || nMonth > 12 || nDay < 1 ) return false;

	if( nYear % 4 == 0 && nYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( nYear % 4 == 0 && nYear % 100 == 0 && nYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( nDay > nMonthList[nMonth] ) return false;

	int nHour = (ptszDateTime[8] - '0') * 10 + (ptszDateTime[9] - '0');
	int nMin = (ptszDateTime[10] - '0') * 10 + (ptszDateTime[11] - '0');
	int nSec = (ptszDateTime[12] - '0') * 10 + (ptszDateTime[13] - '0');

	if( nHour < 0 || nHour > 23 || nMin < 0 || nMin > 59 || nSec < 0 || nSec > 59 ) return false;

	return true;
}

//***************************************************************************
// @brief 문자열 날짜/시간을 SQL_TIMESTAMP_STRUCT 구조체로 변환합니다.
// @param stDateTime 결과 SQL_TIMESTAMP_STRUCT 참조
// @param ptszDateTime 변환할 날짜/시간 문자열 (YYYYMMDDhhmmss)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetSqlTime(SQL_TIMESTAMP_STRUCT& stDateTime, const TCHAR* ptszDateTime)
{
	int nYear(0), nMonth(0), nDay(0), nHour(0), nMinute(0), nSec(0);

	if( !IsValidDateTime(ptszDateTime) ) return false;

	nYear = (ptszDateTime[0] - '0') * 1000 + (ptszDateTime[1] - '0') * 100 +
		(ptszDateTime[2] - '0') * 10 + (ptszDateTime[3] - '0');
	nMonth = (ptszDateTime[4] - '0') * 10 + (ptszDateTime[5] - '0');
	nDay = (ptszDateTime[6] - '0') * 10 + (ptszDateTime[7] - '0');
	nHour = (ptszDateTime[8] - '0') * 10 + (ptszDateTime[9] - '0');
	nMinute = (ptszDateTime[10] - '0') * 10 + (ptszDateTime[11] - '0');
	nSec = (ptszDateTime[12] - '0') * 10 + (ptszDateTime[13] - '0');

	if( nHour > 23 || nMinute > 59 || nSec > 59 ) return false;

	stDateTime.year = nYear;
	stDateTime.month = nMonth;
	stDateTime.day = nDay;
	stDateTime.hour = nHour;
	stDateTime.minute = nMinute;
	stDateTime.second = nSec;

	return true;
}

//***************************************************************************
// @brief 문자열 날짜/시간을 SYSTEMTIME 구조체로 변환합니다.
// @param tTime 결과 SYSTEMTIME 참조
// @param ptszDateTime 변환할 날짜/시간 문자열 (YYYYMMDDhhmmss)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetSystemTime(SYSTEMTIME& tTime, const TCHAR* ptszDateTime)
{
	int nYear(0), nMonth(0), nDay(0), nHour(0), nMinute(0), nSec(0);

	if( !IsValidDateTime(ptszDateTime) ) return false;

	nYear = (ptszDateTime[0] - '0') * 1000 + (ptszDateTime[1] - '0') * 100 +
		(ptszDateTime[2] - '0') * 10 + (ptszDateTime[3] - '0');
	nMonth = (ptszDateTime[4] - '0') * 10 + (ptszDateTime[5] - '0');
	nDay = (ptszDateTime[6] - '0') * 10 + (ptszDateTime[7] - '0');
	nHour = (ptszDateTime[8] - '0') * 10 + (ptszDateTime[9] - '0');
	nMinute = (ptszDateTime[10] - '0') * 10 + (ptszDateTime[11] - '0');
	nSec = (ptszDateTime[12] - '0') * 10 + (ptszDateTime[13] - '0');

	if( nHour > 23 || nMinute > 59 || nSec > 59 ) return false;

	tTime.wYear = nYear;
	tTime.wMonth = nMonth;
	tTime.wDay = nDay;
	tTime.wHour = nHour;
	tTime.wMinute = nMinute;
	tTime.wSecond = nSec;

	return true;
}

//***************************************************************************
// @brief SYSTEMTIME을 표준 GMT(UTC) 형식의 문자열로 변환합니다.
// @param ptszStdDateTime 결과를 저장할 TCHAR 버퍼
// @param sTime 변환할 SYSTEMTIME 객체
//***************************************************************************
void GetGMTTime(TCHAR* ptszStdDateTime, SYSTEMTIME sTime)
{
	const static TCHAR atszWdayList[][4] = { _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat") };
	const static TCHAR atszMonList[][4] = { _T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"), _T("May"), _T("Jun"), _T("Jul"), _T("Aug"), _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec") };

	_stprintf_s(ptszStdDateTime, STD_DATETIME_STRLEN, _T("%s, %d %s %04d %02d:%02d:%02d GMT"), atszWdayList[sTime.wDayOfWeek],
		sTime.wDay, atszMonList[sTime.wMonth - 1], sTime.wYear, sTime.wHour, sTime.wMinute, sTime.wSecond);
}

//***************************************************************************
// @brief 두 SYSTEMTIME 간의 분(minute) 단위 간격을 계산합니다.
// @param stime1 첫 번째 SYSTEMTIME 참조
// @param stime2 두 번째 SYSTEMTIME 참조 (기준)
// @return 분 단위 시간 차이 (stime2가 이전이거나 같으면 0)
//***************************************************************************
uint64 TIME::DifMinute(SYSTEMTIME& stime1, SYSTEMTIME& stime2)
{
	uint64 nRet = 0;

	FILETIME fTm1, fTm2;
	uint64* pnVal1, * pnVal2;
	uint64 nDiff;

	SystemTimeToFileTime(&stime1, &fTm1);
	SystemTimeToFileTime(&stime2, &fTm2);

	pnVal1 = (ULONGLONG*)&fTm1;
	pnVal2 = (ULONGLONG*)&fTm2;
	if( *pnVal2 <= *pnVal1 )
		return nRet;

	nDiff = *pnVal2 - *pnVal1;
	nRet = uint64(nDiff / 10000000 / 60);

	return nRet;
}

//***************************************************************************
// @brief SYSTEMTIME 객체에 지정된 시간(100나노초 단위 틱)을 더합니다.
// @param stime 연산 대상 SYSTEMTIME 참조
// @param nAddTime 더할 시간 값 (100나노초 단위)
//***************************************************************************
void TIME::IncreaseSystemTime(SYSTEMTIME& stime, __int64 nAddTime)
{
	FILETIME ftm;
	LARGE_INTEGER largeInt;

	SystemTimeToFileTime(&stime, &ftm);

	memcpy(&largeInt, &ftm, sizeof(FILETIME));
	largeInt.QuadPart += nAddTime;
	memcpy(&ftm, &largeInt, sizeof(FILETIME));

	FileTimeToSystemTime(&ftm, &stime);
}

//***************************************************************************
// @brief FILETIME(UTC)을 로컬 시각 "YYYY-MM-DD HH:MM:SS" 문자열로 변환합니다.
// @detail FileTimeToLocalFileTime + FileTimeToSystemTime + operator<<(TCHAR*,
//         SYSTEMTIME&)를 감싼 편의 함수입니다. 파일 생성/수정 시각처럼
//         WIN32_FIND_DATA 등에서 얻은 FILETIME을 바로 문자열로 쓸 때 사용합니다.
// @param ft 변환할 FILETIME (UTC 기준)
// @return "YYYY-MM-DD HH:MM:SS" 형식의 로컬 시각 문자열
//***************************************************************************
_tstring FileTimeToLocalString(const FILETIME& ft)
{
	FILETIME ftLocal;
	SYSTEMTIME stime;

	FileTimeToLocalFileTime(&ft, &ftLocal);
	FileTimeToSystemTime(&ftLocal, &stime);

	TCHAR tszDateTime[STD_DATETIME_STRLEN] = { 0 };
	tszDateTime << stime;

	return tszDateTime;
}

#endif // _WIN32

// ===========================================================================
// 크로스플랫폼 (ptime 네임스페이스)
// ===========================================================================
namespace ptime
{
	//***************************************************************************
	// @brief 현재 시각을 UNIX epoch 기준 밀리초로 반환합니다.
	// @return epoch 이후 경과 밀리초
	//***************************************************************************
	int64_t NowMillis()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			Clock::now().time_since_epoch()).count();
	}

	//***************************************************************************
	// @brief 현재 시각을 UNIX epoch 기준 마이크로초로 반환합니다.
	// @return epoch 이후 경과 마이크로초
	//***************************************************************************
	int64_t NowMicros()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(
			Clock::now().time_since_epoch()).count();
	}

	//***************************************************************************
	// @brief 단조 증가 클럭(steady_clock) 기준 현재 시각을 초 단위로 반환합니다.
	// @detail 시스템 시간이 변경(NTP 보정, 사용자 변경 등)되어도 영향받지 않으므로
	//         절대 시각이 아닌 경과 시간 측정 용도로만 사용해야 합니다.
	// @return steady_clock epoch 이후 경과 초
	//***************************************************************************
	double MonotonicNowSec()
	{
		return std::chrono::duration<double>(SteadyClock::now().time_since_epoch()).count();
	}

	//***************************************************************************
	// @brief thread-safe localtime 변환 (Windows: localtime_s, POSIX: localtime_r)
	// @param out 결과가 채워질 std::tm 참조
	// @param t 변환할 time_t 값
	// @return 성공 시 true
	//***************************************************************************
	bool LocalTimeSafe(std::tm& out, const time_t& t)
	{
#ifdef _WIN32
		return localtime_s(&out, &t) == 0;
#else
		return localtime_r(&t, &out) != nullptr;
#endif
	}

	//***************************************************************************
	// @brief thread-safe gmtime 변환 (Windows: gmtime_s, POSIX: gmtime_r)
	// @param out 결과가 채워질 std::tm 참조
	// @param t 변환할 time_t 값
	// @return 성공 시 true
	//***************************************************************************
	bool GmTimeSafe(std::tm& out, const time_t& t)
	{
#ifdef _WIN32
		return gmtime_s(&out, &t) == 0;
#else
		return gmtime_r(&t, &out) != nullptr;
#endif
	}

	//***************************************************************************
	// @brief time_t를 ISO 8601 형식 문자열("YYYY-MM-DDThh:mm:ss")로 변환합니다.
	// @param t 변환할 time_t 값
	// @return ISO 8601 문자열, 변환 실패 시 빈 문자열
	//***************************************************************************
	_tstring ToIso8601(const time_t& t)
	{
		return Format(t, _T("%Y-%m-%dT%H:%M:%S"));
	}

	//***************************************************************************
	// @brief time_t를 strftime 포맷 문자열에 따라 변환합니다. (로컬 타임 기준)
	// @param t 변환할 time_t 값
	// @param fmt strftime 포맷 (예: _T("%Y%m%d"), _T("%Y-%m"))
	// @return 포맷팅된 문자열, 변환 실패 시 빈 문자열
	//***************************************************************************
	_tstring Format(const time_t& t, const TCHAR* fmt)
	{
		std::tm tmVal{};
		if( !LocalTimeSafe(tmVal, t) ) return {};

		_tstringstream oss;
		oss << std::put_time(&tmVal, fmt);
		return oss.str();
	}

	//***************************************************************************
	// @brief 현재 시각을 지정한 strftime 포맷으로 반환합니다.
	// @param fmt strftime 포맷
	// @return 포맷팅된 문자열
	//***************************************************************************
	_tstring FormatNow(const TCHAR* fmt)
	{
		return Format(Clock::to_time_t(Clock::now()), fmt);
	}

	//***************************************************************************
	// 아래는 Format()을 감싼 편의 함수들입니다. (t 생략 시 현재 시각 사용)
	// @return 예) ToYYYY: "2026", ToYYYYMM: "202608", ToYYYYMMDD: "20260819"
	//***************************************************************************
	_tstring ToYYYY(const time_t& t) { return Format(t, _T("%Y")); }
	_tstring ToYYYYMM(const time_t& t) { return Format(t, _T("%Y%m")); }
	_tstring ToYYYYMMDD(const time_t& t) { return Format(t, _T("%Y%m%d")); }
	_tstring ToMMDD(const time_t& t) { return Format(t, _T("%m%d")); }
	_tstring ToHHMMSS(const time_t& t) { return Format(t, _T("%H%M%S")); }
	_tstring ToHHMM(const time_t& t) { return Format(t, _T("%H%M")); }
	_tstring ToYYYYMMDDHHMMSS(const time_t& t) { return Format(t, _T("%Y%m%d%H%M%S")); }
	_tstring ToYYYY_MM_DD(const time_t& t) { return Format(t, _T("%Y-%m-%d")); }

	_tstring ToYYYY() { return FormatNow(_T("%Y")); }
	_tstring ToYYYYMM() { return FormatNow(_T("%Y%m")); }
	_tstring ToYYYYMMDD() { return FormatNow(_T("%Y%m%d")); }
	_tstring ToMMDD() { return FormatNow(_T("%m%d")); }
	_tstring ToHHMMSS() { return FormatNow(_T("%H%M%S")); }
	_tstring ToHHMM() { return FormatNow(_T("%H%M")); }
	_tstring ToYYYYMMDDHHMMSS() { return FormatNow(_T("%Y%m%d%H%M%S")); }
	_tstring ToYYYY_MM_DD() { return FormatNow(_T("%Y-%m-%d")); }

	//***************************************************************************
	// @brief 지정한 밀리초만큼 현재 스레드를 대기시킵니다.
	// @param ms 대기 시간(밀리초)
	//***************************************************************************
	void SleepMillis(int64_t ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	//***************************************************************************
	// 양력(그레고리력) 계산
	//***************************************************************************

	//***************************************************************************
	// @brief 지정한 연도가 그레고리력 기준 윤년인지 확인합니다.
	// @param year 연도
	// @return 윤년이면 true
	//***************************************************************************
	bool IsLeapYear(int year)
	{
		if( year % 4 != 0 ) return false;
		if( year % 100 != 0 ) return true;
		return (year % 400 == 0);
	}

	//***************************************************************************
	// @brief 지정한 연/월의 마지막 일(말일)을 반환합니다.
	// @param year 연도
	// @param month 월 (1~12)
	// @return 말일(28~31), month가 범위를 벗어나면 0
	//***************************************************************************
	int LastDayOfMonth(int year, int month)
	{
		static const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		if( month < 1 || month > 12 ) return 0;
		if( month == 2 && IsLeapYear(year) ) return 29;

		return daysInMonth[month - 1];
	}

	//***************************************************************************
	// @brief t가 속한 연/월의 마지막 일(말일)을 반환합니다.
	// @param t 기준 time_t 값
	// @return 말일, 변환 실패 시 0
	//***************************************************************************
	int LastDayOfMonth(const time_t& t)
	{
		std::tm tmVal{};
		if( !LocalTimeSafe(tmVal, t) ) return 0;

		return LastDayOfMonth(tmVal.tm_year + 1900, tmVal.tm_mon + 1);
	}

	//***************************************************************************
	// @brief 지정한 연/월/일의 요일 인덱스를 계산합니다.
	// @detail std::mktime이 struct tm을 정규화하며 tm_wday를 채워주는 것을
	//         이용합니다. DST 경계에서 날짜가 밀리는 것을 피하기 위해
	//         시각을 정오(12시)로 고정합니다.
	// @param year 연도
	// @param month 월 (1~12)
	// @param day 일
	// @return 요일 인덱스 (0=일요일 ~ 6=토요일), 실패 시 -1
	//***************************************************************************
	int DayOfWeekIndex(int year, int month, int day)
	{
		std::tm tmVal{};
		tmVal.tm_year = year - 1900;
		tmVal.tm_mon = month - 1;
		tmVal.tm_mday = day;
		tmVal.tm_hour = 12;

		if( std::mktime(&tmVal) == static_cast<time_t>(-1) ) return -1;

		return tmVal.tm_wday;
	}

	//***************************************************************************
	// @brief t의 요일 인덱스를 반환합니다.
	// @param t 기준 time_t 값
	// @return 요일 인덱스 (0=일요일 ~ 6=토요일), 실패 시 -1
	//***************************************************************************
	int DayOfWeekIndex(const time_t& t)
	{
		std::tm tmVal{};
		if( !LocalTimeSafe(tmVal, t) ) return -1;

		return tmVal.tm_wday;
	}

	//***************************************************************************
	// @brief 요일 인덱스를 한글 요일명으로 변환합니다.
	// @param dayOfWeekIndex 0=일요일 ~ 6=토요일
	// @return 한글 요일명("일요일" 등), 범위를 벗어나면 빈 문자열
	//***************************************************************************
	_tstring WeekdayNameKo(int dayOfWeekIndex)
	{
		static const TCHAR* names[] = { _T("일요일"), _T("월요일"), _T("화요일"), _T("수요일"), _T("목요일"), _T("금요일"), _T("토요일") };

		if( dayOfWeekIndex < 0 || dayOfWeekIndex > 6 ) return {};
		return names[dayOfWeekIndex];
	}

	//***************************************************************************
	// @brief t의 한글 요일명을 반환합니다.
	// @param t 기준 time_t 값
	// @return 한글 요일명, 실패 시 빈 문자열
	//***************************************************************************
	_tstring WeekdayNameKo(const time_t& t)
	{
		return WeekdayNameKo(DayOfWeekIndex(t));
	}

	//***************************************************************************
	// @brief 요일 인덱스를 영문 요일명으로 변환합니다.
	// @param dayOfWeekIndex 0=Sunday ~ 6=Saturday
	// @return 영문 요일명("Sunday" 등), 범위를 벗어나면 빈 문자열
	//***************************************************************************
	_tstring WeekdayNameEn(int dayOfWeekIndex)
	{
		static const TCHAR* names[] = { _T("Sunday"), _T("Monday"), _T("Tuesday"), _T("Wednesday"), _T("Thursday"), _T("Friday"), _T("Saturday") };

		if( dayOfWeekIndex < 0 || dayOfWeekIndex > 6 ) return {};
		return names[dayOfWeekIndex];
	}

	//***************************************************************************
	// @brief t의 영문 요일명을 반환합니다.
	// @param t 기준 time_t 값
	// @return 영문 요일명, 실패 시 빈 문자열
	//***************************************************************************
	_tstring WeekdayNameEn(const time_t& t)
	{
		return WeekdayNameEn(DayOfWeekIndex(t));
	}

	//***************************************************************************
	// @brief 월 번호(1~12)를 영문 월 이름으로 변환합니다.
	// @param month 월 번호 (1~12)
	// @return 영문 월 이름("January" 등), 범위를 벗어나면 빈 문자열
	//***************************************************************************
	_tstring MonthNameEn(int month)
	{
		static const TCHAR* names[] =
		{
			_T("January"), _T("February"), _T("March"), _T("April"), _T("May"), _T("June"),
			_T("July"), _T("August"), _T("September"), _T("October"), _T("November"), _T("December")
		};

		if( month < 1 || month > 12 ) return {};
		return names[month - 1];
	}

	//***************************************************************************
	// @brief 월 번호(1~12)를 한글 월 표기("1월" 등)로 변환합니다.
	// @param month 월 번호 (1~12)
	// @return 한글 월 표기, 범위를 벗어나면 빈 문자열
	//***************************************************************************
	_tstring MonthNameKo(int month)
	{
		if( month < 1 || month > 12 ) return {};

		_tstringstream oss;
		oss << month << _T("월");
		return oss.str();
	}

	//***************************************************************************
	// 기준 시각 연산
	//***************************************************************************

	//***************************************************************************
	// @brief 현재 시각을 time_t로 반환합니다.
	// @return 현재 time_t 값
	//***************************************************************************
	time_t Now()
	{
		return Clock::to_time_t(Clock::now());
	}

	//***************************************************************************
	// @brief t에 지정한 초(음수 가능)를 더한 time_t를 반환합니다.
	// @param t 기준 time_t 값
	// @param seconds 더할 초 (음수면 과거로 이동)
	// @return 계산된 time_t 값
	//***************************************************************************
	time_t AddSeconds(const time_t& t, int64_t seconds)
	{
		return static_cast<time_t>(t + seconds);
	}

	//***************************************************************************
	// @brief t가 속한 날짜에 지정한 일수(음수 가능)를 더한 time_t를 반환합니다.
	// @detail 로컬 달력 필드(년/월/일)에 일수를 더한 뒤 mktime으로 정규화하므로
	//         월말/윤년 경계를 자동으로 처리합니다.
	//         (예: AddDays(Now(), -1) 이 어제를 의미합니다.)
	// @param t 기준 time_t 값
	// @param days 더할 일수 (음수면 과거로 이동)
	// @return 계산된 time_t 값, 변환 실패 시 t 그대로 반환
	//***************************************************************************
	time_t AddDays(const time_t& t, int days)
	{
		std::tm tmVal{};
		if( !LocalTimeSafe(tmVal, t) ) return t;

		tmVal.tm_mday += days;
		tmVal.tm_isdst = -1;

		time_t result = std::mktime(&tmVal);
		return (result == static_cast<time_t>(-1)) ? t : result;
	}

	//***************************************************************************
	// @brief 두 time_t 간의 초 단위 차이를 계산합니다. (t1 - t2)
	// @param t1 첫 번째 time_t 값
	// @param t2 두 번째 time_t 값 (기준)
	// @return 초 단위 차이 (t1이 이후면 양수)
	//***************************************************************************
	long SecondsBetween(const time_t& t1, const time_t& t2)
	{
		return static_cast<long>(std::difftime(t1, t2));
	}

	//***************************************************************************
	// @brief 두 time_t 간의 분 단위 차이를 계산합니다. (t1 - t2)
	// @param t1 첫 번째 time_t 값
	// @param t2 두 번째 time_t 값 (기준)
	// @return 분 단위 차이 (t1이 이후면 양수)
	//***************************************************************************
	long MinutesBetween(const time_t& t1, const time_t& t2)
	{
		return SecondsBetween(t1, t2) / 60;
	}

	//***************************************************************************
	// 문자열 파싱/검증
	//***************************************************************************

	//***************************************************************************
	// @brief TCHAR 문자가 '0'~'9' 범위인지 확인합니다. (로케일에 의존하지
	//        않도록 std::isdigit/iswdigit 대신 직접 비교합니다.)
	// @param c 검사할 문자
	// @return 숫자 문자이면 true
	//***************************************************************************
	static bool IsDigitChar(TCHAR c)
	{
		return c >= _T('0') && c <= _T('9');
	}

	//***************************************************************************
	// @brief YYYYMMDD(8자리) 문자열이 유효한 날짜인지 검증합니다.
	// @param yyyymmdd 검증할 문자열
	// @return 유효하면 true
	//***************************************************************************
	bool IsValidDate(const _tstring& yyyymmdd)
	{
		if( yyyymmdd.size() != 8 ) return false;
		for( TCHAR c : yyyymmdd ) if( !IsDigitChar(c) ) return false;

		int year = std::stoi(yyyymmdd.substr(0, 4));
		int month = std::stoi(yyyymmdd.substr(4, 2));
		int day = std::stoi(yyyymmdd.substr(6, 2));

		if( year < 1 || month < 1 || month > 12 || day < 1 ) return false;
		return day <= LastDayOfMonth(year, month);
	}

	//***************************************************************************
	// @brief YYYYMMDDHHMMSS(14자리) 문자열이 유효한 날짜/시간인지 검증합니다.
	// @param yyyymmddhhmmss 검증할 문자열
	// @return 유효하면 true
	//***************************************************************************
	bool IsValidDateTime(const _tstring& yyyymmddhhmmss)
	{
		if( yyyymmddhhmmss.size() != 14 ) return false;
		for( TCHAR c : yyyymmddhhmmss ) if( !IsDigitChar(c) ) return false;

		if( !IsValidDate(yyyymmddhhmmss.substr(0, 8)) ) return false;

		int hour = std::stoi(yyyymmddhhmmss.substr(8, 2));
		int minute = std::stoi(yyyymmddhhmmss.substr(10, 2));
		int sec = std::stoi(yyyymmddhhmmss.substr(12, 2));

		return hour <= 23 && minute <= 59 && sec <= 59;
	}

	//***************************************************************************
	// @brief YYYYMMDD(8자리) 문자열을 연/월/일 정수로 분리합니다.
	// @param s 파싱할 문자열
	// @param year 결과 연도가 저장될 참조
	// @param month 결과 월이 저장될 참조
	// @param day 결과 일이 저장될 참조
	// @return 성공 시 true, 형식이 잘못되면 false
	//***************************************************************************
	bool ParseYYYYMMDD(const _tstring& s, int& year, int& month, int& day)
	{
		if( !IsValidDate(s) ) return false;

		year = std::stoi(s.substr(0, 4));
		month = std::stoi(s.substr(4, 2));
		day = std::stoi(s.substr(6, 2));
		return true;
	}

	//***************************************************************************
	// @brief YYYYMMDDHHMMSS(14자리) 문자열을 std::tm으로 파싱합니다.
	// @param s 파싱할 문자열
	// @param out 결과가 채워질 std::tm 참조
	// @return 성공 시 true, 형식이 잘못되면 false
	//***************************************************************************
	bool ParseYYYYMMDDHHMMSS(const _tstring& s, std::tm& out)
	{
		if( !IsValidDateTime(s) ) return false;

		out = std::tm{};
		out.tm_year = std::stoi(s.substr(0, 4)) - 1900;
		out.tm_mon = std::stoi(s.substr(4, 2)) - 1;
		out.tm_mday = std::stoi(s.substr(6, 2));
		out.tm_hour = std::stoi(s.substr(8, 2));
		out.tm_min = std::stoi(s.substr(10, 2));
		out.tm_sec = std::stoi(s.substr(12, 2));
		out.tm_isdst = -1;
		return true;
	}

	//***************************************************************************
	// @brief YYYYMMDDHHMMSS(14자리) 문자열을 time_t로 변환합니다.
	// @param yyyymmddhhmmss 변환할 문자열
	// @param out 결과 time_t가 저장될 참조
	// @return 성공 시 true, 형식이 잘못되면 false
	//***************************************************************************
	bool ToTimeT(const _tstring& yyyymmddhhmmss, time_t& out)
	{
		std::tm tmVal{};
		if( !ParseYYYYMMDDHHMMSS(yyyymmddhhmmss, tmVal) ) return false;

		out = std::mktime(&tmVal);
		return out != static_cast<time_t>(-1);
	}

	//***************************************************************************
	// @brief 지정한 날짜(YYYYMMDD 또는 YYYYMMDDHHMMSS) 문자열을 오늘과 비교합니다.
	// @param yyyymmddOrYyyymmddhhmmss 비교할 문자열 (8자리 또는 14자리)
	// @return 음수: 오늘보다 이전, 0: 오늘과 같음, 양수: 오늘보다 이후 (형식 오류 시 -9999)
	//***************************************************************************
	int CompareToToday(const _tstring& yyyymmddOrYyyymmddhhmmss)
	{
		const _tstring& s = yyyymmddOrYyyymmddhhmmss;
		if( s.size() != 8 && s.size() != 14 ) return -9999;
		for( TCHAR c : s ) if( !IsDigitChar(c) ) return -9999;

		_tstring today = (s.size() == 8) ? ToYYYYMMDD() : ToYYYYMMDDHHMMSS();
		return s.compare(today);
	}

	//***************************************************************************
	// 초 단위 시간 <-> 시/분/초
	//***************************************************************************

	//***************************************************************************
	// @brief 총 초를 "HHMMSS" 형식 문자열로 변환합니다.
	// @param totalSeconds 변환할 총 초 값 (음수 불가)
	// @return "HHMMSS" 형식 문자열, 음수 입력 시 빈 문자열
	//***************************************************************************
	_tstring SecondsToHHMMSS(long totalSeconds)
	{
		int hour = 0, minute = 0, sec = 0;
		if( !SecondsToHms(totalSeconds, hour, minute, sec) ) return {};

		_tstringstream oss;
		oss << std::setw(2) << std::setfill(_T('0')) << hour
			<< std::setw(2) << std::setfill(_T('0')) << minute
			<< std::setw(2) << std::setfill(_T('0')) << sec;
		return oss.str();
	}

	//***************************************************************************
	// @brief 총 초를 시/분/초로 분할합니다.
	// @param totalSeconds 변환할 총 초 값 (음수 불가)
	// @param hour 시간이 저장될 참조
	// @param minute 분이 저장될 참조
	// @param sec 초가 저장될 참조
	// @return 성공 시 true, 음수 입력 시 false
	//***************************************************************************
	bool SecondsToHms(long totalSeconds, int& hour, int& minute, int& sec)
	{
		hour = minute = sec = 0;
		if( totalSeconds < 0 ) return false;

		hour = static_cast<int>(totalSeconds / 3600);
		minute = static_cast<int>((totalSeconds % 3600) / 60);
		sec = static_cast<int>(totalSeconds % 60);
		return true;
	}

	//***************************************************************************
	// GMT 문자열 / 로컬<->GMT 재해석
	//***************************************************************************

	//***************************************************************************
	// @brief time_t를 RFC 822 스타일 GMT 문자열로 변환합니다.
	// @detail 로케일에 영향받지 않도록 요일/월 이름을 직접 매핑합니다.
	// @param t 변환할 time_t 값
	// @return 예) "Wed, 19 Aug 2026 10:23:00 GMT", 변환 실패 시 빈 문자열
	//***************************************************************************
	_tstring ToGmtString(const time_t& t)
	{
		static const TCHAR* wdayNames[] = { _T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat") };
		static const TCHAR* monNames[] =
		{
			_T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"), _T("May"), _T("Jun"), _T("Jul"), _T("Aug"), _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec")
		};

		std::tm tmVal{};
		if( !GmTimeSafe(tmVal, t) ) return {};

		_tstringstream oss;
		oss << wdayNames[tmVal.tm_wday] << _T(", ")
			<< std::setw(2) << std::setfill(_T('0')) << tmVal.tm_mday << _T(' ')
			<< monNames[tmVal.tm_mon] << _T(' ')
			<< (tmVal.tm_year + 1900) << _T(' ')
			<< std::setw(2) << std::setfill(_T('0')) << tmVal.tm_hour << _T(':')
			<< std::setw(2) << std::setfill(_T('0')) << tmVal.tm_min << _T(':')
			<< std::setw(2) << std::setfill(_T('0')) << tmVal.tm_sec << _T(" GMT");
		return oss.str();
	}

	//***************************************************************************
	// @brief std::tm(UTC 필드 기준)을 time_t로 변환합니다. (Windows: _mkgmtime,
	//        POSIX: timegm)
	// @param tmVal UTC 기준 필드가 채워진 std::tm 참조
	// @return 변환된 time_t 값
	//***************************************************************************
	static time_t FromUtcTm(std::tm& tmVal)
	{
#ifdef _WIN32
		return _mkgmtime(&tmVal);
#else
		return timegm(&tmVal);
#endif
	}

	//***************************************************************************
	// @brief t의 로컬 벽시계 값(년/월/일 시:분:초)을 그대로 GMT로 재해석한
	//        time_t를 반환합니다. 실제 시간대 변환이 아니라 같은 숫자를
	//        다른 시간대로 재해석합니다.
	// @param t 기준 time_t 값
	// @return 재해석된 time_t 값, 변환 실패 시 t 그대로 반환
	//***************************************************************************
	time_t ShiftLocalToGmt(const time_t& t)
	{
		std::tm tmVal{};
		if( !LocalTimeSafe(tmVal, t) ) return t;

		return FromUtcTm(tmVal);
	}

	//***************************************************************************
	// @brief t의 GMT 벽시계 값(년/월/일 시:분:초)을 그대로 로컬 시각으로
	//        재해석한 time_t를 반환합니다. 실제 시간대 변환이 아니라 같은
	//        숫자를 다른 시간대로 재해석합니다.
	// @param t 기준 time_t 값
	// @return 재해석된 time_t 값, 변환 실패 시 t 그대로 반환
	//***************************************************************************
	time_t ShiftGmtToLocal(const time_t& t)
	{
		std::tm tmVal{};
		if( !GmTimeSafe(tmVal, t) ) return t;

		tmVal.tm_isdst = -1;
		time_t result = std::mktime(&tmVal);
		return (result == static_cast<time_t>(-1)) ? t : result;
	}
}