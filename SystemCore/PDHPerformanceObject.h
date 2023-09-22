
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

	//List�Ķ���ʹ� \0�� �����ڷ� �Է��ϸ� �ȴ�.
	//�������� \0�� �� �ٿ����Ѵ�.!!!
	//ex1)"Current Bandwidth\0"	"Bytes Received/sec\0" 
	//ex2)"Current Bandwidth\0Bytes Received/sec\0"
	//p_szCounterNameList�� PDH_COUNTER_MAX_NUM ��͸�ŭ�� �ȴ�.
	//p_szInstanceNameList�� ���õǸ� ���õ� ������ ���Ѵ�. ������ ��� �༮!
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


//�޸� üũ�� ��ɷ� �ص� ��...
//psapi.lib ���
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