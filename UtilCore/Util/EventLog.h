
//***************************************************************************
// EventLog.h : interface for the CEventLog class.
//
//***************************************************************************

#ifndef __EVENTLOG_H__
#define __EVENTLOG_H__

#ifndef __BASEFILE_H__
#include "BaseFile.h"
#endif

#ifndef __CRITICALSECTION_H__
#include "Thread/CriticalSection.h"
#endif

#ifndef __LOCKGUARD_H__
#include "Thread/LockGuard.h"
#endif

#define EVENTLOG_MAX_BUFFER_SIZE 4095
#define TERMINATOR_STRLEN 1

#define EVENTLOG_FMT_DEFAULT 0	
#define EVENTLOG_FMT_SEC	 1
#define EVENTLOG_FMT_MIN	 2
#define EVENTLOG_FMT_HOURS	 3
#define EVENTLOG_FMT_DAILY	 4
#define EVENTLOG_FMT_MONTHLY 5

#define	DEFAULT_TERM_CHAR		_T(",")
#define DEFAULT_FILE_EXTENSION	_T(".log")

//***************************************************************************
//
class CEventLog : public CBaseFile
{
public:
	CEventLog();
	~CEventLog();

	void		InitForLogFile(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const int nConfig, const TCHAR* ptszTerminator = NULL);

	bool		EventLog(const TCHAR* ptszLog, bool bFlag = true);
	bool		EventLog(bool bFlag, const TCHAR* ptszFormat, ...);

	bool		LogString(void);
	void		ResetString(void);
	void		AddString(TCHAR* ptszStr);
	void		AddInteger(int nValue);
	void		AddTerminator(TCHAR* ptszTerm);

private:
	DWORD		OpenFile(void);
	DWORD		CloseFile(void);

private:
	CCriticalSection  m_kLock;

	TCHAR			  m_tszDirectory[DIRECTORY_STRLEN];
	TCHAR			  m_tszFileNamePrefix[FILENAMEEXT_STRLEN - DATETIME_STRLEN];
	TCHAR			  m_tszTerm[TERMINATOR_STRLEN];
	TCHAR			  m_tszBuffer[EVENTLOG_MAX_BUFFER_SIZE];
	BYTE			  m_cLogFmt;
	size_t			  m_nCount;
};

#endif //__EVENTLOG_H__