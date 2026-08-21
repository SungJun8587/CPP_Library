
//***************************************************************************
// PDHPerformanceObject.h: interface for the PDHPerformanceObject and ProcessMemoryInfo classes.
// 
//***************************************************************************

#ifndef __PDHPERFORMANCEOBJECT_H__
#define __PDHPERFORMANCEOBJECT_H__

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

#include <pdh.h>

#define PDH_COUNTER_MAX_NUM 10 // 인스턴스당 동시에 수집 가능한 최대 카운터 개수

//***************************************************************************
// @struct  PDHInstance
// @brief   PDH 인스턴스(예: 프로세스별, 디스크별 등) 하나에 대한 쿼리 핸들과
//          카운터 핸들, 최근 수집 값을 보관하는 구조체입니다.
// @details PDHPerformanceObject가 관리하는 오브젝트(예: "Process")에 여러
//          인스턴스(예: "chrome", "explorer")가 존재할 경우, 인스턴스마다
//          이 구조체가 하나씩 생성되어 독립적인 PDH 쿼리와 카운터 집합을 가집니다.
//***************************************************************************
struct PDHInstance
{
	//***************************************************************************
	// @brief   PDHInstance 구조체의 생성자입니다.
	// @details 쿼리 핸들 및 모든 카운터 핸들을 INVALID_HANDLE_VALUE로, 값 배열을
	//          0으로 초기화합니다.
	//***************************************************************************
	PDHInstance();

	//***************************************************************************
	// @brief   PDHInstance 구조체의 소멸자입니다.
	//***************************************************************************
	~PDHInstance();

	_tstring stName; // 인스턴스 이름 (예: 프로세스명, 디스크명 등)

	HQUERY m_hQuery; // 이 인스턴스에 대한 PDH 쿼리 핸들
	HCOUNTER hCounter[PDH_COUNTER_MAX_NUM]; // 등록된 카운터 핸들 배열
	LONG nValue[PDH_COUNTER_MAX_NUM]; // 가장 최근에 수집된 카운터 값 배열
};

//***************************************************************************
// @class   PDHPerformanceObject
// @brief   PDH(Performance Data Helper) API를 이용해 특정 성능 오브젝트의
//          카운터 값을 주기적으로 수집하는 클래스입니다.
// @details 오브젝트명(예: "Process"), 카운터명 목록(예: "% Processor Time"),
//          선택적으로 인스턴스명 목록을 지정해 Create()로 초기화하면, 이후
//          Snapshot()을 호출할 때마다 각 인스턴스의 카운터 값을 갱신합니다.
//***************************************************************************
class PDHPerformanceObject
{
public:
	//***************************************************************************
	// @brief   PDHPerformanceObject 클래스의 생성자입니다.
	//***************************************************************************
	PDHPerformanceObject();

	//***************************************************************************
	// @brief   PDHPerformanceObject 클래스의 소멸자입니다.
	// @details Destroy()를 호출하여 보유 중인 모든 PDH 쿼리/카운터 핸들을 해제합니다.
	//***************************************************************************
	~PDHPerformanceObject();

	//***************************************************************************
	// @brief   대상 오브젝트/카운터/인스턴스 목록을 지정하여 PDH 쿼리를 초기화합니다.
	// @param   p_szObjectName 대상 성능 오브젝트 이름 (예: "Process", "PhysicalDisk")
	// @param   p_szCounterNameList 수집할 카운터 이름 목록 문자열. 각 이름은 '\0'로
	//          구분하고, 목록의 끝은 '\0'를 한 번 더 붙여 이중 널로 종료합니다.
	//          예1) "Current Bandwidth\0" "Bytes Received/sec\0"
	//          예2) "Current Bandwidth\0Bytes Received/sec\0\0"
	//          최대 PDH_COUNTER_MAX_NUM개까지만 사용됩니다.
	// @param   p_szInstanceNameList 대상 인스턴스 이름 목록 (형식은 위와 동일).
	//          생략(NULL)하면 오브젝트에 존재하는 모든 인스턴스를 대상으로 합니다.
	// @return  bool 초기화 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool Create(const TCHAR* p_szObjectName, const TCHAR* p_szCounterNameList, const TCHAR* p_szInstanceNameList = NULL);

	//***************************************************************************
	// @brief   보유 중인 모든 PDH 쿼리 및 카운터 핸들을 해제하고 인스턴스 목록을 비웁니다.
	// @return  void
	//***************************************************************************
	void Destroy();

