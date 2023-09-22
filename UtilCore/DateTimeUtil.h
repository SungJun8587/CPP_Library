
//***************************************************************************
// DateTimeUtil.h : interface for the DateTimeUtil Functions.
//
//***************************************************************************

#ifndef __DATETIMEUTIL_H__
#define __DATETIMEUTIL_H__

#include <time.h>
#include <sqltypes.h>

namespace TIME
{
	const __int64 IN_SEC = 10000000;
	const __int64 IN_MIN = IN_SEC * 60;
	const __int64 IN_HOUR = IN_MIN * 60;
	const __int64 IN_DAY = IN_HOUR * 24;
	const __int64 IN_WEEK = IN_DAY * 7;

	void IncreaseSystemTime(SYSTEMTIME& stime, __int64 nAddTime);
	uint64 DifMinute(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
}

time_t& operator<<(time_t& t, const TIMESTAMP_STRUCT& ts);
TIMESTAMP_STRUCT& operator<<(TIMESTAMP_STRUCT& ts, const time_t& t);

time_t& operator<<(time_t& t, const SYSTEMTIME& stime);
SYSTEMTIME& operator<<(SYSTEMTIME& stime, const time_t& t);

SYSTEMTIME& operator<<(SYSTEMTIME& stime, const TIMESTAMP_STRUCT& ts);
TIMESTAMP_STRUCT& operator<<(TIMESTAMP_STRUCT& ts, const SYSTEMTIME& stime);

BOOL operator==(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
BOOL operator>(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
BOOL operator>=(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
BOOL operator<(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
BOOL operator<=(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
uint64 operator-(const SYSTEMTIME& stime1, const SYSTEMTIME& stime2);

SYSTEMTIME& operator<<(SYSTEMTIME& stime, TCHAR* tszDateTime);
TCHAR* operator<<(TCHAR* tszDateTime, SYSTEMTIME& stime);

time_t	GetCurTimestamp();
void	GetCurDateTime(TCHAR* ptszDateTime);
void	GetCurDate(TCHAR* ptszDate);
void	GetYesterdayTime(TCHAR* ptszDateTime);
void    GetYesterday(TCHAR* ptszDate);

time_t	GetTimestampToDateTime(const SYSTEMTIME tTime);
time_t	GetTimestampToDateTime(const TCHAR* ptszDateTime);
BOOL	GetSystemTimeToTimestamp(SYSTEMTIME& tTime, const time_t timestamp);
BOOL	GetDateTimeToTimestamp(TCHAR* ptszDateTime, const time_t timestamp);

BOOL	IsValidDateTime(const TCHAR* ptszDateTime);
BOOL	IsValidDate(const TCHAR* ptszDate);
BOOL	IsValidTime(const TCHAR* ptszTime);

int		CompareToday(const TCHAR* ptszDate);

BOOL	GetDateIntervalDate(TCHAR* ptszDestDate, const TCHAR* ptszSrcDate, const int nInterval);
BOOL	GetDateIntervalToday(TCHAR* ptszDate, const int nInterval);
BOOL	GetIntervalSec(const TCHAR* ptszTime1, const TCHAR* ptszTime2, long& lSecInterval);
BOOL	GetSqlTime(SQL_TIMESTAMP_STRUCT& stDateTime, const TCHAR* ptszDateTime);
BOOL	GetSystemTime(SYSTEMTIME& tTime, const TCHAR* ptszDateTime);

BOOL	GetMonthName(TCHAR* ptszMonthName, const int nMonth);
BOOL	GetDayOfWeek(TCHAR* ptszDayOfWeek, const int nYear, const int nMonth, const int nDay);
int		DayOfWeek(const int nYear, const int nMonth, const int nDay);

int		CountOfFeb29(int nYear);
BOOL	IsLeapYear(int nYear);
BOOL	GetTimeOfSecond(TCHAR* ptszTime, const long lSecond);
BOOL	GetTimeOfSecond(int& nHour, int& nMinute, int& nSec, const long lSecond);

void	GetGMTTime(TCHAR* ptszStdDateTime, SYSTEMTIME sTime);
void	ConvertLocaltimeToGMT(time_t& dest, const time_t& src);
void	ConvertGMTToLocaltime(time_t& dest, const time_t& src);

#endif // ndef __DATETIMEUTIL_H__
