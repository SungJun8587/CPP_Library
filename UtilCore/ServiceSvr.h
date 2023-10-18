
//***************************************************************************
// ServiceSvr.h : interface for the CServiceSvr class.
//
//***************************************************************************

#ifndef __SERVICESVR_H__
#define __SERVICESVR_H__

class CServiceSvr
{
public:
	CServiceSvr(const TCHAR* tszAppName, const TCHAR* tszServiceName, const TCHAR* tszDisplayName, const TCHAR* tszServiceDesc);
	virtual	~CServiceSvr(void);

	virtual void	Main(const int32& nArgCnt, TCHAR** pptszArgVec) = 0;
	virtual	bool	Init(char* pszArgv = nullptr) = 0;
	virtual	bool	Start(void) = 0;
	virtual bool	Running(void) = 0;
	virtual	bool	Stop(void) = 0;
	virtual	bool	Cleanup(void) = 0;
	virtual void	serviceMain(DWORD dwArgc, LPTSTR* lpszArgv) = 0;

	void	StartService(TCHAR* tszMachineName, TCHAR* tszServiceName, DWORD dwArgc, LPTSTR* lpszArgv);
	void	StopService(TCHAR* tszMachineName, TCHAR* tszServiceName);
	DWORD	GetServiceState(TCHAR* tszMachineName, TCHAR* tszServiceName);

	BOOL ServiceStop(void)	{ return ( m_hSvrStopEvent && SetEvent(m_hSvrStopEvent) ); }
	bool IsSvrStopped(void)	{ return ::WaitForSingleObject(m_hSvrStopEvent, 0) == WAIT_OBJECT_0; }

protected:
	void InstallService(void);
	void UninstallService(void);

	void serviceCtrl(DWORD dwCtrlCode);
	BOOL controlHandler(DWORD dwCtrlType);

	BOOL registerSCHandler(void);
	BOOL reportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
	void addToMessageLog(const TCHAR* ptszMsg);

public:
	static	shared_ptr<CServiceSvr>	sm_spSvrInstancePtr;

	static void SetSvrInstance(shared_ptr<CServiceSvr>& spSvrInstancePtr) {
		sm_spSvrInstancePtr = spSvrInstancePtr;
	}
	static shared_ptr<CServiceSvr>& GetSvrInstance(void) {
		return sm_spSvrInstancePtr;
	}

	static void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
	static void WINAPI ServiceCtrl(DWORD dwCtrlCode);
	static BOOL WINAPI ControlHandler(DWORD dwCtrlType);

protected:
	TCHAR					m_tszAppName[MAX_PATH];
	TCHAR					m_tszServiceName[MAX_PATH];
	TCHAR					m_tszDisplayName[MAX_PATH];
	TCHAR					m_tszServiceDesc[MAX_PATH];
	TCHAR					m_tszAppPath[FULLPATH_STRLEN];

	SERVICE_STATUS			m_ServiceStatus;
	SERVICE_STATUS_HANDLE	m_ServiceStatusHandle;
	DWORD					m_dwErrCode;

	DWORD					m_dwCheckPoint;
	bool					m_bConsoleMode;
	HANDLE					m_hSvrStopEvent;
};

#endif // ndef __SERVICESVR_H__
