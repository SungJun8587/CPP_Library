
//***************************************************************************
// Log.h : interface for the CNewLog class.
//
//***************************************************************************

#ifndef __LOG_H__
#define __LOG_H__

#define LOG_MAX_BUFFER_SIZE 8300

#define LOG_FMT_DEFAULT 0	
#define LOG_FMT_SEC		1
#define LOG_FMT_MIN		2
#define LOG_FMT_HOURS	3
#define LOG_FMT_DAILY	4
#define LOG_FMT_MONTHLY 5

#define DEFAULT_FILE_EXTENSION	_T(".log")

//< 조합으로 만들어낸 색상 
#define RED         (FOREGROUND_RED | FOREGROUND_INTENSITY) 
#define BLUE        (FOREGROUND_BLUE | FOREGROUND_INTENSITY) 
#define PINK        (RED | BLUE) 
#define WHITE       (RED | GREEN | BLUE) 
#define GREEN       (FOREGROUND_GREEN | FOREGROUND_INTENSITY) 
#define YELLOW      (RED | GREEN) 
#define SKYBLUE     (GREEN | BLUE)  
#define BLACK		0x0000 
#define RED_BG      (BACKGROUND_RED | BACKGROUND_INTENSITY) 
#define BLUE_BG     (BACKGROUND_BLUE | BACKGROUND_INTENSITY) 
#define PINK_BG     (RED_BG | BLUE_BG) 
#define WHITE_BG    (RED_BG | GREEN_BG | BLUE_BG) 
#define GREEN_BG    (BACKGROUND_GREEN | BACKGROUND_INTENSITY) 
#define YELLOW_BG   (RED_BG | GREEN_BG) 
#define SKYBLUE_BG  (GREEN_BG | BLUE_BG) 

enum class ELOG_TYPE : short
{
	LOG_TYPE_DEBUG,
	LOG_TYPE_TRACE,
	LOG_TYPE_INFO,
	LOG_TYPE_WARNING,
	LOG_TYPE_ERROR,

	LOG_TYPE_MAX_NUM
};

enum class ELOG_TYPE_COLOR : short
{
	LOG_TYPE_DEBUG_COLOR = WHITE,
	LOG_TYPE_TRACE_COLOR = BLUE,
	LOG_TYPE_INFO_COLOR = GREEN,
	LOG_TYPE_WARNING_COLOR = YELLOW,
	LOG_TYPE_ERROR_COLOR = RED,
};

class CLog
{
public:
	CLog();
	~CLog() {}

	void Init(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const BYTE cLogFmt);
	void LogWrite(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag = true);

private:
	void Write(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag = true);
	void SetTextColor(short sColor);

private:
	std::mutex _mutex;

	TCHAR _tszDirectory[DIRECTORY_STRLEN];
	TCHAR _tszFileNamePrefix[FILENAME_STRLEN - DATETIME_STRLEN];
	BYTE  _cLogFmt;
};

#define LOG_WRITE(LOGLEVEL, LOGFLAG, ...) \
	CLogManager::Instance()->Write(LOGLEVEL, LOGFLAG, __VA_ARGS__)

#define LOG_DEBUG(...) \
	CLogManager::Instance()->Write(ELOG_TYPE::LOG_TYPE_DEBUG, true, __VA_ARGS__)
#define LOG_TRACE(...) \
	CLogManager::Instance()->Write(ELOG_TYPE::LOG_TYPE_TRACE, true, __VA_ARGS__)
#define LOG_INFO(...) \
	CLogManager::Instance()->Write(ELOG_TYPE::LOG_TYPE_INFO, true, __VA_ARGS__)
#define LOG_WARNING(...) \
	CLogManager::Instance()->Write(ELOG_TYPE::LOG_TYPE_WARNING, true, __VA_ARGS__)
#define LOG_ERROR(...) \
	CLogManager::Instance()->Write(ELOG_TYPE::LOG_TYPE_ERROR, true, __VA_ARGS__)

class CLogManager
{
public:
	void Create(const TCHAR* ptszDirecoryName);
	void Write(const ELOG_TYPE p_nType, const bool bFlag, const TCHAR* ptszFormat, ...);

	static std::shared_ptr<CLogManager> Instance();

private:
	CLog m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_MAX_NUM)];
};

static std::shared_ptr<CLogManager> logmanager_;

inline std::shared_ptr<CLogManager> CLogManager::Instance() {
	if( !logmanager_ )
		logmanager_ = std::make_shared<CLogManager>();

	return logmanager_;
};

#endif // ndef __LOG_H__