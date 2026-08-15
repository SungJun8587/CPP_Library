
//***************************************************************************
// WindowsServiceBase.h : interface for the WindowsServiceBase class.
//
//***************************************************************************

#ifndef __WINDOWSSERVICEBASE_H__
#define __WINDOWSSERVICEBASE_H__

#pragma once

//***************************************************************************
// @class WindowsServiceBase
// @brief 윈도우 서비스(Windows Service) 구현을 위한 추상 베이스 클래스.
//
// @details
// SCM(Service Control Manager)과 통신하여 윈도우 서비스를 설치, 제거, 시작, 중지하고 
// 상태를 보고할 수 있는 기능을 제공합니다. 또한 콘솔 디버그 모드를 지원하여 
// 개발 환경에서 쉽게 테스트할 수 있으며, 정적 인스턴스를 통해 SCM 콜백을 안전하게 위임합니다.
//
// 주요 특징:
//  - 서비스 모드와 콘솔 디버그 모드 듀얼 지원
//  - SCM과의 원활한 통신을 위한 상태 보고 및 제어 핸들러 등록
//  - RAII 패턴을 활용한 안전한 리소스 관리 및 이벤트 동기화
//***************************************************************************
class WindowsServiceBase
{
public:
	WindowsServiceBase(const TCHAR* ptszAppName, const TCHAR* ptszServiceName, const TCHAR* ptszDisplayName, const TCHAR* ptszServiceDesc);
	virtual	~WindowsServiceBase(void);

	void Main(const int32& nArgCnt, TCHAR** pptszArgVec);
	virtual	bool	Init(const TCHAR* ptszArgv = nullptr);
	virtual	bool	Start(void) = 0;
	virtual bool	Running(void) = 0;
	virtual	bool	Stop(void) = 0;
	virtual	bool	Cleanup(void) = 0;

	void	StartService(TCHAR* ptszMachineName, TCHAR* ptszServiceName, DWORD dwArgc, LPTSTR* lptszArgv);
	void	StopService(TCHAR* ptszMachineName, TCHAR* ptszServiceName);
	DWORD	GetServiceState(TCHAR* ptszMachineName, TCHAR* ptszServiceName);

	BOOL ServiceStop(void) { return (m_hSvrStopEvent && SetEvent(m_hSvrStopEvent)); }
	bool IsSvrStopped(void) { return ::WaitForSingleObject(m_hSvrStopEvent, 0) == WAIT_OBJECT_0; }

protected:
	void installService(void);
	void uninstallService(void);

	void serviceMain(DWORD dwArgc, LPTSTR* lpszArgv);
	void serviceCtrl(DWORD dwCtrlCode);
	BOOL controlHandler(DWORD dwCtrlType);

	BOOL registerSCHandler(void);
	BOOL reportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
	void addToMessageLog(const TCHAR* ptszMsg);

public:
	static	shared_ptr<WindowsServiceBase>	sm_spSvrInstancePtr;

	static void SetSvrInstance(shared_ptr<WindowsServiceBase>& spSvrInstancePtr) { sm_spSvrInstancePtr = spSvrInstancePtr; }
	static shared_ptr<WindowsServiceBase>& GetSvrInstance(void) { return sm_spSvrInstancePtr; }

	static void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
	static void WINAPI ServiceCtrl(DWORD dwCtrlCode);
	static BOOL WINAPI ControlHandler(DWORD dwCtrlType);

protected:
	TCHAR					m_tszAppName[MAX_PATH];         // 애플리케이션 이름
	TCHAR					m_tszServiceName[MAX_PATH];     // 서비스 이름 (SCM 등록용 고유 식별자)
	TCHAR					m_tszDisplayName[MAX_PATH];     // 서비스 표시 이름
	TCHAR					m_tszServiceDesc[MAX_PATH];     // 서비스 설명
	TCHAR					m_tszAppPath[FULLPATH_STRLEN];  // 애플리케이션 실행 파일 경로

	SERVICE_STATUS			m_ServiceStatus;                // 윈도우 서비스 상태 구조체
	SERVICE_STATUS_HANDLE	m_ServiceStatusHandle;          // 서비스 상태 핸들
	DWORD					m_dwErrCode;                    // 마지막 발생한 에러 코드

	DWORD					m_dwCheckPoint;                 // SCM 상태 보고 체크포인트 카운터
	bool					m_bConsoleMode;                 // 콘솔 디버그 모드 여부
	HANDLE					m_hSvrStopEvent;                // 서비스 종료 동기화 이벤트 핸들
};

#endif // ndef __WINDOWSSERVICEBASE_H__