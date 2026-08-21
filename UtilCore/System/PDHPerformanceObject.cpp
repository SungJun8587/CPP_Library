
//***************************************************************************
// PDHPerformanceObject.cpp: implementation of the PDHPerformanceObject and ProcessMemoryInfo classes.
// 
//***************************************************************************

#include "pch.h"
#include "PDHPerformanceObject.h"

#pragma comment(lib, "pdh.lib")

//***************************************************************************
// @brief   PDHInstance 구조체의 생성자입니다.
// @details 쿼리 핸들 및 모든 카운터 핸들을 INVALID_HANDLE_VALUE로, 값 배열을
//          0으로 초기화합니다.
//***************************************************************************
PDHInstance::PDHInstance() : m_hQuery(INVALID_HANDLE_VALUE)
{
	for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		hCounter[i] = INVALID_HANDLE_VALUE;
	memset(nValue, 0, sizeof(nValue));
}

//***************************************************************************
// @brief   PDHInstance 구조체의 소멸자입니다.
//***************************************************************************
PDHInstance::~PDHInstance() {}

//***************************************************************************
// @brief   PDHPerformanceObject 클래스의 생성자입니다.
//***************************************************************************
PDHPerformanceObject::PDHPerformanceObject()
{
}

//***************************************************************************
// @brief   PDHPerformanceObject 클래스의 소멸자입니다.
// @details Destroy()를 호출하여 보유 중인 모든 PDH 쿼리/카운터 핸들을 해제합니다.
//***************************************************************************
PDHPerformanceObject::~PDHPerformanceObject()
{
	Destroy();
}

//***************************************************************************
// @brief   대상 오브젝트/카운터/인스턴스 목록을 지정하여 PDH 쿼리를 초기화합니다.
// @param   p_szObjectName 대상 성능 오브젝트 이름 (예: "Process", "PhysicalDisk")
// @param   p_szCounterNameList 수집할 카운터 이름 목록 문자열. 각 이름은 '\0'로
//          구분하고, 목록의 끝은 '\0'를 한 번 더 붙여 이중 널로 종료합니다.
// @param   p_szInstanceNameList 대상 인스턴스 이름 목록 (형식은 위와 동일).
//          생략(NULL)하면 오브젝트에 존재하는 모든 인스턴스를 대상으로 합니다.
// @return  bool 초기화 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool PDHPerformanceObject::Create(const TCHAR* p_szObjectName, const TCHAR* p_szCounterNameList, const TCHAR* p_szInstanceNameList)
{
	if( !p_szObjectName || !p_szCounterNameList )
		return false;

	m_stObjectName = p_szObjectName;

	// [수정] 원본은 (TCHAR*)로 const를 제거하는 불필요한 캐스팅을 사용했습니다.
	// p는 길이 계산과 포인터 이동에만 쓰이고 쓰기를 하지 않으므로 const로 선언합니다.
	const TCHAR* p = p_szCounterNameList;
	while( *p && m_vCounterName.size() < PDH_COUNTER_MAX_NUM )
	{
		m_vCounterName.push_back(p);
		p += _tcslen(p) + 1;
	}

	if( p_szInstanceNameList )
	{
		const TCHAR* p2 = p_szInstanceNameList;
		while( *p2 )
		{
			m_vCheckInstanceName.push_back(p2);
			p2 += _tcslen(p2) + 1;
		}
	}

	return PDHInit();
}

