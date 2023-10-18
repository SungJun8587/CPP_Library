
//***************************************************************************
// ServiceSvr.cpp: implementation of the CServiceSvr class.
//
//***************************************************************************

#include "pch.h"
#include "ServiceSvr.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

shared_ptr<CServiceSvr> CServiceSvr::sm_spSvrInstancePtr;

CServiceSvr::CServiceSvr(const TCHAR* ptszAppName, const TCHAR* ptszServiceName, const TCHAR* ptszDisplayName, const TCHAR* ptszServiceDesc)
{
	m_dwCheckPoint = 1;
	m_bConsoleMode = false;
	m_hSvrStopEvent = NULL;

	m_ServiceStatus = {0};
	m_ServiceStatusHandle = nullptr;
	m_dwErrCode = 0;

	TCHAR tszFilePath[FULLPATH_STRLEN] = { 0, };
	::GetModuleFileName(NULL, tszFilePath, FULLPATH_STRLEN);

	TCHAR* pDot = _tcsrchr(tszFilePath, '\\');
	_tcsncpy_s(m_tszAppPath, _countof(m_tszAppPath), tszFilePath, (__int32)(pDot - tszFilePath + 1));

	_tcsncpy_s(m_tszAppName, _countof(m_tszAppName), ptszAppName, _TRUNCATE);
	_tcsncpy_s(m_tszServiceName, _countof(m_tszServiceName), ptszServiceName, _TRUNCATE);
	_tcsncpy_s(m_tszDisplayName, _countof(m_tszDisplayName), ptszDisplayName, _TRUNCATE);
	_tcsncpy_s(m_tszServiceDesc, _countof(m_tszServiceDesc), ptszServiceDesc, _TRUNCATE);
}

CServiceSvr::~CServiceSvr(void)
{
	if( m_hSvrStopEvent )
		CloseHandle(m_hSvrStopEvent);
}

//***************************************************************************
//
void CServiceSvr::StartService(TCHAR* ptszMachineName, TCHAR* ptszServiceName, DWORD dwArgc, LPTSTR* lpszArgv)
{
	SC_HANDLE	hSCManager = nullptr;
	SC_HANDLE	hSCHandle = nullptr;

	__try
	{
		hSCManager = ::OpenSCManager(ptszMachineName, NULL, SC_MANAGER_ALL_ACCESS);
		if( !hSCManager )
			__leave;

		hSCHandle = ::OpenService(hSCManager, ptszServiceName, SERVICE_ALL_ACCESS);
		if( !hSCHandle )
			__leave;
		_tprintf_s(_T("%s\n"), lpszArgv[0]);
		//::StartService(hSCHandle, dwArgc, (LPCWSTR*)lpszArgv);
	}
	__finally
	{
		if( hSCHandle )
			::CloseServiceHandle(hSCHandle);

		if( hSCManager )
			::CloseServiceHandle(hSCManager);
	}
}

//***************************************************************************
//
void CServiceSvr::StopService(TCHAR* ptszMachineName, TCHAR* ptszServiceName)
{
	SC_HANDLE	hSCManager = nullptr;
	SC_HANDLE	hSCHandle = nullptr;

	__try
	{
		hSCManager = ::OpenSCManager(ptszMachineName, NULL, SC_MANAGER_ALL_ACCESS);
		if( !hSCManager )
			__leave;

		hSCHandle = ::OpenService(hSCManager, ptszServiceName, SERVICE_ALL_ACCESS);
		if( !hSCHandle )
			__leave;

		SERVICE_STATUS ss;
		::ControlService(hSCHandle, SERVICE_CONTROL_STOP, &ss);
	}
	__finally
	{
		if( hSCHandle )
			::CloseServiceHandle(hSCHandle);

		if( hSCManager )
			::CloseServiceHandle(hSCManager);
	}
}

//***************************************************************************
//
DWORD CServiceSvr::GetServiceState(TCHAR* ptszMachineName, TCHAR* ptszServiceName)
{
	SC_HANDLE		hSCManager = nullptr;
	SC_HANDLE		hSCHandle = nullptr;
	SERVICE_STATUS	ServiceStatus = { 0 };

	__try
	{
		hSCManager = ::OpenSCManager(ptszMachineName, NULL, SC_MANAGER_ALL_ACCESS);
		if( !hSCManager )
			__leave;

		hSCHandle = ::OpenService(hSCManager, ptszServiceName, SERVICE_ALL_ACCESS);
		if( !hSCHandle )
			__leave;

		if( FALSE == ::QueryServiceStatus(hSCHandle, &ServiceStatus) )
			__leave;
	}
	__finally
	{
		if( hSCHandle )
			::CloseServiceHandle(hSCHandle);

		if( hSCManager )
			::CloseServiceHandle(hSCManager);
	}

	return ServiceStatus.dwCurrentState;
}

//***************************************************************************
//
void CServiceSvr::InstallService(void)
{
	SC_HANDLE	hSCManager = nullptr;
	SC_HANDLE	hSCHandle = nullptr;
	TCHAR		tszPath[FULLPATH_STRLEN] = { 0, };

	__try
	{
		// 실행 파일 이름 받아옴
		if( 0 == ::GetModuleFileName(NULL, tszPath, FULLPATH_STRLEN) )
			__leave;

		hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if( !hSCManager )
			__leave;

		hSCHandle = ::CreateService(
			hSCManager,					// hSCManager database
			m_tszServiceName,			// name of service
			m_tszDisplayName,			// name to display
			SERVICE_ALL_ACCESS,			// desired access
			SERVICE_WIN32_OWN_PROCESS,	// service type
			SERVICE_DEMAND_START,		// start type
			SERVICE_ERROR_NORMAL,		// error control type
			tszPath,						// service's binary
			NULL,						// no load ordering group
			NULL,						// no tag identifier
			DEFAULT_DEP_STR,			// dependencies
			NULL,						// LocalSystem account
			NULL);						// no password

		if( hSCHandle )
		{
			SERVICE_DESCRIPTION lpDes;
			lpDes.lpDescription = m_tszServiceDesc;
			::ChangeServiceConfig2(hSCHandle, SERVICE_CONFIG_DESCRIPTION, &lpDes);
		}
	}
	__finally
	{
		if( hSCHandle )
			::CloseServiceHandle(hSCHandle);

		if( hSCManager )
			::CloseServiceHandle(hSCManager);
	}
}

