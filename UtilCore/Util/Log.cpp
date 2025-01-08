
//***************************************************************************
// Log.cpp : implementation of the CLog class.
//
//***************************************************************************

#include "pch.h"
#include "Log.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CLog::CLog() : _cLogFmt(1)
{
	memset(_tszDirectory, 0, sizeof(_tszDirectory));
	memset(_tszFileNamePrefix, 0, sizeof(_tszFileNamePrefix));
}

//***************************************************************************
//
void CLog::Init(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const BYTE cLogFmt)
{
	_tcsncpy_s(_tszDirectory, _countof(_tszDirectory), ptszDirecoryName, _TRUNCATE);
	_tcsncpy_s(_tszFileNamePrefix, _countof(_tszFileNamePrefix), ptszFileNamePrefix, _TRUNCATE);

	_cLogFmt = cLogFmt;
}

//***************************************************************************
//
void CLog::LogWrite(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag)
{
	Write(p_nType, ptszLog, bFlag);
}

//***************************************************************************
//
void CLog::Write(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag)
{
	TCHAR	tszFullPath[MAX_PATH] = { 0, };
	TCHAR	tszFileNameExt[FILENAMEEXT_STRLEN] = { 0, };
	TCHAR   tszLogFormat[LOG_MAX_BUFFER_SIZE] = { 0, };

	SYSTEMTIME	stime;

	GetLocalTime(&stime);

	std::lock_guard<std::mutex> lockGuard(_mutex);

#ifdef _FILE_LOG
	switch( p_nType )
	{
		case ELOG_TYPE::LOG_TYPE_DEBUG:
			_cLogFmt = LOG_FMT_DAILY;
			break;
		case ELOG_TYPE::LOG_TYPE_TRACE:
			_cLogFmt = LOG_FMT_DAILY;
			break;
		case ELOG_TYPE::LOG_TYPE_INFO:
			_cLogFmt = LOG_FMT_DAILY;
			break;
		case ELOG_TYPE::LOG_TYPE_WARNING:
			_cLogFmt = LOG_FMT_DAILY;
			break;
		case ELOG_TYPE::LOG_TYPE_ERROR:
			_cLogFmt = LOG_FMT_DAILY;
			break;
	}

	switch( _cLogFmt )
	{
		case LOG_FMT_SEC:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, DEFAULT_FILE_EXTENSION);
			break;
		case LOG_FMT_MIN:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, DEFAULT_FILE_EXTENSION);
			break;
		case LOG_FMT_HOURS:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, DEFAULT_FILE_EXTENSION);
			break;
		case LOG_FMT_DAILY:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, DEFAULT_FILE_EXTENSION);
			break;
		case LOG_FMT_MONTHLY:
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, DEFAULT_FILE_EXTENSION);
			break;
		default: // EVENTLOG_FMT_DAILY
			_sntprintf_s(tszFileNameExt, _countof(tszFileNameExt), _TRUNCATE, _T("%s%02d%02d%02d%s"), _tszFileNamePrefix, stime.wYear, stime.wMonth, stime.wDay, DEFAULT_FILE_EXTENSION);
			break;
	}
	_sntprintf_s(tszFullPath, _countof(tszFullPath), _TRUNCATE, _T("%s%s"), _tszDirectory, tszFileNameExt);

	FILE* fp = nullptr;
	_tfopen_s(&fp, tszFullPath, _T("a+"));
	if( fp == 0x00 ) return;

	if( bFlag )
		_ftprintf_s(fp, _T("[%02d-%02d-%02d %02d:%02d:%02d] # %s\n"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, ptszLog);
	else _ftprintf_s(fp, _T("%s\n"), ptszLog);

	fclose(fp);
#endif

#ifdef _CONSOLE_LOG
	short sConsoleTextColor = WHITE;

	if( bFlag )
		_sntprintf_s(tszLogFormat, _countof(tszLogFormat), _TRUNCATE, _T("[%02d-%02d-%02d %02d:%02d:%02d] # %s"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, ptszLog);
	else _sntprintf_s(tszLogFormat, _countof(tszLogFormat), _TRUNCATE, _T("%s"), ptszLog);

	switch( p_nType )
	{
		case ELOG_TYPE::LOG_TYPE_DEBUG:
			sConsoleTextColor = static_cast<short>(ELOG_TYPE_COLOR::LOG_TYPE_DEBUG_COLOR);
			break;
		case ELOG_TYPE::LOG_TYPE_TRACE:
			sConsoleTextColor = static_cast<short>(ELOG_TYPE_COLOR::LOG_TYPE_TRACE_COLOR);
			break;
		case ELOG_TYPE::LOG_TYPE_INFO:
			sConsoleTextColor = static_cast<short>(ELOG_TYPE_COLOR::LOG_TYPE_INFO_COLOR);
			break;
		case ELOG_TYPE::LOG_TYPE_WARNING:
			sConsoleTextColor = static_cast<short>(ELOG_TYPE_COLOR::LOG_TYPE_WARNING_COLOR);
			break;
		case ELOG_TYPE::LOG_TYPE_ERROR:
			sConsoleTextColor = static_cast<short>(ELOG_TYPE_COLOR::LOG_TYPE_ERROR_COLOR);
			break;
	}

	if( sConsoleTextColor != WHITE )
		SetTextColor(sConsoleTextColor);

#ifdef _UNICODE
	std::wcout << tszLogFormat << std::endl;
#else
	std::cout << tszLogFormat << std::endl;
#endif

	SetTextColor(WHITE);
#endif

#ifdef _OUTPUT_LOG
	if( bFlag )
		_sntprintf_s(tszLogFormat, _countof(tszLogFormat), _TRUNCATE, _T("[%02d-%02d-%02d %02d:%02d:%02d] # %s"), stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond, ptszLog);
	else _sntprintf_s(tszLogFormat, _countof(tszLogFormat), _TRUNCATE, _T("%s"), ptszLog);

	OutputDebugString(tszLogFormat);
#endif
}

//***************************************************************************
//
void CLog::SetTextColor(short sColor)
{
	//< FOREGROUND_WHITE | FOREGROUND_INTENSITY 
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), sColor);
}

//***************************************************************************
//
void CLogManager::Create(const TCHAR* ptszDirecoryName)
{
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_DEBUG)].Init(ptszDirecoryName, _T("1_DEBUG"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_TRACE)].Init(ptszDirecoryName, _T("1_TRACE"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_INFO)].Init(ptszDirecoryName, _T("1_INFO"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_ERROR)].Init(ptszDirecoryName, _T("1_WARNING"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_ERROR)].Init(ptszDirecoryName, _T("1_ERROR"), LOG_FMT_DAILY);
}

//***************************************************************************
//
void CLogManager::Write(const ELOG_TYPE p_nType, const bool bFlag, const TCHAR* ptszFormat, ...)
{
	TCHAR tszLog[LOG_MAX_BUFFER_SIZE] = { 0, };
	va_list args;

	va_start(args, ptszFormat);
	_vsntprintf_s(tszLog, _countof(tszLog), LOG_MAX_BUFFER_SIZE, ptszFormat, args);
	va_end(args);

	m_LogType[static_cast<short>(p_nType)].LogWrite(p_nType, tszLog, bFlag);
}