//***************************************************************************
// @brief   대상 오브젝트의 카운터/인스턴스 목록을 조회하고 PDHBind()를 호출합니다.
// @details PdhEnumObjectItems를 두 번 호출하는 표준 PDH 패턴을 사용합니다.
//          1차 호출로 필요한 버퍼 크기를 얻고, 버퍼를 할당한 뒤 2차 호출로
//          실제 카운터/인스턴스 이름 목록을 채웁니다. 호출자가 인스턴스 이름을
//          명시적으로 지정하지 않은 경우, 여기서 조회한 전체 인스턴스 목록을
//          그대로 사용합니다.
// @return  bool 초기화 성공 여부
//***************************************************************************
bool PDHPerformanceObject::PDHInit()
{
	// [수정] Create()가 동일 인스턴스에서 여러 번 호출될 경우(재초기화), 이전에
	// 생성해 둔 PDH 쿼리/카운터 핸들이 정리되지 않고 m_vInstance에 계속 누적되어
	// 핸들이 누수되는 것을 방지하기 위해, 새로 초기화하기 전에 기존 상태를 먼저
	// 정리합니다.
	Destroy();

	// 카운터와 인스턴스 이름 목록을 담을 버퍼
	TCHAR* szCounterListBuffer = NULL;
	DWORD dwCounterListSize = 0;
	TCHAR* szInstanceListBuffer = NULL;
	DWORD dwInstanceListSize = 0;

	// 1차 호출: 버퍼를 NULL로 넘겨 필요한 버퍼 크기(문자 수)만 조회합니다.
	PdhEnumObjectItems(NULL, NULL, m_stObjectName.c_str(),
		szCounterListBuffer, &dwCounterListSize, szInstanceListBuffer, &dwInstanceListSize, PERF_DETAIL_WIZARD, 0);

	if( dwCounterListSize <= 0 )
		return false;

	// [수정] sizeof(TCHAR*)(포인터 크기)가 아닌 sizeof(TCHAR)(문자 크기)를 곱해야
	// 합니다. dwCounterListSize/dwInstanceListSize는 이미 '문자 개수' 단위이므로,
	// 여기에 포인터 크기(4 또는 8바이트)를 곱하면 필요한 바이트 수보다 훨씬 크게
	// 과다 할당됩니다(UNICODE x64 기준 약 4배). 오버플로우로 이어지진 않지만
	// 명백히 잘못된 크기 계산이며 메모리를 불필요하게 낭비합니다.
	szCounterListBuffer = (TCHAR*)malloc(sizeof(TCHAR) * dwCounterListSize);
	if( 0 < dwInstanceListSize )
	{
		szInstanceListBuffer = (TCHAR*)malloc(sizeof(TCHAR) * dwInstanceListSize);
	}

	// 2차 호출: 실제 버퍼에 카운터/인스턴스 이름 목록을 채웁니다.
	if( ERROR_SUCCESS != PdhEnumObjectItems(NULL, NULL, m_stObjectName.c_str(),
		szCounterListBuffer, &dwCounterListSize, szInstanceListBuffer, &dwInstanceListSize, PERF_DETAIL_WIZARD, 0) )
	{
		free(szCounterListBuffer);
		if( szInstanceListBuffer )
			free(szInstanceListBuffer);
		return false;
	}

	// 호출자가 인스턴스 이름을 명시하지 않은 경우, 조회된 전체 인스턴스 목록을 사용합니다.
	if( m_vCheckInstanceName.empty() && NULL != szInstanceListBuffer )
	{
		TCHAR* p = szInstanceListBuffer;
		while( *p )
		{
			m_vCheckInstanceName.push_back(p);
			p += _tcslen(p) + 1;
		}
	}

	bool bResult = PDHBind();

	free(szCounterListBuffer);
	if( szInstanceListBuffer )
		free(szInstanceListBuffer);

	return bResult;
}

