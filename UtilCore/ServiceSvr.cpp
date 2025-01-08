
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
	_tcsncpy_s(m_tszAppName, _countof(m_tszAppName), ptszAppName, _TRUNCATE);
	_tcsncpy_s(m_tszServiceName, _countof(m_tszServiceName), ptszServiceName, _TRUNCATE);
	_tcsncpy_s(m_tszDisplayName, _countof(m_tszDisplayName), ptszDisplayName, _TRUNCATE);
	_tcsncpy_s(m_tszServiceDesc, _countof(m_tszServiceDesc), ptszServiceDesc, _TRUNCATE);

	m_dwCheckPoint = 1;
	m_bConsoleMode = false;
	m_hSvrStopEvent = NULL;

	m_ServiceStatus = { 0 };
	m_ServiceStatusHandle = nullptr;
	m_dwErrCode = 0;

	TCHAR tszFilePath[FULLPATH_STRLEN] = { 0, };
	::GetModuleFileName(NULL, tszFilePath, FULLPATH_STRLEN);

	TCHAR* pDot = _tcsrchr(tszFilePath, '\\');
	_tcsncpy_s(m_tszAppPath, _countof(m_tszAppPath), tszFilePath, (__int32)(pDot - tszFilePath + 1));
}

CServiceSvr::~CServiceSvr(void)
{
	if( m_hSvrStopEvent )
		CloseHandle(m_hSvrStopEvent);
}

//***************************************************************************
//
void CServiceSvr::Main(const int32& nArgCnt, TCHAR** pptszArgVec)
{
	bool bSuccess = false;

	SERVICE_TABLE_ENTRY	dispatchTable[] =
	{
		{ m_tszServiceName, (LPSERVICE_MAIN_FUNCTION)CServiceSvr::ServiceMain },
		{ NULL, NULL }
	};

#ifdef _DEBUG
	bSuccess = true;
	m_bConsoleMode = true;
	serviceMain(nArgCnt, pptszArgVec);
#else
	if( 1 < nArgCnt && '-' == *pptszArgVec[1] )
	{
		bSuccess = true;
		if( _tcsicmp(SERVICE_INSTALL, pptszArgVec[1] + 1) == 0 )
			installService();
		else if( _tcsicmp(SERVICE_UNINSTALL, pptszArgVec[1] + 1) == 0 )
			uninstallService();
		else if( _tcsicmp(SERVICE_CTRL_START, pptszArgVec[1] + 1) == 0 )
			StartService(NULL, m_tszServiceName, nArgCnt - 2, &pptszArgVec[2]);
		else if( _tcsicmp(SERVICE_CTRL_STOP, pptszArgVec[1] + 1) == 0 )
			StopService(NULL, m_tszServiceName);
		else if( _tcsicmp(SERVICE_DEBUG, pptszArgVec[1] + 1) == 0 )
		{
			m_bConsoleMode = true;
			serviceMain(nArgCnt - 1, &pptszArgVec[1]);
		}
		else
			bSuccess = false;
	}
#endif

	(!bSuccess) && StartServiceCtrlDispatcher(dispatchTable);
}

//***************************************************************************
//
bool CServiceSvr::Init(const TCHAR* ptszArgv)
{
	TCHAR tszTempArgv[FULLPATH_STRLEN] = { 0, };

	if( ptszArgv == nullptr || _tcslen(ptszArgv) < 1 )
	{
		_sntprintf_s(tszTempArgv, FULLPATH_STRLEN, _TRUNCATE, _T("config\\server_config.json"));
	}
	else
	{
		_tcsncpy_s(tszTempArgv, FULLPATH_STRLEN, ptszArgv, _TRUNCATE);
	}

	if( false == SERVER_CONFIG->Init(tszTempArgv) )
	{
		printf("SERVER_CONFIG->Init Fail\n");
		exit(-1);
	}

	if( _tcslen(SERVER_CONFIG->GetServerName()) > 0 )
		_tcsncpy_s(m_tszAppName, _countof(m_tszAppName), SERVER_CONFIG->GetServerName(), _TRUNCATE);
	if( _tcslen(SERVER_CONFIG->GetServiceName()) > 0 )
		_tcsncpy_s(m_tszServiceName, _countof(m_tszServiceName), SERVER_CONFIG->GetServiceName(), _TRUNCATE);
	if( _tcslen(SERVER_CONFIG->GetDisplayName()) > 0 )
		_tcsncpy_s(m_tszDisplayName, _countof(m_tszDisplayName), SERVER_CONFIG->GetDisplayName(), _TRUNCATE);

	return true;
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

#ifdef _UNICODE
		::StartService(hSCHandle, dwArgc, (LPCWSTR*)lpszArgv);
#else
		::StartService(hSCHandle, dwArgc, (LPCSTR*)lpszArgv);
#endif

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
	SERVICE_STATUS	ServiceStatus;

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
void CServiceSvr::installService(void)
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
			tszPath,					// service's binary
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
void CServiceSvr::uninstallService(void)
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
void CServiceSvr::serviceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	setlocale(LC_ALL, "korean");

	m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_ServiceStatus.dwServiceSpecificExitCode = 0;

	// register our service control handler:
	if( !registerSCHandler() )
		return;

	// Initialize Stop Event
	m_hSvrStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if( !m_hSvrStopEvent )
		return;

	// report SERVICE_START_PENDING
	if( !reportStatusToSCMgr(SERVICE_START_PENDING, NO_ERROR, REPORT_TIMEOUT) )
		return;

#ifdef _UNICODE	
	::SetConsoleTitleW(m_tszAppName);
#else
	::SetConsoleTitle(m_tszAppName);
#endif

	// 콘솔 사이즈 조정(코드 작동이 잘안됨)
	//////////////////////////////////////////////////////////////////////////
// 	CONSOLE_SCREEN_BUFFER_INFO screenInfo;
// 	::GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenInfo);
// 	int nWidth = screenInfo.srWindow.Right - screenInfo.srWindow.Left + 1;
// 	int nHeight = screenInfo.srWindow.Bottom - screenInfo.srWindow.Top + 1;

// 	COORD dwSize;
// 	dwSize.X = nWidth * 10;
// 	dwSize.Y = nHeight;
// 	::SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), dwSize);

// 	SMALL_RECT rect;
// 	rect.Left = 0;
// 	rect.Right = nWidth * 10 - 1;
// 	rect.Top = 0;
// 	rect.Bottom = nHeight - 1;
// 	::SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);	
	//////////////////////////////////////////////////////////////////////////

	bool bRet = false;
	if( dwArgc <= 1 )
	{
		TCHAR tszTempArgv[FULLPATH_STRLEN] = { 0, };
		_sntprintf_s(tszTempArgv, FULLPATH_STRLEN, _TRUNCATE, _T("config\\server_config.json"));

		bRet = Init(tszTempArgv);
	}
	else
	{
		bRet = Init(lpszArgv[1]);
	}

	// Init
	if( bRet )
	{
		// Start
		if( Start() )
		{
			// report SERVICE_RUNNING
			if( !reportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, REPORT_TIMEOUT) )
				return;

			Running();
		}

		Stop();		// Stop
		Cleanup();	// Clean up
	}
	else
		ServiceStop();

	// report SERVICE_STOPPED
	reportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 0);
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
	BOOL bResult = TRUE;

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
				(LPCTSTR*)ptszStrings,	// array of error strings
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

