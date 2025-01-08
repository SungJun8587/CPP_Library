
//***************************************************************************
// ServiceSvr.h : interface for the CServiceSvr class.
//
//***************************************************************************

#ifndef __SERVICESVR_H__
#define __SERVICESVR_H__

class CServiceSvr
{
public:
	CServiceSvr(const TCHAR* ptszAppName, const TCHAR* ptszServiceName, const TCHAR* ptszDisplayName, const TCHAR* ptszServiceDesc);
	virtual	~CServiceSvr(void);

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
	static	shared_ptr<CServiceSvr>	sm_spSvrInstancePtr;

	static void SetSvrInstance(shared_ptr<CServiceSvr>& spSvrInstancePtr) { sm_spSvrInstancePtr = spSvrInstancePtr; }
	static shared_ptr<CServiceSvr>& GetSvrInstance(void) { return sm_spSvrInstancePtr; }

	static void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
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