//***************************************************************************
// @brief   조사된 인스턴스별로 PDH 쿼리를 열고 카운터를 등록합니다.
// @details m_vCheckInstanceName이 비어있지 않으면 인스턴스별로 별도의 쿼리를
//          생성하고("\\오브젝트(인스턴스)\\카운터" 형식), 비어있으면 인스턴스가
//          없는 오브젝트로 간주해 단일 쿼리를 생성합니다("\\오브젝트\\카운터" 형식).
// @return  bool 바인딩 성공 여부
//***************************************************************************
bool PDHPerformanceObject::PDHBind()
{
	if( !m_vCheckInstanceName.empty() )
	{
		TCHAR szCounterName[PDH_MAX_COUNTER_NAME] = { 0, };
		for( std::vector<_tstring>::iterator iterInst = m_vCheckInstanceName.begin(); iterInst != m_vCheckInstanceName.end(); ++iterInst )
		{
			PDHInstance* pInstance = new PDHInstance;
			if( ERROR_SUCCESS != PdhOpenQuery(NULL, 0, &pInstance->m_hQuery) )
			{
				delete pInstance;
				continue;
			}
			int i = 0;
			for( std::vector<_tstring>::iterator iterCnt = m_vCounterName.begin(); iterCnt != m_vCounterName.end(); ++iterCnt )
			{
				_sntprintf_s(szCounterName, _countof(szCounterName), _TRUNCATE, _T("\\%s(%s)\\%s"), m_stObjectName.c_str(), iterInst->c_str(), iterCnt->c_str());
				if( ERROR_SUCCESS != PdhAddCounter(pInstance->m_hQuery, szCounterName, 0, &pInstance->hCounter[i]) )
				{
					continue;
				}
				i++;
			}
			pInstance->stName = *iterInst;
			m_vInstance.push_back(pInstance);
		}
	}
	else
	{
		PDHInstance* pInstance = new PDHInstance;
		if( ERROR_SUCCESS != PdhOpenQuery(NULL, 0, &pInstance->m_hQuery) )
		{
			delete pInstance;
			return false;
		}
		int i = 0;
		TCHAR szCounterName[PDH_MAX_COUNTER_NAME] = { 0, };
		for( std::vector<_tstring>::iterator iter = m_vCounterName.begin(); iter != m_vCounterName.end(); ++iter )
		{
			_sntprintf_s(szCounterName, _countof(szCounterName), _TRUNCATE, _T("\\%s\\%s"), m_stObjectName.c_str(), iter->c_str());
			if( ERROR_SUCCESS != PdhAddCounter(pInstance->m_hQuery, szCounterName, 0, &pInstance->hCounter[i]) )
			{
				continue;
			}
			i++;
		}
		// [수정] 기존에는 이 대입이 루프 내부에 있어 카운터 개수만큼 동일한 값을
		// 반복해서 대입했습니다. 루프 종료 후 한 번만 대입하도록 옮겼습니다.
		pInstance->stName = m_stObjectName;
		m_vInstance.push_back(pInstance);
	}
	return true;
}

//***************************************************************************
// @brief   보유 중인 모든 PDH 쿼리 및 카운터 핸들을 해제하고 인스턴스 목록을 비웁니다.
// @return  void
//***************************************************************************
void PDHPerformanceObject::Destroy()
{
	for( std::vector<PDHInstance*>::iterator iter = m_vInstance.begin(); iter != m_vInstance.end(); ++iter )
	{
		for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		{
			if( INVALID_HANDLE_VALUE != (*iter)->hCounter[i] )
				PdhRemoveCounter((*iter)->hCounter[i]);
		}
		PdhCloseQuery((*iter)->m_hQuery);
		delete* iter;
	}
	std::vector<PDHInstance*>().swap(m_vInstance);
}

//***************************************************************************
// @brief   등록된 모든 인스턴스의 PDH 쿼리를 갱신하여 카운터 값을 새로 수집합니다.
// @return  bool 수집 성공 여부 (true: 성공, false: 등록된 인스턴스가 없을 때)
//***************************************************************************
bool PDHPerformanceObject::Snapshot()
{
	if( m_vInstance.empty() )
		return false;

	PDH_FMT_COUNTERVALUE fmtValue;
	for( std::vector<PDHInstance*>::iterator iter = m_vInstance.begin(); iter != m_vInstance.end(); ++iter )
	{
		if( ERROR_SUCCESS != PdhCollectQueryData((*iter)->m_hQuery) )
		{
			continue;
		}
		for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		{
			if( INVALID_HANDLE_VALUE == (*iter)->hCounter[i] )
				continue;

			fmtValue.longValue = 0;
			PdhGetFormattedCounterValue((*iter)->hCounter[i], PDH_FMT_LONG, 0, &fmtValue);
			(*iter)->nValue[i] = fmtValue.longValue;
		}
	}
	return true;
}

