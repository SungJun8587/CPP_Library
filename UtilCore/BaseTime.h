
//***************************************************************************
// BaseTime.h: interface for the CBaseTime class.
//
//***************************************************************************

#ifndef __BASETIME_H__
#define __BASETIME_H__

#include <time.h>
#include <sqltypes.h>

//***************************************************************************
//
class CBaseTime
{
public:
	CBaseTime() { }
	~CBaseTime() { }

	BOOL	IsValidDateTime(const TCHAR* ptszDateTime);
	BOOL	IsValidDate(const TCHAR* ptszDate);
	BOOL	IsValidTime(const TCHAR* ptszTime);

	int		CompareToday(const TCHAR* ptszDate);
	void	GetCurTime(TCHAR* ptszDateTime);
	void	GetCurDate(TCHAR* ptszDate);
	void	GetYesterdayTime(TCHAR* ptszDateTime);
	void    GetYesterday(TCHAR* ptszDate);

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
};

#endif // ndef __BASETIME_H__
