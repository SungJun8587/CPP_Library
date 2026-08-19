
//***************************************************************************
// DateTimeUtil.h : interface for the DateTimeUtil Functions.
//
// #ifdef _WIN32 블록 : SYSTEMTIME/TIMESTAMP_STRUCT/SQL_TIMESTAMP_STRUCT 등
//                      Windows/ODBC 전용 타입에 묶인 함수들.
// ptime 네임스페이스 : std::chrono/time_t/_tstring 기반의 플랫폼 독립
//                      함수들. _tstring/_tstringstream/TCHAR는
//                      BaseRedefineDataType.h(프로젝트 공통 헤더)에서
//                      UNICODE 여부에 따라 정의됩니다. 이 파일이 그 헤더
//                      없이 단독으로 컴파일되는 경우를 위한 폴백만 아래에
//                      둡니다. 함수별 설명(@brief/@param/@return)은
//                      DateTimeUtil.cpp 에 있습니다.
//***************************************************************************

#ifndef __DATETIMEUTIL_H__
#define __DATETIMEUTIL_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include "BaseRedefineDataType.h"
#endif

#include <chrono>
#include <ctime>
#include <cstdint>
#include <string>
#include <sstream>

//***************************************************************************
// Windows 전용 (SYSTEMTIME / TIMESTAMP_STRUCT / SQL_TIMESTAMP_STRUCT)
//***************************************************************************
#ifdef _WIN32

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

