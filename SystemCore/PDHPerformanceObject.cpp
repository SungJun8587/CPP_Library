
#include "pch.h"
#include "PDHPerformanceObject.h"

#pragma comment(lib, "pdh.lib")

PDHInstance::PDHInstance() : m_hQuery(INVALID_HANDLE_VALUE)
{
	for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		hCounter[i] = INVALID_HANDLE_VALUE;
	memset( nValue, 0, sizeof(nValue) );
}
PDHInstance::~PDHInstance(){}

PDHPerformanceObject::PDHPerformanceObject()
{
}
PDHPerformanceObject::~PDHPerformanceObject()
{
	Destroy();	
}

bool PDHPerformanceObject::Create(const TCHAR* p_szObjectName, const TCHAR* p_szCounterNameList, const TCHAR* p_szInstanceNameList)
{
	if( !p_szObjectName || !p_szCounterNameList )
		return false;

	m_stObjectName = p_szObjectName;
	TCHAR* p = (TCHAR*)p_szCounterNameList;
	while( *p && m_vCounterName.size() < PDH_COUNTER_MAX_NUM )
	{
		m_vCounterName.push_back( p );
		p += _tcslen(p) + 1;
	}

	if( p_szInstanceNameList )
	{
		TCHAR* p = (TCHAR*)p_szInstanceNameList;
		while( *p )
		{
			m_vCheckInstanceName.push_back( p );
			p += _tcslen(p) + 1;
		}
	}
	
	return PDHInit();
}
bool PDHPerformanceObject::PDHInit()
{
	//카운터와 인스턴트 이름 파악
	TCHAR* szCounterListBuffer = NULL;
	DWORD dwCounterListSize = 0;
	TCHAR* szInstanceListBuffer = NULL;
	DWORD dwInstanceListSize = 0;

	PdhEnumObjectItems( NULL, NULL, m_stObjectName.c_str(), 
		szCounterListBuffer, &dwCounterListSize, szInstanceListBuffer, &dwInstanceListSize, PERF_DETAIL_WIZARD, 0);

	if( dwCounterListSize <= 0 )
		return false;

	szCounterListBuffer = (TCHAR*)malloc(sizeof(TCHAR*) * dwCounterListSize);
	if( 0 < dwInstanceListSize )
	{
		szInstanceListBuffer = (TCHAR*)malloc(sizeof(TCHAR*) * dwInstanceListSize);
	}
	if( ERROR_SUCCESS != PdhEnumObjectItems( NULL, NULL, m_stObjectName.c_str(), 
		szCounterListBuffer, &dwCounterListSize, szInstanceListBuffer, &dwInstanceListSize, PERF_DETAIL_WIZARD, 0) ) 
	{
		free(szCounterListBuffer);
		if( szInstanceListBuffer )
			free(szInstanceListBuffer);
		return false;
	}

	if (m_vCheckInstanceName.empty() && NULL != szInstanceListBuffer )
	{
		TCHAR* p = (TCHAR*)szInstanceListBuffer;
		while( *p )
		{
			m_vCheckInstanceName.push_back( p );
			p += _tcslen(p) + 1;
		}
	}

	bool bResult = PDHBind();

	free(szCounterListBuffer);
	if( szInstanceListBuffer )
		free(szInstanceListBuffer);

	return bResult;
}
bool PDHPerformanceObject::PDHBind()
{	
	if( !m_vCheckInstanceName.empty() )
	{
		int mInstanceCnt = 0;
		TCHAR szCounterName[PDH_MAX_COUNTER_NAME] = {0,};
		for( std::vector<_tstring>::iterator iterInst = m_vCheckInstanceName.begin(); iterInst != m_vCheckInstanceName.end(); ++iterInst )
			{
			PDHInstance* pInstance = new PDHInstance;//이거 일로 수정ㅋ
			if( ERROR_SUCCESS != PdhOpenQuery( NULL, 0, &pInstance->m_hQuery ) )
			{
				delete pInstance;
				continue;
			}
			int i = 0;
			for (std::vector<_tstring>::iterator iterCnt = m_vCounterName.begin(); iterCnt != m_vCounterName.end(); ++iterCnt)
			{
				_sntprintf_s(szCounterName, _countof(szCounterName), _TRUNCATE, _T("\\%s(%s)\\%s"), m_stObjectName.c_str(), iterInst->c_str(), iterCnt->c_str());
				if( ERROR_SUCCESS != PdhAddCounter( pInstance->m_hQuery, szCounterName, 0, &pInstance->hCounter[i] ) )
				{
					continue;
				}				
				i++;
			}			
			pInstance->stName = iterInst->c_str();
			m_vInstance.push_back( pInstance );
		}
	}
	else 
	{
		PDHInstance* pInstance = new PDHInstance;//이거 일로 수정ㅋ
		if( ERROR_SUCCESS != PdhOpenQuery( NULL, 0, &pInstance->m_hQuery ) )
		{
			delete pInstance;
			return false;
		}
		int i = 0;
		TCHAR szCounterName[PDH_MAX_COUNTER_NAME] = {0,};
		for( std::vector<_tstring>::iterator iter = m_vCounterName.begin(); iter != m_vCounterName.end(); ++iter )
		{
			_sntprintf_s(szCounterName, _countof(szCounterName), _TRUNCATE, _T("\\%s\\%s"), m_stObjectName.c_str(), iter->c_str());
			if( ERROR_SUCCESS != PdhAddCounter( pInstance->m_hQuery, szCounterName, 0, &pInstance->hCounter[i] ) )
			{
				continue;
			}				
			pInstance->stName = m_stObjectName;
			i++;
		}			
		m_vInstance.push_back( pInstance );
	}
	return true;
}
void PDHPerformanceObject::Destroy()
{
	for( std::vector<PDHInstance*>::iterator iter = m_vInstance.begin(); iter != m_vInstance.end(); ++iter )
	{
		for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		{
			if( INVALID_HANDLE_VALUE != (*iter)->hCounter[i] )
				PdhRemoveCounter( (*iter)->hCounter[i] );
		}
		PdhCloseQuery( (*iter)->m_hQuery );
		delete *iter;
	}
	std::vector<PDHInstance*>().swap( m_vInstance );	
}
bool PDHPerformanceObject::Snapshot()
{
	if( m_vInstance.empty() )
		return false;

	PDH_FMT_COUNTERVALUE fmtValue;
	for( std::vector<PDHInstance*>::iterator iter = m_vInstance.begin(); iter != m_vInstance.end(); ++iter )
	{
		if( ERROR_SUCCESS != PdhCollectQueryData( (*iter)->m_hQuery ) )
		{
			continue;
		}	
		for( int i = 0; i < PDH_COUNTER_MAX_NUM; ++i )
		{
			if( INVALID_HANDLE_VALUE == (*iter)->hCounter[i] )
				continue;

			fmtValue.longValue = 0;
			PdhGetFormattedCounterValue( (*iter)->hCounter[i], PDH_FMT_LONG, 0, &fmtValue );
			(*iter)->nValue[i] = fmtValue.longValue;
		}		
	}	
	return true;
}
const std::vector<PDHInstance*>& PDHPerformanceObject::GetData()
{
	return m_vInstance;
}

#include <psapi.h>
#pragma comment(lib, "psapi.lib")
ProcessMemoryInfo::ProcessMemoryInfo() : m_hProcess(INVALID_HANDLE_VALUE), m_nPeakWorkingSetSize(0), m_nWorkingSetSize(0)
{	
	m_hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
}
ProcessMemoryInfo::~ProcessMemoryInfo()
{
	CloseHandle( m_hProcess );
}
bool ProcessMemoryInfo::Snapshot()
{
	if( INVALID_HANDLE_VALUE == m_hProcess )
		return false;

	PROCESS_MEMORY_COUNTERS pmc;
	if( GetProcessMemoryInfo( m_hProcess, &pmc, sizeof(pmc) ) )
	{		
		m_nWorkingSetSize = (unsigned long)pmc.WorkingSetSize >> 20;
		m_nPeakWorkingSetSize = (unsigned long)pmc.PeakWorkingSetSize >> 20;
		m_nNonPagedPoolUsage = (unsigned long)pmc.QuotaNonPagedPoolUsage;
		m_nPeakNonPagedPoolUsage = (unsigned long)pmc.QuotaPeakNonPagedPoolUsage;

		return true;
	}
	return false;	
}