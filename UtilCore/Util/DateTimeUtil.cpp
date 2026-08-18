
//***************************************************************************
// DateTimeUtil.cpp : implementation of the DateTimeUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "DateTimeUtil.h"

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
// @brief 현재 시스템의 타임스탬프 값을 반환합니다.
// @return 현재 time_t 타임스탬프
//***************************************************************************
time_t GetCurTimestamp()
{
	time_t now = time(nullptr);
	return now;
}

//***************************************************************************
// @brief 현재 지역 날짜 및 시간(YYYYMMDDhhmmss, 14자리)을 가져옵니다.
// @param ptszDateTime 결과를 저장할 TCHAR 버퍼
//***************************************************************************
void GetCurDateTime(TCHAR* ptszDateTime)
{
	if( ptszDateTime == nullptr ) return;

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	_stprintf_s(ptszDateTime, STD_DATETIME_STRLEN, _T("%04d%02d%02d%02d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond);
}

//***************************************************************************
// @brief 현재 지역 날짜(YYYYMMDD, 8자리)를 가져옵니다.
// @param ptszDate 결과를 저장할 TCHAR 버퍼
//***************************************************************************
void GetCurDate(TCHAR* ptszDate)
{
	if( ptszDate == nullptr ) return;

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	_stprintf_s(ptszDate, DATE_STRLEN, _T("%04d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay);
}

//***************************************************************************
// @brief 어제의 날짜 및 시간(YYYYMMDDhhmmss)을 가져옵니다.
// @param ptszDateTime 결과를 저장할 TCHAR 버퍼
//***************************************************************************
void GetYesterdayTime(TCHAR* ptszDateTime)
{
	if( ptszDateTime == nullptr ) return;

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if( stime.wYear % 4 == 0 && stime.wYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( stime.wYear % 4 == 0 && stime.wYear % 100 == 0 && stime.wYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( stime.wDay - 1 <= 0 )
	{
		if( stime.wMonth - 1 <= 0 )
		{
			stime.wYear--;
			stime.wMonth = 12;
			stime.wDay = 31;
		}
		else
		{
			stime.wMonth--;
			stime.wDay = nMonthList[stime.wMonth];
		}
	}
	else
		stime.wDay--;

	_stprintf_s(ptszDateTime, STD_DATETIME_STRLEN, _T("%04d%02d%02d%02d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond);
}

//***************************************************************************
// @brief 어제의 날짜(YYYYMMDD)를 가져옵니다.
// @param ptszDate 결과를 저장할 TCHAR 버퍼
//***************************************************************************
void GetYesterday(TCHAR* ptszDate)
{
	if( ptszDate == nullptr ) return;

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if( stime.wYear % 4 == 0 && stime.wYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( stime.wYear % 4 == 0 && stime.wYear % 100 == 0 && stime.wYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( stime.wDay - 1 <= 0 )
	{
		if( stime.wMonth - 1 <= 0 )
		{
			stime.wYear--;
			stime.wMonth = 12;
			stime.wDay = 31;
		}
		else
		{
			stime.wMonth--;
			stime.wDay = nMonthList[stime.wMonth];
		}
	}
	else
		stime.wDay--;

	_stprintf_s(ptszDate, DATE_STRLEN, _T("%04d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay);
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
// @brief YYYYMMDDhhmmss 형식의 문자열을 time_t 타임스탬프로 변환합니다.
// @param ptszDateTime 변환할 날짜/시간 문자열
// @return 변환된 time_t 값
//***************************************************************************
time_t GetTimestampToDateTime(const TCHAR* ptszDateTime)
{
	struct tm time;

	time.tm_sec = (ptszDateTime[12] - '0') * 10 + (ptszDateTime[13] - '0');					/* seconds after the minute - [0,59] */
	time.tm_min = (ptszDateTime[10] - '0') * 10 + (ptszDateTime[11] - '0');					/* minutes after the hour - [0,59] */
	time.tm_hour = (ptszDateTime[8] - '0') * 10 + (ptszDateTime[9] - '0');					/* hours since midnight - [0,23] */
	time.tm_mday = (ptszDateTime[6] - '0') * 10 + (ptszDateTime[7] - '0');					/* day of the month - [1,31] */
	time.tm_mon = (ptszDateTime[4] - '0') * 10 + (ptszDateTime[5] - '0') - 1;				/* months since January - [0,11] */
	time.tm_year = (ptszDateTime[0] - '0') * 1000 + (ptszDateTime[1] - '0') * 100 + \
		(ptszDateTime[2] - '0') * 10 + (ptszDateTime[3] - '0') - 1900;						/* years since 1900 */
	time.tm_wday = 0;																		/* days since Sunday - [0,6] */
	time.tm_yday = 0;																		/* days since January 1 - [0,365] */
	time.tm_isdst = 0;																		/* daylight savings time flag */

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
// @brief time_t 타임스탬프를 YYYYMMDDhhmmss 형식의 문자열로 변환합니다.
// @param ptszDateTime 결과를 저장할 TCHAR 버퍼
// @param timestamp 변환할 time_t 값
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetDateTimeToTimestamp(TCHAR* ptszDateTime, const time_t timestamp)
{
	struct tm tm_info;

	errno_t err_no = localtime_s(&tm_info, &timestamp);
	if( err_no > 0 ) return false;

	_tcsftime(ptszDateTime, STD_DATETIME_STRLEN, _T("%Y%m%d%H%M%S"), &tm_info);

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
// @brief YYYYMMDD 형식의 문자열이 유효한 날짜인지 검증합니다.
// @param ptszDate 검증할 날짜 문자열
// @return 유효하면 true, 아니면 false
//***************************************************************************
bool  IsValidDate(const TCHAR* ptszDate)
{
	int i = 0;

	for( i = 0; i < 9; i++ )
		if( *(ptszDate + i) == NULL ) break;

	if( i < 8 ) return false;

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	int nYear = (ptszDate[0] - '0') * 1000 + (ptszDate[1] - '0') * 100 + (ptszDate[2] - '0') * 10 + (ptszDate[3] - '0');
	int nMonth = (ptszDate[4] - '0') * 10 + (ptszDate[5] - '0');
	int nDay = (ptszDate[6] - '0') * 10 + (ptszDate[7] - '0');

	if( nYear < 1 || nMonth < 1 || nMonth > 12 || nDay < 1 ) return false;

	if( nYear % 4 == 0 && nYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( nYear % 4 == 0 && nYear % 100 == 0 && nYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( nDay > nMonthList[nMonth] ) return false;

	return true;
}

//***************************************************************************
// @brief hhmmss 형식의 문자열이 유효한 시간인지 검증합니다.
// @param ptszTime 검증할 시간 문자열
// @return 유효하면 true, 아니면 false
//***************************************************************************
bool  IsValidTime(const TCHAR* ptszTime)
{
	int i = 0;

	for( i = 0; i < 7; i++ )
		if( *(ptszTime + i) == NULL ) break;

	if( i < 6 ) return false;

	int nHour = (ptszTime[0] - '0') * 10 + (ptszTime[1] - '0');
	int nMin = (ptszTime[2] - '0') * 10 + (ptszTime[3] - '0');
	int nSec = (ptszTime[4] - '0') * 10 + (ptszTime[5] - '0');

	if( nHour < 0 || nHour > 23 || nMin < 0 || nMin > 59 || nSec < 0 || nSec > 59 ) return false;

	return true;
}

//***************************************************************************
// @brief 지정한 날짜/시간과 오늘을 비교합니다.
// @param ptszDate 비교할 날짜/시간 문자열 (YYYYMMDD 또는 YYYYMMDDhhmmss)
// @return 음수: 오늘보다 이전, 0: 오늘과 같음, 양수: 오늘보다 이후 (오류 시 -9999)
//***************************************************************************
int CompareToday(const TCHAR* ptszDate)
{
	if( ptszDate == nullptr ) return -9999;

	size_t nRet = _tcslen(ptszDate);
	if( nRet != 8 && nRet != 14 )
		return -9999;

	for( TCHAR* lpsz = (TCHAR*)ptszDate; *lpsz != NULL; ++lpsz )
		if( !isdigit(*lpsz) ) return -9999;

	TCHAR		tszDateTime[STD_DATETIME_STRLEN];

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	if( nRet == 8 ) _stprintf_s(tszDateTime, STD_DATETIME_STRLEN, _T("%04d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay);
	else _stprintf_s(tszDateTime, STD_DATETIME_STRLEN, _T("%04d%02d%02d%02d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond);

	return _tcscmp(ptszDate, tszDateTime);
}

//***************************************************************************
// @brief 기준 날짜에서 지정한 간격(일 단위)만큼 더하거나 뺀 날짜를 계산합니다.
// @param ptszDestDate 결과 날짜(YYYYMMDD)가 저장될 버퍼
// @param ptszSrcDate 기준 날짜(YYYYMMDD) 문자열
// @param nInterval 가감할 일수 간격 (양수: 과거, 음수: 미래)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetDateIntervalDate(TCHAR* ptszDestDate, const TCHAR* ptszSrcDate, const int nInterval)
{
	int nYear(0), nMonth(0), nDay(0);

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	nYear = (ptszSrcDate[0] - '0') * 1000 + (ptszSrcDate[1] - '0') * 100 +
		(ptszSrcDate[2] - '0') * 10 + (ptszSrcDate[3] - '0');
	nMonth = (ptszSrcDate[4] - '0') * 10 + (ptszSrcDate[5] - '0');
	nDay = (ptszSrcDate[6] - '0') * 10 + (ptszSrcDate[7] - '0');

	stime.wYear = nYear;
	stime.wMonth = nMonth;
	stime.wDay = nDay;

	if( stime.wYear % 4 == 0 && stime.wYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( stime.wYear % 4 == 0 && stime.wYear % 100 == 0 && stime.wYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( nInterval > 0 )
	{
		if( (stime.wDay - nInterval) < 0 )
		{
			if( (stime.wMonth - 1) < 0 )
			{
				stime.wYear--;
				stime.wMonth = 12;
				int nTemp = nInterval - stime.wDay;

				if( nTemp > 31 )
				{
					ptszDestDate[0] = _T('\0');
					return false;
				}

				stime.wDay = 31 - nTemp;
			}
			else
			{
				stime.wMonth--;
				int nTemp = nInterval - stime.wDay;

				if( nTemp > nMonthList[stime.wMonth] )
				{
					ptszDestDate[0] = _T('\0');
					return false;
				}

				stime.wDay = nMonthList[stime.wMonth] - nTemp;
			}
		}
		else if( (stime.wDay - nInterval) == 0 )
		{
			if( stime.wMonth - 1 < 0 )
			{
				stime.wYear--;
				stime.wMonth = 12;
				stime.wDay = 31;
			}
			else
			{
				stime.wMonth--;
				stime.wDay = nMonthList[stime.wMonth];
			}
		}
		else
			stime.wDay = stime.wDay - nInterval;
	}
	else
	{
		if( (stime.wDay + (-1 * nInterval)) > nMonthList[stime.wMonth] )
		{
			if( stime.wMonth + 1 > 12 )
			{
				stime.wYear++;
				stime.wMonth = 1;
				int nTemp = (-1 * nInterval) - (nMonthList[12] - stime.wDay);

				if( nTemp > 31 || nTemp < 0 )
				{
					ptszDestDate[0] = _T('\0');
					return false;
				}
				stime.wDay = nTemp;
			}
			else
			{
				int nTemp = (-1 * nInterval) - (nMonthList[stime.wMonth] - stime.wDay);

				stime.wMonth = stime.wMonth + 1;

				if( nTemp > 31 || nTemp < 0 )
				{
					ptszDestDate[0] = _T('\0');
					return false;
				}
				stime.wDay = nTemp;
			}
		}
		else stime.wDay = stime.wDay + (-1 * nInterval);
	}

	_stprintf_s(ptszDestDate, DATE_STRLEN, _T("%04d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay);

	return true;
}

//***************************************************************************
// @brief 오늘 기준에서 지정한 간격(일 단위)만큼 더하거나 뺀 날짜를 계산합니다.
// @param ptszDate 결과 날짜(YYYYMMDD)가 저장될 버퍼
// @param nInterval 가감할 일수 간격 (양수: 과거, 음수: 미래)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetDateIntervalToday(TCHAR* ptszDate, const int nInterval)
{
	if( ptszDate == nullptr ) return false;

	SYSTEMTIME	stime;
	GetLocalTime(&stime);

	int nMonthList[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if( stime.wYear % 4 == 0 && stime.wYear % 100 != 0 ) nMonthList[2] = nMonthList[2] + 1;
	else if( stime.wYear % 4 == 0 && stime.wYear % 100 == 0 && stime.wYear % 400 == 0 ) nMonthList[2] = nMonthList[2] + 1;

	if( nInterval > 0 )
	{
		if( (stime.wDay - nInterval) < 0 )
		{
			if( (stime.wMonth - 1) < 0 )
			{
				stime.wYear--;
				stime.wMonth = 12;
				int nTemp = nInterval - stime.wDay;

				if( nTemp > 31 )
				{
					ptszDate[0] = _T('\0');
					return false;
				}

				stime.wDay = 31 - nTemp;
			}
			else
			{
				stime.wMonth--;
				int nTemp = nInterval - stime.wDay;

				if( nTemp > nMonthList[stime.wMonth] )
				{
					ptszDate[0] = _T('\0');
					return false;
				}

				stime.wDay = nMonthList[stime.wMonth] - nTemp;
			}
		}
		else if( (stime.wDay - nInterval) == 0 )
		{
			if( stime.wMonth - 1 < 0 )
			{
				stime.wYear--;
				stime.wMonth = 12;
				stime.wDay = 31;
			}
			else
			{
				stime.wMonth--;
				stime.wDay = nMonthList[stime.wMonth];
			}
		}
		else
			stime.wDay = stime.wDay - nInterval;
	}
	else
	{
		if( (stime.wDay + (-1 * nInterval)) > nMonthList[stime.wMonth] )
		{
			if( stime.wMonth + 1 > 12 )
			{
				stime.wYear++;
				stime.wMonth = 1;
				int nTemp = (-1 * nInterval) - (nMonthList[12] - stime.wDay);

				if( nTemp > 31 || nTemp < 0 )
				{
					ptszDate[0] = _T('\0');
					return false;
				}
				stime.wDay = nTemp;
			}
			else
			{
				int nTemp = (-1 * nInterval) - (nMonthList[stime.wMonth] - stime.wDay);

				stime.wMonth = stime.wMonth + 1;

				if( nTemp > 31 || nTemp < 0 )
				{
					ptszDate[0] = _T('\0');
					return false;
				}
				stime.wDay = nTemp;
			}
		}
		else stime.wDay = stime.wDay + (-1 * nInterval);
	}
	_stprintf_s(ptszDate, DATE_STRLEN, _T("%04d%02d%02d"), stime.wYear, stime.wMonth, stime.wDay);

	return true;
}

//***************************************************************************
// @brief 두 날짜/시간 문자열 간의 시간 차이를 초(sec) 단위로 계산합니다.
// @param ptszTime1 첫 번째 날짜/시간 문자열 (YYYYMMDDhhmmss)
// @param ptszTime2 두 번째 날짜/시간 문자열 (YYYYMMDDhhmmss)
// @param lSecInterval 계산된 초 간격이 저장될 변수 참조
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetIntervalSec(const TCHAR* ptszTime1, const TCHAR* ptszTime2, long& lSecInterval)
{
	if( _tcslen(ptszTime1) != 14 || _tcslen(ptszTime2) != 14 ) return false;

	struct tm time1;
	struct tm time2;
	time_t tTime1;
	time_t tTime2;

	time1.tm_sec = (ptszTime1[12] - '0') * 10 + (ptszTime1[13] - '0');				/* seconds after the minute - [0,59] */
	time1.tm_min = (ptszTime1[10] - '0') * 10 + (ptszTime1[11] - '0');				/* minutes after the hour - [0,59] */
	time1.tm_hour = (ptszTime1[8] - '0') * 10 + (ptszTime1[9] - '0');					/* hours since midnight - [0,23] */
	time1.tm_mday = (ptszTime1[6] - '0') * 10 + (ptszTime1[7] - '0');					/* day of the month - [1,31] */
	time1.tm_mon = (ptszTime1[4] - '0') * 10 + (ptszTime1[5] - '0') - 1;				/* months since January - [0,11] */
	time1.tm_year = (ptszTime1[0] - '0') * 1000 + (ptszTime1[1] - '0') * 100 + \
		(ptszTime1[2] - '0') * 10 + (ptszTime1[3] - '0') - 1900;		/* years since 1900 */
	time1.tm_wday = 0;																	/* days since Sunday - [0,6] */
	time1.tm_yday = 0;																	/* days since January 1 - [0,365] */
	time1.tm_isdst = 0;																	/* daylight savings time flag */

	time2.tm_sec = (ptszTime2[12] - '0') * 10 + (ptszTime2[13] - '0');				/* seconds after the minute - [0,59] */
	time2.tm_min = (ptszTime2[10] - '0') * 10 + (ptszTime2[11] - '0');				/* minutes after the hour - [0,59] */
	time2.tm_hour = (ptszTime2[8] - '0') * 10 + (ptszTime2[9] - '0');					/* hours since midnight - [0,23] */
	time2.tm_mday = (ptszTime2[6] - '0') * 10 + (ptszTime2[7] - '0');					/* day of the month - [1,31] */
	time2.tm_mon = (ptszTime2[4] - '0') * 10 + (ptszTime2[5] - '0') - 1;				/* months since January - [0,11] */
	time2.tm_year = (ptszTime2[0] - '0') * 1000 + (ptszTime2[1] - '0') * 100 + \
		(ptszTime2[2] - '0') * 10 + (ptszTime2[3] - '0') - 1900;		/* years since 1900 */
	time2.tm_wday = 0;																	/* days since Sunday - [0,6] */
	time2.tm_yday = 0;																	/* days since January 1 - [0,365] */
	time2.tm_isdst = 0;																	/* daylight savings time flag */

	if( (tTime1 = mktime(&time1)) < 0 ) return false;
	if( (tTime2 = mktime(&time2)) < 0 ) return false;

	lSecInterval = (long)difftime(tTime1, tTime2);

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
// @brief 월 번호(1~12)에 해당하는 영문 월 이름을 가져옵니다.
// @param ptszMonthName 결과를 저장할 TCHAR 버퍼
// @param nMonth 월 번호 (1 ~ 12)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetMonthName(TCHAR* ptszMonthName, const int nMonth)
{
	const static TCHAR atszMonthNames[][MONTH_ENAME_STRLEN] =
	{
		_T("January"), _T("February"), _T("March"), _T("April"), _T("May"), _T("June"), _T("July"), _T("August"), _T("September"), _T("October"), _T("November"), _T("December")
	};

	if( nMonth < 1 || nMonth > 12 ) return false;

	_tcscpy_s(ptszMonthName, MONTH_ENAME_STRLEN, atszMonthNames[nMonth - 1]);

	return true;
}

//***************************************************************************
// @brief 특정 연, 월, 일에 해당하는 영문 요일 이름을 가져옵니다.
// @param ptszDayOfWeek 결과를 저장할 TCHAR 버퍼
// @param nYear 연도
// @param nMonth 월
// @param nDay 일
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetDayOfWeek(TCHAR* ptszDayOfWeek, const int nYear, const int nMonth, const int nDay)
{
	int		nDayOfWeek;

	const static TCHAR atszWeekDayNames[][WEEKDAY_ENAME_STRLEN] =
	{
		_T("Sunday"), _T("Monday"), _T("Tuesday"), _T("Wednesday"), _T("Thursay"), _T("Friday"), _T("Saturday")
	};

	nDayOfWeek = DayOfWeek(nYear, nMonth, nDay);
	if( nDayOfWeek < 0 || nDayOfWeek > 6 ) return false;

	_tcscpy_s(ptszDayOfWeek, WEEKDAY_ENAME_STRLEN, atszWeekDayNames[nDayOfWeek]);

	return true;
}

//***************************************************************************
// @brief 특정 연, 월, 일의 요일 인덱스를 계산합니다.
// @param nYear 연도
// @param nMonth 월
// @param nDay 일
// @return 요일 인덱스 (0: 일요일 ~ 6: 토요일, 실패 시 -1)
//***************************************************************************
int	DayOfWeek(const int nYear, const int nMonth, const int nDay)
{
	int		nDayOfWeek;
	const static int pnDaysBeforeMonth[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

	if( nMonth < 0 && nMonth <= 12 ) return -1;
	if( nDay < 0 ) return -1;
	if( nDay > (pnDaysBeforeMonth[nMonth + 1] - pnDaysBeforeMonth[nMonth])
		&& (nMonth != 2 || nDay != 29 || !IsLeapYear(nYear)) ) return -1;

	/* the day of Jan 1, nYear */
	nDayOfWeek = 6 + nYear % 7 + CountOfFeb29(nYear) % 7 + 14;	/* + 14 : makes nDayOfWeek >= 0 */

	/* the day of nMonth 1, nYear */
	nDayOfWeek += pnDaysBeforeMonth[nMonth];

	if( nMonth > 2 && IsLeapYear(nYear) )	nDayOfWeek++;

	/* the day of nMonth nDay, nYear */
	nDayOfWeek += nDay - 1;
	nDayOfWeek %= 7;

	return nDayOfWeek;
}

//***************************************************************************
// @brief 특정 연도까지의 2월 29일(윤일) 누적 횟수를 계산합니다.
// @param nYear 연도
// @return 윤일 누적 횟수
//***************************************************************************
int	CountOfFeb29(int nYear)
{
	int		nCount = 0;
	if( nYear > 0 )
	{
		nCount = 1;		/* Year 0 is a leap year */
		nYear--;		/* Year nYear is not in the period */
	}
	nCount += nYear / 4 - nYear / 100 + nYear / 400;

	return nCount;
}

//***************************************************************************
// @brief 지정한 연도가 윤년인지 여부를 확인합니다.
// @param nYear 연도
// @return 윤년이면 true, 아니면 false
//***************************************************************************
bool  IsLeapYear(int nYear)
{
	if( nYear % 4 != 0 ) return false;
	if( nYear % 100 != 0 )	return true;
	return (nYear % 400 == 0);
}

//***************************************************************************
// @brief 초(sec)를 hhmmss 형식의 문자열로 변환합니다.
// @param ptszTime 결과를 저장할 TCHAR 버퍼
// @param lSecond 변환할 총 초 값
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetTimeOfSecond(TCHAR* ptszTime, const long lSecond)
{
	int	nHour = 0, nMinute = 0, nSec = 0;
	int	nRemindHour = 0;

	if( lSecond < 1 ) return false;

	nHour = lSecond / 3600;
	nRemindHour = lSecond % 3600;
	if( nRemindHour > 0 )
	{
		nMinute = nRemindHour / 60;
		nSec = nRemindHour % 60;
	}
	else
	{
		nMinute = 0;
		nSec = 0;
	}

	_stprintf_s(ptszTime, TIME_STRLEN, _T("%02d%02d%02d"), nHour, nMinute, nSec);

	if( !IsValidTime(ptszTime) ) return false;
	if( nHour > 23 || nMinute > 59 || nSec > 59 ) return false;

	return true;
}

//***************************************************************************
// @brief 초(sec)를 시, 분, 초 단위의 정수로 각각 분할합니다.
// @param nHour 시간이 저장될 변수 참조
// @param nMinute 분이 저장될 변수 참조
// @param nSec 초가 저장될 변수 참조
// @param lSecond 변환할 총 초 값
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool  GetTimeOfSecond(int& nHour, int& nMinute, int& nSec, const long lSecond)
{
	int	nRemindHour = 0;

	nHour = nMinute = nSec = 0;

	if( lSecond < 1 ) return false;

	nHour = lSecond / 3600;
	nRemindHour = lSecond % 3600;
	if( nRemindHour > 0 )
	{
		nMinute = nRemindHour / 60;
		nSec = nRemindHour % 60;
	}
	else
	{
		nMinute = 0;
		nSec = 0;
	}

	if( nHour > 23 || nMinute > 59 || nSec > 59 ) return false;

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
// @brief 로컬 타임스탬프를 GMT 타임스탬프로 변환합니다.
// @param dest 변환된 GMT time_t가 저장될 참조
// @param src 변환할 로컬 time_t 상수 참조
//***************************************************************************
void ConvertLocaltimeToGMT(time_t& dest, const time_t& src)
{
	struct tm tm_src;
	struct tm tm_dest;

	memset(&tm_src, 0, sizeof(struct tm));
	memset(&tm_dest, 0, sizeof(struct tm));

	localtime_s(&tm_src, &src);

	time_t temp = mktime(&tm_src);
	gmtime_s(&tm_dest, &temp);

	dest = mktime(&tm_dest);
}

//***************************************************************************
// @brief GMT 타임스탬프를 로컬 타임스탬프로 변환합니다.
// @param dest 변환된 로컬 time_t가 저장될 참조
// @param src 변환할 GMT time_t 상수 참조
//***************************************************************************
void ConvertGMTToLocaltime(time_t& dest, const time_t& src)
{
	struct tm tm_src;
	struct tm tm_dest;

	memset(&tm_src, 0, sizeof(struct tm));
	memset(&tm_dest, 0, sizeof(struct tm));

	gmtime_s(&tm_src, &src);

	time_t temp = mktime(&tm_src);
	localtime_s(&tm_dest, &temp);

	dest = mktime(&tm_dest);
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