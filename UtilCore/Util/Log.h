
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

//***************************************************************************
// @brief 로그의 레벨 및 종류를 정의하는 열거형입니다.
//***************************************************************************
enum class ELOG_TYPE : short
{
	LOG_TYPE_DEBUG,   // 디버그용 로그
	LOG_TYPE_TRACE,   // 흐름 추적용 로그
	LOG_TYPE_INFO,    // 일반 정보용 로그
	LOG_TYPE_WARNING, // 경고 메시지용 로그
	LOG_TYPE_ERROR,   // 에러 및 실패 로그

	LOG_TYPE_MAX_NUM  // 로그 타입의 총 개수 (인덱스용)
};

//***************************************************************************
// @brief 각 로그 레벨별 콘솔 출력 색상을 정의하는 열거형입니다.
//***************************************************************************
enum class ELOG_TYPE_COLOR : short
{
	LOG_TYPE_DEBUG_COLOR = WHITE,   // 디버그 로그 색상 (흰색)
	LOG_TYPE_TRACE_COLOR = BLUE,    // 추적 로그 색상 (파란색)
	LOG_TYPE_INFO_COLOR = GREEN,    // 정보 로그 색상 (초록색)
	LOG_TYPE_WARNING_COLOR = YELLOW,// 경고 로그 색상 (노란색)
	LOG_TYPE_ERROR_COLOR = RED,     // 에러 로그 색상 (빨간색)
};

//***************************************************************************
// @brief 개별 로그 파일 생성, 파일 쓰기 및 콘솔 출력을 관리하는 클래스입니다.
// @detail 디렉토리와 파일 접두사를 설정받아 형식별 로그 파일을 생성하고,
//         스레드 안전(Thread-safe)하게 파일과 콘솔에 로그를 기록합니다.
//***************************************************************************
class CLog
{
public:
	CLog();
	~CLog() {}

	//***************************************************************************
	// @brief 로그 시스템을 초기화합니다.
	// @param ptszDirecoryName 로그 파일을 저장할 디렉토리 경로
	// @param ptszFileNamePrefix 로그 파일 이름의 접두사
	// @param cLogFmt 로그 파일 생성/분할 형식 (분/시/일/월 등)
	//***************************************************************************
	void Init(const TCHAR* ptszDirecoryName, const TCHAR* ptszFileNamePrefix, const BYTE cLogFmt);

	//***************************************************************************
	// @brief 지정된 로그 타입으로 로그를 기록합니다.
	// @param p_nType 로그의 레벨 타입 (DEBUG, INFO 등)
	// @param ptszLog 기록할 로그 문자열
	// @param bFlag 화면 출력 여부 플래그 (기본값: true)
	//***************************************************************************
	void LogWrite(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag = true);

private:
	//***************************************************************************
	// @brief 실제 로그 파일 작성 및 콘솔 출력을 수행하는 내부 함수입니다.
	// @param p_nType 로그의 레벨 타입
	// @param ptszLog 기록할 로그 문자열
	// @param bFlag 화면 출력 여부 플래그
	//***************************************************************************
	void Write(const ELOG_TYPE p_nType, const TCHAR* ptszLog, const bool bFlag = true);
	void SetTextColor(short sColor);

private:
	std::mutex _mutex; // 동기화를 위한 뮤텍스 객체

	TCHAR _tszDirectory[DIRECTORY_STRLEN]; // 로그 디렉토리 경로 저장 버퍼
	TCHAR _tszFileNamePrefix[FILENAME_STRLEN - DATETIME_STRLEN]; // 로그 파일명 접두사 저장 버퍼
	BYTE  _cLogFmt; // 로그 파일 분할 포맷 값
};

#define LOG_WRITE(LOGLEVEL, LOGFLAG, ...) \
	CLogManager::Instance().Write(LOGLEVEL, LOGFLAG, __VA_ARGS__)

#define LOG_DEBUG(...) \
	CLogManager::Instance().Write(ELOG_TYPE::LOG_TYPE_DEBUG, true, __VA_ARGS__)
#define LOG_TRACE(...) \
	CLogManager::Instance().Write(ELOG_TYPE::LOG_TYPE_TRACE, true, __VA_ARGS__)
#define LOG_INFO(...) \
	CLogManager::Instance().Write(ELOG_TYPE::LOG_TYPE_INFO, true, __VA_ARGS__)
#define LOG_WARNING(...) \
	CLogManager::Instance().Write(ELOG_TYPE::LOG_TYPE_WARNING, true, __VA_ARGS__)
#define LOG_ERROR(...) \
	CLogManager::Instance().Write(ELOG_TYPE::LOG_TYPE_ERROR, true, __VA_ARGS__)

//***************************************************************************
// @brief 전체 로그 시스템을 총괄하는 싱글톤 매니저 클래스입니다.
// @detail 각 로그 타입별 CLog 인스턴스를 관리하며, 가변 인자를 통해
//         편리하게 로그를 남길 수 있는 인터페이스를 제공합니다.
//***************************************************************************
class CLogManager
{
public:
	CLogManager(const CLogManager&) = delete;
	CLogManager& operator=(const CLogManager&) = delete;

	//***************************************************************************
	// @brief CLogManager 싱글톤 인스턴스를 반환합니다.
	// @return CLogManager 싱글톤 객체 참조
	//***************************************************************************
	static CLogManager& Instance()
	{
		static CLogManager instance;
		return instance;
	}

	//***************************************************************************
	// @brief 로그 매니저를 생성하고 하위 로그 객체들을 초기화합니다.
	// @param ptszDirecoryName 로그 파일을 저장할 디렉토리 경로
	//***************************************************************************
	void Create(const TCHAR* ptszDirecoryName);

	//***************************************************************************
	// @brief 가변 인자를 받아 포맷에 맞춘 로그를 생성하고 기록합니다.
	// @param p_nType 로그의 레벨 타입
	// @param bFlag 화면 출력 여부 플래그
	// @param ptszFormat 가변 인자를 포함하는 로그 포맷 문자열
	//***************************************************************************
	void Write(const ELOG_TYPE p_nType, const bool bFlag, const TCHAR* ptszFormat, ...);

private:
	CLogManager() = default;
	~CLogManager() = default;

	CLog m_LogType[static_cast<short>(ELOG_TYPE::LOG_TYPE_MAX_NUM)]; // 로그 타입별 CLog 인스턴스 배열
};

#endif // ndef __LOG_H__