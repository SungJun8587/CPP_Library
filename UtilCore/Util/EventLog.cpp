
//***************************************************************************
// EventLog.cpp : implementation of the CEventLog class.
//
//***************************************************************************

#include "pch.h"
#include "EventLog.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CEventLog::CEventLog() : m_cLogFmt(1), m_nCount(0)
{
	memset(m_tszDirectory, 0, sizeof(m_tszDirectory));
	memset(m_tszFileNamePrefix, 0, sizeof(m_tszFileNamePrefix));
	memset(m_tszTerm, 0, sizeof(m_tszTerm));
	memset(m_tszBuffer, 0, sizeof(m_tszBuffer));
}

CEventLog::~CEventLog()
{
}

//***************************************************************************
//
void CEventLog::InitForLogFile(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const int nConfig, const TCHAR* ptszTerm)
{
	_tcsncpy_s(m_tszDirectory, _countof(m_tszDirectory), ptszDirecoryName, _TRUNCATE);
	_tcsncpy_s(m_tszFileNamePrefix, _countof(m_tszFileNamePrefix), ptszFileNamePrefix, _TRUNCATE);

	if( ptszTerm && _tcslen(ptszTerm) > 0 )
		_tcsncpy_s(m_tszTerm, _countof(m_tszTerm), ptszTerm, _TRUNCATE);
	else _tcsncpy_s(m_tszTerm, _countof(m_tszTerm), DEFAULT_TERM_CHAR, _TRUNCATE);

	m_cLogFmt = nConfig;
}

//***************************************************************************
// bFlag = true -> Write Line Header
// bFlag = false -> Don't Write Line Header
bool CEventLog::EventLog(TCHAR* ptszLog, bool bFlag)
{
	bool	bResult = true;
	DWORD	dwHighCount, dwWritten;
	TCHAR	tszBuffer[EVENTLOG_MAX_BUFFER_SIZE];

	SYSTEMTIME	stime;

	dwHighCount = dwWritten = 0;

	CLockGuard<CCriticalSection> lockGuard(m_kLock);

	GetLocalTime(&stime);

	if( bFlag == true ) _sntprintf_s(tszBuffer, _countof(tszBuffer), _TRUNCATE, _T("%02d-%02d-%02d , %02d:%02d:%02d ### %s\r\n"), stime.wYear, stime.wMonth,
									stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, ptszLog);
	else _sntprintf_s(tszBuffer, _countof(tszBuffer), _TRUNCATE, _T("%s\r\n"), ptszLog);

	if( OpenFile() == ERROR_SUCCESS )
	{
		SetPosition(0, NULL, FILE_END);

#ifdef _UNICODE
		char szBuffer[EVENTLOG_MAX_BUFFER_SIZE];

		if( !UnicodeToAnsi(szBuffer, _countof(szBuffer), tszBuffer, wcslen(tszBuffer) + 1) )
		{
			return false;
		}

		dwWritten = Write(szBuffer, (DWORD)strlen(szBuffer), &dwHighCount, NULL);
#else
		dwWritten = Write(tszBuffer, (DWORD)strlen(tszBuffer), &dwHighCount, NULL);
#endif

		if( dwWritten == HandleToULong(INVALID_HANDLE_VALUE) ) bResult = false;
	}

	CloseFile();

	return bResult;
}