//***************************************************************************
//
void CServiceSvr::UninstallService(void)
{
	SC_HANDLE	hSCManager = nullptr;
	SC_HANDLE	hSCHandle = nullptr;

	__try
	{
		hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if( !hSCManager )
			__leave;

		hSCHandle = ::OpenService(hSCManager, m_tszServiceName, SERVICE_ALL_ACCESS);
		if( !hSCHandle )
			__leave;

		if( ::ControlService(hSCHandle, SERVICE_CONTROL_STOP, &m_ServiceStatus) )
		{
			Sleep(1000);

			while( ::QueryServiceStatus(hSCHandle, &m_ServiceStatus) )
			{
				if( SERVICE_STOP_PENDING != m_ServiceStatus.dwCurrentState )
					break;

				Sleep(1000);
			}
		}

		// now remove the service
		if( FALSE == ::DeleteService(hSCHandle) )
		{
		}
	}
	__finally
	{
		if( hSCHandle )
			::CloseServiceHandle(hSCHandle);

		if( hSCManager )
			::CloseServiceHandle(hSCManager);
	}
}

//***************************************************************************
//
void CServiceSvr::serviceCtrl(DWORD dwCtrlCode)
{
	switch( dwCtrlCode )
	{
		case SERVICE_CONTROL_STOP:
			reportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0);
			ServiceStop();
			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
	reportStatusToSCMgr(m_ServiceStatus.dwCurrentState, NO_ERROR, 0);
}

//***************************************************************************
//
BOOL CServiceSvr::controlHandler(DWORD dwCtrlType)
{
	switch( dwCtrlType )
	{
		case CTRL_BREAK_EVENT:	// use Ctrl+C or Ctrl+Break to simulate
		case CTRL_C_EVENT:		// SERVICE_CONTROL_STOP in debug mode
			ServiceStop();
			return TRUE;
		default:
			return FALSE;
	}
}

//***************************************************************************
//
BOOL CServiceSvr::registerSCHandler(void)
{
	if( !m_bConsoleMode )
	{
		m_ServiceStatusHandle = RegisterServiceCtrlHandler(m_tszServiceName, ServiceCtrl);
		return NULL != m_ServiceStatusHandle;
	}

	// if console
	return SetConsoleCtrlHandler(CServiceSvr::ControlHandler, TRUE);
}

//***************************************************************************
//
BOOL CServiceSvr::reportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	bool bResult = TRUE;

	if( !m_bConsoleMode )	// when debugging we don't report to the SCM
	{
		if( SERVICE_START_PENDING == dwCurrentState )
			m_ServiceStatus.dwControlsAccepted = 0;
		else
			m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

		m_ServiceStatus.dwCurrentState = dwCurrentState;
		m_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
		m_ServiceStatus.dwWaitHint = dwWaitHint;

		if( SERVICE_RUNNING == dwCurrentState || SERVICE_STOPPED == dwCurrentState )
			m_ServiceStatus.dwCheckPoint = 0;
		else
			m_ServiceStatus.dwCheckPoint = m_dwCheckPoint++;

		if( !(bResult = SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus)) )
			addToMessageLog(_T("SetServiceStatus"));
	}

	return bResult;
}

//***************************************************************************
//
void CServiceSvr::addToMessageLog(const TCHAR* ptszMsg)
{
	TCHAR   tszMsg[256];
	HANDLE  hEventSource;
	TCHAR*	ptszStrings[2];

	if( !m_bConsoleMode )
	{
		m_dwErrCode = GetLastError();

		// Use event logging to log the error.
		//
		hEventSource = RegisterEventSource(NULL, m_tszServiceName);

		_sntprintf_s(tszMsg, 256, _TRUNCATE, _T("%s error: %d"), m_tszServiceName, m_dwErrCode);
		ptszStrings[0] = tszMsg;
		ptszStrings[1] = (TCHAR*)ptszMsg;

		if( hEventSource != NULL )
		{
			ReportEvent(hEventSource,	// handle of event source
						EVENTLOG_ERROR_TYPE,	// event type
						0,						// event category
						0,						// event ID
						NULL,					// current user's SID
						2,						// strings in lpszStrings
						0,						// no bytes of raw data
						(LPCTSTR*)ptszStrings,			// array of error strings
						NULL);					// no raw data

			DeregisterEventSource(hEventSource);
		}
	}
}

//***************************************************************************
//
void WINAPI CServiceSvr::ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	if( sm_spSvrInstancePtr )
		sm_spSvrInstancePtr->serviceMain(dwArgc, lpszArgv);
}

//***************************************************************************
//
void WINAPI CServiceSvr::ServiceCtrl(DWORD dwCtrlCode)
{
	if( sm_spSvrInstancePtr )
		sm_spSvrInstancePtr->serviceCtrl(dwCtrlCode);
}

//***************************************************************************
//
BOOL WINAPI CServiceSvr::ControlHandler(DWORD dwCtrlType)
{
	return (sm_spSvrInstancePtr && sm_spSvrInstancePtr->controlHandler(dwCtrlType));
}
