
//***************************************************************************
// Log.cpp : implementation of the CLog class.
//
//***************************************************************************

#include "pch.h"
#include "Log.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief CLog 클래스의 생성자입니다.
// @detail 멤버 변수를 초기화하고 디렉토리 및 파일명 버퍼를 0으로 설정합니다.
//***************************************************************************
CLog::CLog() : _cLogFmt(1)
{
	memset(_tszDirectory, 0, sizeof(_tszDirectory));
	memset(_tszFileNamePrefix, 0, sizeof(_tszFileNamePrefix));
}

//***************************************************************************
// @brief 로그 시스템을 초기화합니다.
// @param ptszDirecoryName 로그 파일을 저장할 디렉토리 경로
// @param ptszFileNamePrefix 로그 파일 이름의 접두사
// @param cLogFmt 로그 파일 생성/분할 형식
//***************************************************************************
void CLog::Init(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const BYTE cLogFmt)
{
	_tcsncpy_s(_tszDirectory, _countof(_tszDirectory), ptszDirecoryName, _TRUNCATE);
	_tcsncpy_s(_tszFileNamePrefix, _countof(_tszFileNamePrefix), ptszFileNamePrefix, _TRUNCATE);

	_cLogFmt = cLogFmt;
}

//***************************************************************************
// @brief 지정된 로그 타입으로 로그 기록을 요청합니다.
// @param p_nType 로그의 레벨 타입
// @param ptszLog 기록할 로그 문자열
// @param bFlag 화면 출력 여부 플래그
//***************************************************************************
void CLog::LogWrite(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag)
{
	Write(p_nType, ptszLog, bFlag);
}

//***************************************************************************
// @brief 실제 파일 쓰기, 콘솔 출력, 디버그 출력을 수행하는 내부 함수입니다.
// @param p_nType 로그의 레벨 타입
// @param ptszLog 기록할 로그 문자열
// @param bFlag 화면 출력 여부 플래그
//***************************************************************************
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
// @brief 콘솔 출력 텍스트 색상을 설정합니다.
// @param sColor 적용할 색상 속성 값
//***************************************************************************
void CLog::SetTextColor(short sColor)
{
	//< FOREGROUND_WHITE | FOREGROUND_INTENSITY 
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), sColor);
}

//***************************************************************************
// @brief 로그 매니저를 생성하고 하위 로그 인스턴스들을 초기화합니다.
// @param ptszDirecoryName 로그 파일을 저장할 디렉토리 경로
//***************************************************************************
void CLogManager::Create(const TCHAR* ptszDirecoryName)
{
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_DEBUG)].Init(ptszDirecoryName, _T("1_DEBUG"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_TRACE)].Init(ptszDirecoryName, _T("1_TRACE"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_INFO)].Init(ptszDirecoryName, _T("1_INFO"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_WARNING)].Init(ptszDirecoryName, _T("1_WARNING"), LOG_FMT_DAILY);
	m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_ERROR)].Init(ptszDirecoryName, _T("1_ERROR"), LOG_FMT_DAILY);
}

//***************************************************************************
// @brief 가변 인자를 받아 포맷에 맞춘 로그를 생성하고 기록을 요청합니다.
// @param p_nType 로그의 레벨 타입
// @param bFlag 화면 출력 여부 플래그
// @param ptszFormat 가변 인자를 포함하는 포맷 문자열
//***************************************************************************
void CLogManager::Write(const ELOG_TYPE p_nType, const bool bFlag, const TCHAR* ptszFormat, ...)
{
	TCHAR tszLog[LOG_MAX_BUFFER_SIZE] = { 0, };
	va_list args;

	va_start(args, ptszFormat);
	_vsntprintf_s(tszLog, _countof(tszLog), LOG_MAX_BUFFER_SIZE, ptszFormat, args);
	va_end(args);

	m_LogType[static_cast<short>(p_nType)].LogWrite(p_nType, tszLog, bFlag);
}