
//***************************************************************************
// CalculatedElapsedTime.h : interface and implementation for the CCalculatedElapsedTime class.
//
//***************************************************************************

#ifndef __CALCULATEDELAPSEDTIME_H__
#define __CALCULATEDELAPSEDTIME_H__

#pragma once

#include <string>

#if(_MSC_VER >= 1900 )
#include <chrono>
#else
#include <windows.h>
#endif

// Visual Studio 2015 이상에서는 std::chrono를 사용해서 구현합니다.
// Visual Studio 2015 미만에서는 WIN API 함수인 QueryPerformanceCounter함수를 사용해서 구현합니다.

#if(_MSC_VER >= 1900 )
//***************************************************************************
// @class CCalculatedElapsedTime
// @brief C++11 std::chrono를 활용한 경과 시간 측정 및 출력 클래스.
//
// @details
// Visual Studio 2015 이상 환경에서 동작하며, 생성 시점 또는 SetStartTime 호출 시점부터
// 특정 지점까지의 경과 시간을 초, 밀리초, 마이크로초, 나노초 단위로 측정하고 출력합니다.
//***************************************************************************
class CCalculatedElapsedTime
{
public:
	//***************************************************************************
	// @brief 기본 생성자. 현재 시점을 시작 시간으로 설정합니다.
	//***************************************************************************
	CCalculatedElapsedTime() : m_start(Now())
	{
	}

	//***************************************************************************
	// @brief 이름을 지정하는 생성자. 현재 시점을 시작 시간으로 설정합니다.
	// @param name 측정 식별자 이름
	//***************************************************************************
	CCalculatedElapsedTime(const std::string& name) : m_start(Now()), m_name(name)
	{
	}

	//***************************************************************************
	// @brief 소멸자
	//***************************************************************************
	virtual ~CCalculatedElapsedTime()
	{
	}

	CCalculatedElapsedTime(const CCalculatedElapsedTime& rhs) = delete;
	CCalculatedElapsedTime& operator=(const CCalculatedElapsedTime& rhs) = delete;

	using sc_clock = std::chrono::system_clock;

	//***************************************************************************
	// @brief 경과 시간을 초(Sec) 단위로 출력합니다.
	//***************************************************************************
	void SecPrint()
	{
		GetElapsedTime<std::chrono::seconds>("seconds");
	}

	//***************************************************************************
	// @brief 경과 시간을 밀리초(milliSec) 단위로 출력합니다.
	//***************************************************************************
	void milliSecPrint()
	{
		GetElapsedTime<std::chrono::milliseconds>("millie seconds");
	}

	//***************************************************************************
	// @brief 경과 시간을 마이크로초(microSec) 단위로 출력합니다.
	//***************************************************************************
	void microSecPrint()
	{
		GetElapsedTime<std::chrono::microseconds>("micro seconds");
	}

	//***************************************************************************
	// @brief 경과 시간을 나노초(nanoSec) 단위로 출력합니다.
	//***************************************************************************
	void nanoSecPrint()
	{
		GetElapsedTime<std::chrono::nanoseconds>("nano seconds");
	}

	//***************************************************************************
	// @brief 측정 시작 시간을 현재 시점으로 초기화합니다.
	//***************************************************************************
	void SetStartTime()
	{
		m_start = Now();
	}

protected:
	//***************************************************************************
	// @brief 지정된 시간 단위 타입으로 경과 시간을 계산하여 콘솔에 출력합니다.
	// @tparam T chrono 시간 단위 타입 (seconds, milliseconds 등)
	// @param timeType 시간 단위를 표현할 문자열 설명
	//***************************************************************************
	template<typename T>
	void GetElapsedTime(std::string timeType)
	{
		T value = std::chrono::duration_cast<T>(Now() - m_start);
		if( m_name != "" )
			std::cout << "[" << m_name.c_str() << "]" << std::endl;
		std::cout << "ElapsedTime : " << value.count() << " " << timeType.c_str() << std::endl;
	}

	//***************************************************************************
	// @brief 현재 시스템의 시각을 반환합니다.
	// @return system_clock의 time_point 객체
	//***************************************************************************
	std::chrono::time_point<sc_clock> Now()
	{
		return sc_clock::now();
	}
private:
	sc_clock::time_point m_start;   // 측정 시작 시점
	std::string          m_name;    // 측정기 식별 이름
};
#else
//***************************************************************************
// @class CCalculatedElapsedTime
// @brief Win32 QPC(QueryPerformanceCounter)를 활용한 경과 시간 측정 및 출력 클래스.
//
// @details
// Visual Studio 2015 미만 환경에서 동작하며, 고성능 타이머 API를 사용하여
// 정밀한 경과 시간을 측정하고 출력합니다.
//***************************************************************************
class CCalculatedElapsedTime
{
public:

