//***************************************************************************
// WindowsServiceBase.cpp: implementation of the WindowsServiceBase class.
//
//***************************************************************************

#include "pch.h"
#include "WindowsServiceBase.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

shared_ptr<WindowsServiceBase> WindowsServiceBase::sm_spSvrInstancePtr;

//***************************************************************************
// @brief 생성자: 기본 문자열 및 상태 변수 초기화, 모듈 파일 경로 추출
// @param ptszAppName 애플리케이션 이름
// @param ptszServiceName 서비스 이름
// @param ptszDisplayName 서비스 표시 이름
// @param ptszServiceDesc 서비스 설명
//***************************************************************************
WindowsServiceBase::WindowsServiceBase(const TCHAR* ptszAppName, const TCHAR* ptszServiceName, const TCHAR* ptszDisplayName, const TCHAR* ptszServiceDesc)
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

//***************************************************************************
// @brief 소멸자: 생성된 정지 이벤트 핸들 닫기
//***************************************************************************
WindowsServiceBase::~WindowsServiceBase(void)
{
	if( m_hSvrStopEvent )
		CloseHandle(m_hSvrStopEvent);
}

//***************************************************************************
// @brief 명령행 인자를 분석하여 서비스 제어 디스패처 또는 관리 명령(-install 등)을 실행
// @param nArgCnt 인자 개수
// @param pptszArgVec 인자 벡터
//***************************************************************************
void WindowsServiceBase::Main(const int32& nArgCnt, TCHAR** pptszArgVec)
{
	bool bSuccess = false;

	SERVICE_TABLE_ENTRY	dispatchTable[] =
	{
		{ m_tszServiceName, (LPSERVICE_MAIN_FUNCTION)WindowsServiceBase::ServiceMain },
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
// @brief 서버 환경 설정 파일 로드 및 객체명 갱신
// @param ptszArgv 설정 파일 경로 인자
// @return true: 초기화 성공, false: 초기화 실패
//***************************************************************************
bool WindowsServiceBase::Init(const TCHAR* ptszArgv)
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
// @brief SCM을 통해 원격/로컬 서비스를 시작
// @param ptszMachineName 대상 머신 이름
// @param ptszServiceName 서비스 이름
// @param dwArgc 전달할 인자 개수
// @param lpszArgv 전달할 인자 벡터
//***************************************************************************
void WindowsServiceBase::StartService(TCHAR* ptszMachineName, TCHAR* ptszServiceName, DWORD dwArgc, LPTSTR* lpszArgv)
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
// @brief SCM을 통해 지정한 서비스에 중지 제어 전송
// @param ptszMachineName 대상 머신 이름
// @param ptszServiceName 서비스 이름
//***************************************************************************
void WindowsServiceBase::StopService(TCHAR* ptszMachineName, TCHAR* ptszServiceName)
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
// @brief SCM을 통해 현재 서비스의 상태 코드를 조회
// @param ptszMachineName 대상 머신 이름
// @param ptszServiceName 서비스 이름
// @return 서비스 현재 상태 (예: SERVICE_RUNNING, SERVICE_STOPPED 등)
//***************************************************************************
DWORD WindowsServiceBase::GetServiceState(TCHAR* ptszMachineName, TCHAR* ptszServiceName)
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
// @brief 현재 바이너리를 SCM에 윈도우 서비스로 등록하고 설명 추가
//***************************************************************************
void WindowsServiceBase::installService(void)
{
	SC_HANDLE	hSCManager = nullptr;
	SC_HANDLE	hSCHandle = nullptr;
	TCHAR		tszPath[FULLPATH_STRLEN] = { 0, };

	__try
	{
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
// @brief 실행 중인 서비스를 중지시킨 후 SCM에서 서비스 삭제
//***************************************************************************
void WindowsServiceBase::uninstallService(void)
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
// @brief 서비스 생명주기(초기화, 실행, 중지, 정리)를 총괄하는 메인 루틴
// @param dwArgc 인자 개수
// @param lpszArgv 인자 벡터
//***************************************************************************
void WindowsServiceBase::serviceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	setlocale(LC_ALL, "korean");

	m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_ServiceStatus.dwServiceSpecificExitCode = 0;

	if( !registerSCHandler() )
		return;

	m_hSvrStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if( !m_hSvrStopEvent )
		return;

	if( !reportStatusToSCMgr(SERVICE_START_PENDING, NO_ERROR, REPORT_TIMEOUT) )
		return;

#ifdef _UNICODE	
	::SetConsoleTitleW(m_tszAppName);
#else
	::SetConsoleTitle(m_tszAppName);
#endif

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

	if( bRet )
	{
		if( Start() )
		{
			if( !reportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, REPORT_TIMEOUT) )
				return;

			Running();
		}

		Stop();
		Cleanup();
	}
	else
		ServiceStop();

	reportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 0);
}

//***************************************************************************
// @brief SCM으로부터 받은 제어 코드에 따라 서비스 상태를 변경하거나 종료 수행
// @param dwCtrlCode 제어 코드
//***************************************************************************
void WindowsServiceBase::serviceCtrl(DWORD dwCtrlCode)
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
// @brief 콘솔 모드에서 발생한 시스템 컨트롤 이벤트(Ctrl+C 등) 처리
// @param dwCtrlType 제어 이벤트 타입
// @return 처리 성공 여부 (TRUE/FALSE)
//***************************************************************************
BOOL WindowsServiceBase::controlHandler(DWORD dwCtrlType)
{
	switch( dwCtrlType )
	{
	case CTRL_BREAK_EVENT:
	case CTRL_C_EVENT:
		ServiceStop();
		return TRUE;
	default:
		return FALSE;
	}
}

//***************************************************************************
// @brief 서비스 모드 또는 콘솔 모드에 맞춰 적절한 제어 핸들러 등록
// @return 등록 성공 여부 (TRUE/FALSE)
//***************************************************************************
BOOL WindowsServiceBase::registerSCHandler(void)
{
	if( !m_bConsoleMode )
	{
		m_ServiceStatusHandle = RegisterServiceCtrlHandler(m_tszServiceName, ServiceCtrl);
		return NULL != m_ServiceStatusHandle;
	}

	return SetConsoleCtrlHandler(WindowsServiceBase::ControlHandler, TRUE);
}

//***************************************************************************
// @brief SCM에 현재 서비스의 상태와 체크포인트 정보를 보고
// @param dwCurrentState 현재 서비스 상태
// @param dwWin32ExitCode Win32 에러 코드
// @param dwWaitHint 대기 시간 힌트
// @return 보고 성공 여부 (TRUE/FALSE)
//***************************************************************************
BOOL WindowsServiceBase::reportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	BOOL bResult = TRUE;

	if( !m_bConsoleMode )
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
// @brief 에러 발생 시 Windows 이벤트 로그(Event Log)에 메시지 기록
// @param ptszMsg 로그에 기록할 메시지
//***************************************************************************
void WindowsServiceBase::addToMessageLog(const TCHAR* ptszMsg)
{
	TCHAR   tszMsg[256];
	HANDLE  hEventSource;
	TCHAR* ptszStrings[2];

	if( !m_bConsoleMode )
	{
		m_dwErrCode = GetLastError();

		hEventSource = RegisterEventSource(NULL, m_tszServiceName);

		_sntprintf_s(tszMsg, 256, _TRUNCATE, _T("%s error: %d"), m_tszServiceName, m_dwErrCode);
		ptszStrings[0] = tszMsg;
		ptszStrings[1] = (TCHAR*)ptszMsg;

		if( hEventSource != NULL )
		{
			ReportEvent(hEventSource,
				EVENTLOG_ERROR_TYPE,
				0,
				0,
				NULL,
				2,
				0,
				(LPCTSTR*)ptszStrings,
				NULL);

			DeregisterEventSource(hEventSource);
		}
	}
}

//***************************************************************************
// @brief SCM 콜백을 싱글톤 인스턴스의 serviceMain으로 위임하는 정적 함수
// @param dwArgc 인자 개수
// @param lpszArgv 인자 벡터
//***************************************************************************
void WINAPI WindowsServiceBase::ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	if( sm_spSvrInstancePtr )
		sm_spSvrInstancePtr->serviceMain(dwArgc, lpszArgv);
}

//***************************************************************************
// @brief SCM 콜백을 싱글톤 인스턴스의 serviceCtrl로 위임하는 정적 함수
// @param dwCtrlCode 제어 코드
//***************************************************************************
void WINAPI WindowsServiceBase::ServiceCtrl(DWORD dwCtrlCode)
{
	if( sm_spSvrInstancePtr )
		sm_spSvrInstancePtr->serviceCtrl(dwCtrlCode);
}

//***************************************************************************
// @brief 콘솔 제어 콜백을 싱글톤 인스턴스의 controlHandler로 위임하는 정적 함수
// @param dwCtrlType 제어 이벤트 타입
// @return 처리 성공 여부 (TRUE/FALSE)
//***************************************************************************
BOOL WINAPI WindowsServiceBase::ControlHandler(DWORD dwCtrlType)
{
	return (sm_spSvrInstancePtr && sm_spSvrInstancePtr->controlHandler(dwCtrlType));
}