	//***************************************************************************
	// @brief   등록된 모든 인스턴스의 PDH 쿼리를 갱신하여 카운터 값을 새로 수집합니다.
	// @return  bool 수집 성공 여부 (true: 성공, false: 등록된 인스턴스가 없을 때)
	//***************************************************************************
	bool Snapshot();

	//***************************************************************************
	// @brief   가장 최근에 수집된 인스턴스별 카운터 정보 목록을 반환합니다.
	// @return  const std::vector<PDHInstance*>& 인스턴스 정보 포인터 벡터
	//***************************************************************************
	const std::vector<PDHInstance*>& GetData() const;
private:
	//***************************************************************************
	// @brief   대상 오브젝트의 카운터/인스턴스 목록을 조회하고 PDHBind()를 호출합니다.
	// @return  bool 초기화 성공 여부
	//***************************************************************************
	bool PDHInit();

	//***************************************************************************
	// @brief   조사된 인스턴스별로 PDH 쿼리를 열고 카운터를 등록합니다.
	// @return  bool 바인딩 성공 여부
	//***************************************************************************
	bool PDHBind();
private:
	_tstring m_stObjectName; // 대상 성능 오브젝트 이름
	std::vector<_tstring> m_vCounterName; // 수집할 카운터 이름 목록
	std::vector<_tstring> m_vCheckInstanceName; // 대상 인스턴스 이름 목록 (비어있으면 전체 인스턴스 대상)

	std::vector<PDHInstance*> m_vInstance; // 인스턴스별 PDH 쿼리/카운터 정보 포인터 벡터
};


//***************************************************************************
// @class   ProcessMemoryInfo
// @brief   현재 프로세스의 메모리 사용량(작업 세트, Non-Paged Pool 등)을
//          조회하는 간단한 유틸리티 클래스입니다.
// @details psapi.lib의 GetProcessMemoryInfo API를 사용하며, 정밀한 메모리
//          프로파일링이 아닌 대략적인 사용량 확인 용도로 사용합니다.
//***************************************************************************
class ProcessMemoryInfo
{
public:
	//***************************************************************************
	// @brief   ProcessMemoryInfo 클래스의 생성자입니다.
	// @details 현재 프로세스에 대한 핸들을 오픈하고 멤버 변수를 초기화합니다.
	//***************************************************************************
	ProcessMemoryInfo();

	//***************************************************************************
	// @brief   ProcessMemoryInfo 클래스의 소멸자입니다.
	// @details 생성자에서 오픈한 프로세스 핸들을 해제합니다.
	//***************************************************************************
	~ProcessMemoryInfo();

	//***************************************************************************
	// @brief   현재 프로세스의 메모리 사용량 정보를 새로 조회합니다.
	// @return  bool 조회 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool Snapshot();

	//***************************************************************************
	// @brief   최대 작업 세트 크기를 반환합니다.
	// @return  unsigned long 최대 작업 세트 크기 (MB 단위)
	//***************************************************************************
	unsigned long GetPeakWorkingSetSize() const { return m_nPeakWorkingSetSize; }

	//***************************************************************************
	// @brief   현재 작업 세트 크기를 반환합니다.
	// @return  unsigned long 현재 작업 세트 크기 (MB 단위)
	//***************************************************************************
	unsigned long GetWorkingSetSize() const { return m_nWorkingSetSize; }

	//***************************************************************************
	// @brief   현재 Non-Paged Pool 사용량을 반환합니다.
	// @return  unsigned long Non-Paged Pool 사용량 (Byte 단위)
	//***************************************************************************
	unsigned long GetNonPagedPoolUsage() const { return m_nNonPagedPoolUsage; }

	//***************************************************************************
	// @brief   최대 Non-Paged Pool 사용량을 반환합니다.
	// @return  unsigned long 최대 Non-Paged Pool 사용량 (Byte 단위)
	//***************************************************************************
	unsigned long GetPeakNonPagedPoolUsage() const { return m_nPeakNonPagedPoolUsage; }
private:
	HANDLE m_hProcess; // 현재 프로세스 핸들 (OpenProcess 결과, 실패 시 NULL)
	unsigned long m_nPeakWorkingSetSize; // 최대 작업 세트 크기 (MB 단위)
	unsigned long m_nWorkingSetSize; // 현재 작업 세트 크기 (MB 단위)
	unsigned long m_nNonPagedPoolUsage; // 현재 Non-Paged Pool 사용량 (Byte 단위)
	unsigned long m_nPeakNonPagedPoolUsage; // 최대 Non-Paged Pool 사용량 (Byte 단위)
};

#endif // ndef __PDHPERFORMANCEOBJECT_H__