//***************************************************************************
// bFlag = true -> Write Line Header
// bFlag = false -> Don't Write Line Header
bool CEventLog::EventLog(bool bFlag, TCHAR* ptszFormat, ...)
{
	bool	bResult = true;
	DWORD	dwHighCount, dwWritten;
	TCHAR	tszBuffer[EVENTLOG_MAX_BUFFER_SIZE];
	TCHAR	tszBufferTemp[EVENTLOG_MAX_BUFFER_SIZE - 100];
	va_list	args;

	SYSTEMTIME	stime;

	CLockGuard<CCriticalSection> lockGuard(m_kLock);

	GetLocalTime(&stime);

	dwHighCount = dwWritten = 0;

	if( !ptszFormat )
	{
		return false;
	}

	va_start(args, ptszFormat);							// Initialize variable arguments.
	_vsntprintf_s(tszBufferTemp, _countof(tszBufferTemp), ptszFormat, args);
	va_end(args);										// Reset variable arguments.

	if( bFlag == true )
		_sntprintf_s(tszBuffer, _countof(tszBuffer), _TRUNCATE, _T("%02d-%02d-%02d , %02d:%02d:%02d ### %s\r\n"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, tszBufferTemp);
	else _sntprintf_s(tszBuffer, _countof(tszBuffer), _TRUNCATE, _T("%s\r\n"), tszBufferTemp);

	if( OpenFile() == ERROR_SUCCESS )
	{
		SetPosition(0, NULL, FILE_END);

#ifdef _UNICODE
		char szBuffer[EVENTLOG_MAX_BUFFER_SIZE];

		if( !UnicodeToAnsi(szBuffer, _countof(szBuffer), tszBuffer, wcslen(tszBuffer) + 1) )
		{
			return false;
		}

		dwWritten = Write(szBuffer, (DWORD)strlen(szBuffer), &dwHighCount, NULL);
#else
		dwWritten = Write(tszBuffer, (DWORD)strlen(tszBuffer), &dwHighCount, NULL);
#endif

		if( dwWritten == HandleToULong(INVALID_HANDLE_VALUE) ) bResult = false;
	}

	CloseFile();

	return bResult;
}

//***************************************************************************
//
bool CEventLog::LogString()
{
	bool	bResult = true;
	DWORD	dwHighCount, dwWritten;

	dwHighCount = dwWritten = 0;

	_tcscat_s(m_tszBuffer, _countof(m_tszBuffer), _T("\r\n"));

	if( OpenFile() == ERROR_SUCCESS )
	{
		SetPosition(0, NULL, FILE_END);

#ifdef _UNICODE
		char szBuffer[EVENTLOG_MAX_BUFFER_SIZE];

		if( !UnicodeToAnsi(szBuffer, _countof(szBuffer), m_tszBuffer, wcslen(m_tszBuffer) + 1) )
		{
			return false;
		}

		dwWritten = Write(szBuffer, (DWORD)strlen(szBuffer), &dwHighCount, NULL);
#else
		dwWritten = Write(m_tszBuffer, (DWORD)strlen(m_tszBuffer), &dwHighCount, NULL);
#endif

		if( dwWritten == HandleToULong(INVALID_HANDLE_VALUE) ) bResult = false;
	}

	CloseFile();

	return bResult;
		}

//***************************************************************************
//
void CEventLog::ResetString()
{
	m_tszBuffer[0] = '\0';
	m_nCount = 0;
}

//***************************************************************************
//
void CEventLog::AddString(TCHAR* ptszStr)
{
	AddTerminator(m_tszTerm);
	_stprintf_s(m_tszBuffer + m_nCount, _countof(m_tszBuffer), _T("%s"), ptszStr);
	m_nCount += _tcslen(m_tszBuffer + m_nCount);
}

//***************************************************************************
//
void CEventLog::AddInteger(int nValue)
{
	AddTerminator(m_tszTerm);
	_stprintf_s(m_tszBuffer + m_nCount, _countof(m_tszBuffer), _T("%i"), nValue);
	m_nCount += _tcslen(m_tszBuffer + m_nCount);
}

//***************************************************************************
//
void CEventLog::AddTerminator(TCHAR* ptszTerm)
{
	if( m_nCount && ptszTerm == NULL )
	{
		_tcscat_s(m_tszBuffer, _countof(m_tszBuffer), m_tszTerm);
		m_nCount += _tcslen(m_tszTerm);
	}
	else if( m_nCount && ptszTerm != NULL )
	{
		_tcscat_s(m_tszBuffer, _countof(m_tszBuffer), ptszTerm);
		m_nCount += _tcslen(ptszTerm);
	}
}

//***************************************************************************
//
DWORD CEventLog::OpenFile()
{
	DWORD	dwError = ERROR_SUCCESS;
	TCHAR	tszFullPath[MAX_PATH];
	TCHAR	tszFileNameExt[FILENAMEEXT_STRLEN];

	SYSTEMTIME	stime;

	if( m_tszDirectory[0] == '\0' ) return ERROR_FILE_NOT_FOUND;

	GetLocalTime(&stime);
	switch( m_cLogFmt )
	{
		case EVENTLOG_FMT_SEC:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, DEFAULT_FILE_EXTENSION);
			break;
		case EVENTLOG_FMT_MIN:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, DEFAULT_FILE_EXTENSION);
			break;
		case EVENTLOG_FMT_HOURS:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, DEFAULT_FILE_EXTENSION);
			break;
		case EVENTLOG_FMT_DAILY:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, DEFAULT_FILE_EXTENSION);
			break;
		case EVENTLOG_FMT_MONTHLY:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, DEFAULT_FILE_EXTENSION);
			break;
		default: // EVENTLOG_FMT_DAILY
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%s"), m_tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, DEFAULT_FILE_EXTENSION);
			break;
	}
	_stprintf_s(tszFullPath, MAX_PATH, _T("%s%s"), m_tszDirectory, tszFileNameExt);

	if( !Create(tszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL) )
	{
		dwError = GetLastError();
	}
	return dwError;
}

//***************************************************************************
//
DWORD CEventLog::CloseFile()
{
	Close();
	return ERROR_SUCCESS;
}
