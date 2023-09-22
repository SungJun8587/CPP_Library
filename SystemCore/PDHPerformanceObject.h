
#ifndef _PDH_MONITORING_H_
#define _PDH_MONITORING_H_

#include <pdh.h>

#define PDH_COUNTER_MAX_NUM 10

struct PDHInstance
{
	PDHInstance();
	~PDHInstance();

	_tstring stName;

	HQUERY m_hQuery;
	HCOUNTER hCounter[PDH_COUNTER_MAX_NUM];	
	LONG nValue[PDH_COUNTER_MAX_NUM];
};

class PDHPerformanceObject
{
public:
	PDHPerformanceObject();
	~PDHPerformanceObject();

	//List파라메터는 \0을 구분자로 입력하면 된다.
	//마지막엔 \0을 꼭 붙여야한다.!!!
	//ex1)"Current Bandwidth\0"	"Bytes Received/sec\0" 
	//ex2)"Current Bandwidth\0Bytes Received/sec\0"
	//p_szCounterNameList는 PDH_COUNTER_MAX_NUM 요것만큼만 된다.
	//p_szInstanceNameList가 세팅되면 세팅된 정보만 취한다. 없으면 모든 녀석!
	bool Create(const TCHAR* p_szObjectName, const TCHAR* p_szCounterNameList, const TCHAR* p_szInstanceNameList = NULL);
	void Destroy();

	bool Snapshot();
	const std::vector<PDHInstance*>& GetData();
private:
	bool PDHInit();
	bool PDHBind();
private:
	_tstring m_stObjectName;
	std::vector<_tstring> m_vCounterName;
	std::vector<_tstring> m_vCheckInstanceName;

	std::vector<PDHInstance*> m_vInstance;
};


//메모리 체크는 요걸로 해도 됨...
//psapi.lib 사용
class ProcessMemoryInfo
{
public:
	ProcessMemoryInfo();
	~ProcessMemoryInfo();

	bool Snapshot();

	unsigned long GetPeakWorkingSetSize() { return m_nPeakWorkingSetSize; }
	unsigned long GetWorkingSetSize() { return m_nWorkingSetSize; }
	unsigned long GetNonPagedPoolUsage() { return m_nNonPagedPoolUsage; }
	unsigned long GetPeakNonPagedPoolUsage() { return m_nPeakNonPagedPoolUsage; }
private:
	HANDLE m_hProcess;
	unsigned long m_nPeakWorkingSetSize; //in Mbytes
	unsigned long m_nWorkingSetSize; //in Mbytes
	unsigned long m_nNonPagedPoolUsage; //in bytes
	unsigned long m_nPeakNonPagedPoolUsage; //in bytes
};

#endif //_PDH_MONITORING_H_