bool  operator==(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
bool  operator>(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
bool  operator>=(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
bool  operator<(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
bool  operator<=(SYSTEMTIME& stime1, SYSTEMTIME& stime2);
uint64 operator-(const SYSTEMTIME& stime1, const SYSTEMTIME& stime2);

SYSTEMTIME& operator<<(SYSTEMTIME& stime, TCHAR* tszDateTime);
TCHAR* operator<<(TCHAR* tszDateTime, SYSTEMTIME& stime);

time_t	GetTimestampToDateTime(const SYSTEMTIME tTime);
bool	GetSystemTimeToTimestamp(SYSTEMTIME& tTime, const time_t timestamp);

bool	IsValidDateTime(const TCHAR* ptszDateTime);

bool	GetSqlTime(SQL_TIMESTAMP_STRUCT& stDateTime, const TCHAR* ptszDateTime);
bool	GetSystemTime(SYSTEMTIME& tTime, const TCHAR* ptszDateTime);

void	GetGMTTime(TCHAR* ptszStdDateTime, SYSTEMTIME sTime);
_tstring FileTimeToLocalString(const FILETIME& ft);

#endif

//***************************************************************************
// 크로스플랫폼 (std::chrono / time_t / _tstring 기반)
//***************************************************************************
namespace ptime
{
	using Clock = std::chrono::system_clock;
	using SteadyClock = std::chrono::steady_clock;

	// TCHAR/_tstring/_tstringstream : BaseRedefineDataType.h(프로젝트 공통 헤더)
	// 에서 UNICODE 여부에 따라 이미 정의됩니다. (UNICODE: wchar_t/std::wstring,
	// 그 외: char/std::string) 이 파일은 그 정의를 그대로 사용합니다.

	int64_t NowMillis();
	int64_t NowMicros();
	double MonotonicNowSec();

	bool LocalTimeSafe(std::tm& out, const time_t& t);
	bool GmTimeSafe(std::tm& out, const time_t& t);

	_tstring ToIso8601(const time_t& t);
	_tstring Format(const time_t& t, const TCHAR* fmt);
	_tstring FormatNow(const TCHAR* fmt);

	_tstring ToYYYY(const time_t& t);
	_tstring ToYYYYMM(const time_t& t);
	_tstring ToYYYYMMDD(const time_t& t);
	_tstring ToMMDD(const time_t& t);
	_tstring ToHHMMSS(const time_t& t);
	_tstring ToHHMM(const time_t& t);
	_tstring ToYYYYMMDDHHMMSS(const time_t& t);
	_tstring ToYYYY_MM_DD(const time_t& t);

	_tstring ToYYYY();
	_tstring ToYYYYMM();
	_tstring ToYYYYMMDD();
	_tstring ToMMDD();
	_tstring ToHHMMSS();
	_tstring ToHHMM();
	_tstring ToYYYYMMDDHHMMSS();
	_tstring ToYYYY_MM_DD();

	void SleepMillis(int64_t ms);

	//***************************************************************************
	// @brief 고정밀 시간 측정용 스톱워치 클래스
	// @details std::chrono::steady_clock을 기반으로 경과 시간을 측정하며, 시스템 시간 변경의 영향을 받지 않습니다.
	//***************************************************************************
	class StopWatch
	{
	public:
		using Clock = std::chrono::steady_clock;

		//***************************************************************************
		// @brief StopWatch 객체를 생성하고 측정 시작 시간을 기록합니다.
		// @details 객체가 생성되는 시점의 steady_clock 타임스탬프를 m_start에 저장합니다.
		//***************************************************************************
		StopWatch() : m_start(Clock::now()) {}

		//***************************************************************************
		// @brief 스톱워치 측정 시간을 초기화합니다.
		// @details 현재 시점을 새로운 시작 시간으로 재설정합니다.
		//***************************************************************************
		void Reset()
		{
			m_start = Clock::now();
		}

		//***************************************************************************
		// @brief 경과 시간을 밀리초(millisecond) 단위로 반환합니다.
		// @details 시작 시간부터 현재 시점까지의 경과 시간을 계산하여 정수형으로 반환합니다.
		// @return int64_t 경과된 밀리초 (ms)
		//***************************************************************************
		int64_t ElapsedMillis() const
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				Clock::now() - m_start).count();
		}

		//***************************************************************************
		// @brief 경과 시간을 초(second) 단위로 반환합니다.
		// @details 시작 시간부터 현재 시점까지의 경과 시간을 소수점 단위의 실수로 반환합니다.
		// @return double 경과된 초 (sec)
		//***************************************************************************
		double ElapsedSec() const
		{
			return std::chrono::duration<double>(Clock::now() - m_start).count();
		}

	private:
		Clock::time_point m_start;	// 측정 시작 시간
	};

	//***************************************************************************
	// 양력(그레고리력) 계산
	//***************************************************************************

	bool IsLeapYear(int year);
	int LastDayOfMonth(int year, int month);
	int LastDayOfMonth(const time_t& t);

	int DayOfWeekIndex(int year, int month, int day);
	int DayOfWeekIndex(const time_t& t);

	_tstring WeekdayNameKo(int dayOfWeekIndex);
	_tstring WeekdayNameKo(const time_t& t);
	_tstring WeekdayNameEn(int dayOfWeekIndex);
	_tstring WeekdayNameEn(const time_t& t);

	_tstring MonthNameEn(int month);
	_tstring MonthNameKo(int month);

	//***************************************************************************
	// 기준 시각 연산
	//***************************************************************************

	time_t Now();
	time_t AddSeconds(const time_t& t, int64_t seconds);
	time_t AddDays(const time_t& t, int days);

	long SecondsBetween(const time_t& t1, const time_t& t2);
	long MinutesBetween(const time_t& t1, const time_t& t2);

	//***************************************************************************
	// 문자열 파싱/검증
	//***************************************************************************

	bool IsValidDate(const _tstring& yyyymmdd);
	bool IsValidDateTime(const _tstring& yyyymmddhhmmss);

	bool ParseYYYYMMDD(const _tstring& s, int& year, int& month, int& day);
	bool ParseYYYYMMDDHHMMSS(const _tstring& s, std::tm& out);
	bool ToTimeT(const _tstring& yyyymmddhhmmss, time_t& out);

	int CompareToToday(const _tstring& yyyymmddOrYyyymmddhhmmss);

	//***************************************************************************
	// 초 단위 시간 <-> 시/분/초
	//***************************************************************************

	_tstring SecondsToHHMMSS(long totalSeconds);
	bool SecondsToHms(long totalSeconds, int& hour, int& minute, int& sec);

	//***************************************************************************
	// GMT 문자열 / 로컬 <-> GMT 재해석
	//***************************************************************************

	_tstring ToGmtString(const time_t& t);

	time_t ShiftLocalToGmt(const time_t& t);
	time_t ShiftGmtToLocal(const time_t& t);
}

#endif // ndef __DATETIMEUTIL_H__