	typedef __int64 TimeCheck;

	//***************************************************************************
	// @brief 기본 생성자. 현재 카운터 값과 주파수를 초기화합니다.
	//***************************************************************************
	CCalculatedElapsedTime() : m_start(Now()), m_freq(GetFreq())
	{
	}

	//***************************************************************************
	// @brief 이름을 지정하는 생성자. 현재 카운터 값과 주파수를 초기화합니다.
	// @param name 측정 식별자 이름
	//***************************************************************************
	CCalculatedElapsedTime(const std::string& name) : m_start(Now()), m_freq(GetFreq()), m_name(name)
	{
	}

	//***************************************************************************
	// @brief 소멸자
	//***************************************************************************
	virtual ~CCalculatedElapsedTime()
	{
	}

	//***************************************************************************
	// @brief 경과 시간을 초(Sec) 단위로 출력합니다.
	//***************************************************************************
	void SecPrint()
	{
		GetElapsedTime("seconds");
	}

	//***************************************************************************
	// @brief 경과 시간을 밀리초(milliSec) 단위로 출력합니다.
	//***************************************************************************
	void milliSecPrint()
	{
		GetElapsedTime("millie seconds", 1000);
	}

	//***************************************************************************
	// @brief 경과 시간을 마이크로초(microSec) 단위로 출력합니다.
	//***************************************************************************
	void microSecPrint()
	{
		GetElapsedTime("micro seconds", 1000000);
	}

	//***************************************************************************
	// @brief 경과 시간을 나노초(nanoSec) 단위로 출력합니다.
	//***************************************************************************
	void nanoSecPrint()
	{
		GetElapsedTime("nano seconds", 1000000000);
	}

	//***************************************************************************
	// @brief 측정 시작 시간을 현재 카운터 값으로 초기화합니다.
	//***************************************************************************
	void SetStartTime()
	{
		m_start = Now();
	}

protected:
	//***************************************************************************
	// @brief 고성능 카운터를 기반으로 경과 시간을 계산하여 출력합니다.
	// @param timeType 시간 단위를 표현할 문자열 설명
	// @param N 배율 인수 (밀리초, 마이크로초 등 단위 변환용, 기본값: 0)
	// @return true: 정상 출력 성공, false: 지원하지 않는 단위 범위
	//***************************************************************************
	bool GetElapsedTime(std::string timeType, int N = 0)
	{
		TimeCheck freq = m_freq;

		if( N > freq ) {
			std::cout << "Not Supported " << timeType.c_str() << std::endl;
			return false;
		}

		if( N > 1 )
		{
			freq /= N;
		}
		TimeCheck value = (Now() - m_start) / freq;
		if( m_name != "" )
			std::cout << "[" << m_name.c_str() << "]" << std::endl;
		std::cout << "ElapsedTime : " << value << " " << timeType.c_str() << std::endl;
		return true;
	}

	//***************************************************************************
	// @brief 현재 고성능 성능 카운터 값을 가져옵니다.
	// @return QPC 카운터 틱 값
	//***************************************************************************
	TimeCheck Now()
	{
		LARGE_INTEGER qpcTime;
		QueryPerformanceCounter(&qpcTime);
		return qpcTime.QuadPart;
	}

	//***************************************************************************
	// @brief 초당 고성능 카운터 주파수를 가져옵니다.
	// @return 초당 주파수 틱 수
	//***************************************************************************
	TimeCheck GetFreq()
	{
		LARGE_INTEGER qpcRate;
		QueryPerformanceFrequency(&qpcRate);
		return qpcRate.QuadPart;
	}

private:
	TimeCheck   m_start;    // 측정 시작 카운터 값
	TimeCheck   m_freq;     // 성능 카운터 주파수
	std::string m_name;     // 측정기 식별 이름

	CCalculatedElapsedTime(const CCalculatedElapsedTime& rhs);
	CCalculatedElapsedTime& operator=(const CCalculatedElapsedTime& rhs);
};
#endif

#endif // ndef __CALCULATEDELAPSEDTIME_H__