//***************************************************************************
// @brief   가장 최근에 수집된 인스턴스별 카운터 정보 목록을 반환합니다.
// @return  const std::vector<PDHInstance*>& 인스턴스 정보 포인터 벡터
//***************************************************************************
const std::vector<PDHInstance*>& PDHPerformanceObject::GetData() const
{
	return m_vInstance;
}

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

//***************************************************************************
// @brief   ProcessMemoryInfo 클래스의 생성자입니다.
// @details 현재 프로세스에 대한 핸들을 오픈하고 멤버 변수를 초기화합니다.
//***************************************************************************
ProcessMemoryInfo::ProcessMemoryInfo()
// [수정] OpenProcess()는 실패 시 INVALID_HANDLE_VALUE가 아니라 NULL을 반환합니다
// (INVALID_HANDLE_VALUE는 CreateFile 계열 API의 실패 값입니다). 기존 코드는
// m_hProcess를 INVALID_HANDLE_VALUE로 초기화한 뒤 이 값을 기준으로 Snapshot()의
// 성공 여부를 판단했는데, 실제 실패 값(NULL)과 달라 OpenProcess가 실패해도 그
// 사실을 감지하지 못하는 문제가 있었습니다. NULL을 기준으로 통일합니다.
// [수정] m_nNonPagedPoolUsage/m_nPeakNonPagedPoolUsage가 기존 초기화 목록에서
// 빠져 있어, Snapshot()이 호출되지 않거나 실패하면 Getter가 초기화되지 않은
// 쓰레기 값을 반환하던 문제도 함께 수정했습니다.
	: m_hProcess(NULL)
	, m_nPeakWorkingSetSize(0)
	, m_nWorkingSetSize(0)
	, m_nNonPagedPoolUsage(0)
	, m_nPeakNonPagedPoolUsage(0)
{
	m_hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
}

//***************************************************************************
// @brief   ProcessMemoryInfo 클래스의 소멸자입니다.
// @details 생성자에서 오픈한 프로세스 핸들을 해제합니다.
//***************************************************************************
ProcessMemoryInfo::~ProcessMemoryInfo()
{
	// [수정] OpenProcess 실패로 m_hProcess가 NULL인 경우 CloseHandle(NULL) 호출을
	// 피하도록 방어적으로 검사합니다.
	if( m_hProcess )
		CloseHandle(m_hProcess);
}

//***************************************************************************
// @brief   현재 프로세스의 메모리 사용량 정보를 새로 조회합니다.
// @return  bool 조회 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool ProcessMemoryInfo::Snapshot()
{
	// [수정] OpenProcess 실패 판정 기준을 NULL로 통일했습니다(생성자 설명 참고).
	if( NULL == m_hProcess )
		return false;

	PROCESS_MEMORY_COUNTERS pmc;
	if( GetProcessMemoryInfo(m_hProcess, &pmc, sizeof(pmc)) )
	{
		// [수정] 기존에는 (unsigned long)pmc.WorkingSetSize >> 20 순서로 계산해,
		// 캐스트가 시프트보다 연산자 우선순위가 높은 탓에 64비트 SIZE_T 값을
		// 32비트로 먼저 자른 뒤 시프트했습니다. 작업 세트가 4GB를 넘는 극단적인
		// 경우 잘못된 값이 나올 수 있어, 시프트를 먼저 수행한 뒤 캐스팅하도록
		// 순서를 바로잡았습니다.
		m_nWorkingSetSize = (unsigned long)(pmc.WorkingSetSize >> 20);
		m_nPeakWorkingSetSize = (unsigned long)(pmc.PeakWorkingSetSize >> 20);
		m_nNonPagedPoolUsage = (unsigned long)pmc.QuotaNonPagedPoolUsage;
		m_nPeakNonPagedPoolUsage = (unsigned long)pmc.QuotaPeakNonPagedPoolUsage;

		return true;
	}
	return false;
}