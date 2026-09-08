
//***************************************************************************
// ServerConfig.h: interface for the CServerConfig class.
//
//***************************************************************************

#ifndef UC_SERVERCONFIG_H
#define UC_SERVERCONFIG_H

#include <ServerConnectInfo.h>

//***************************************************************************
// @brief 서버 설정 정보 관리 클래스
// @details JSON 기반의 서버 설정 데이터(서버 정보, DB/Redis 노드, 스레드/풀 설정 등)를 로드하고 관리합니다.
//***************************************************************************
class CServerConfig : public CSingleton<CServerConfig>
{
public:
	CServerConfig(void);
	virtual	~CServerConfig(void);

	bool							Init(const TCHAR* tszServerInfo);

	//***************************************************************************
	// @brief 서버 이름을 반환합니다.
	// @return 서버 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetServerName(void) { return _tszServerName; }

	//***************************************************************************
	// @brief 서비스 이름을 반환합니다.
	// @return 서비스 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetServiceName(void) { return _tszServiceName; }

	//***************************************************************************
	// @brief 표시 이름을 반환합니다.
	// @return 표시 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetDisplayName(void) { return _tszDisplayName; }

	//***************************************************************************
	// @brief 서버 IP 주소를 반환합니다.
	// @return 서버 IP 주소 문자열 포인터
	//***************************************************************************
	TCHAR* GetServerIP(void) { return _tszIP; }

	//***************************************************************************
	// @brief 서버 포트 번호의 참조를 반환합니다.
	// @return 서버 포트 번호 참조
	//***************************************************************************
	uint16& GetServerPort(void) { return _nServerPort; }

	//***************************************************************************
	// @brief 최대 동시 접속 사용자 수를 반환합니다.
	// @return 최대 동시 접속 사용자 수
	//***************************************************************************
	int32 GetMaxSessionCount(void) { return _nMaxSessionCount; }

	//***************************************************************************
	// @brief 워커 스레드 수를 반환합니다.
	// @return 워커 스레드 수
	//***************************************************************************
	int32 GetWorkerThreadCnt(void) { return _nWorkerThreadCnt; }

	//***************************************************************************
	// @brief Redis 연결 풀 크기를 반환합니다.
	// @return Redis 연결 풀 크기
	//***************************************************************************
	int32 GetRedisPoolSize(void) { return _nRedisPoolSize; }

	//***************************************************************************
	// @brief DB 워커 스레드 개수를 반환합니다.
	// @return DB 워커 스레드 수
	//***************************************************************************
	int32 GetDbWorkerThreadCnt(void) { return _nDbWorkerThreadCnt; }

	//***************************************************************************
	// @brief 서버 하트비트 TTL(초)을 반환합니다.
	// @return 하트비트 TTL(초)
	//***************************************************************************
	int32 GetHeartbeatTtlSec(void) { return _nHeartbeatTtlSec; }

	//***************************************************************************
	// @brief 서버 하트비트 갱신 주기(초)를 반환합니다.
	// @return 하트비트 갱신 주기(초)
	//***************************************************************************
	int32 GetHeartbeatIntervalSec(void) { return _nHeartbeatIntervalSec; }

	//***************************************************************************
	// @brief 서버 노드 목록의 참조를 반환합니다.
	// @return 서버 노드 Vector 참조
	//***************************************************************************
	CVector<CServerNode>& GetServerNodeVec(void) { return _serverNodeVec; }

	//***************************************************************************
	// @brief DB 노드 목록을 반환합니다.
	// @return DB 노드 Vector 참조
	//***************************************************************************
	CVector<CDBNode>& GetDBNodeVec(void) { return _dbNodeVec; }

	//***************************************************************************
	// @brief Redis 노드 목록을 반환합니다.
	// @return Redis 노드 Vector 참조
	//***************************************************************************
	CVector<CRedisNode>& GetRedisNodeVec(void) { return _redisNodeVec; }

	//***************************************************************************
	// @brief 지정한 ID의 서버 노드 참조를 반환합니다.
	// @param nID 서버 노드 ID (1-based index)
	// @return 해당 서버 노드 참조
	//***************************************************************************
	CServerNode& GetServerNode(int16& nID) { return _serverNodeVec[nID - 1]; }

	//***************************************************************************
	// @brief 지정한 ID의 DB 노드 참조를 반환합니다.
	// @param nID DB 노드 ID (1-based index)
	// @return 해당 DB 노드 참조
	//***************************************************************************
	CDBNode& GetDBNode(int16& nID) { return _dbNodeVec[nID - 1]; }

	//***************************************************************************
	// @brief 지정한 ID의 Redis 노드 참조를 반환합니다.
	// @param nID Redis 노드 ID (1-based index)
	// @return 해당 Redis 노드 참조
	//***************************************************************************
	CRedisNode& GetRedisNode(int16& nID) { return _redisNodeVec[nID - 1]; }

	void							PrintServerSettingInfo();

	//***************************************************************************
	// @brief 서버 설정 객체를 JSON 형태로 직렬화합니다.
	// @param value JSON 출력 대상 Value 객체 참조
	// @param allocator JSON 메모리 할당자 참조
	//***************************************************************************
	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ServiceName"), _tValue(_tszServiceName, allocator), allocator);
		value.AddMember(_T("DisplayName"), _tValue(_tszDisplayName, allocator), allocator);
		value.AddMember(_T("Name"), _tValue(_tszServerName, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszIP, allocator), allocator);
		value.AddMember(_T("Port"), _nServerPort, allocator);
		value.AddMember(_T("KeepAliveSec"), _nKeepAliveSec, allocator);
		value.AddMember(_T("MaxSessionCount"), _nMaxSessionCount, allocator);
		value.AddMember(_T("WorkerThreadCnt"), _nWorkerThreadCnt, allocator);
		value.AddMember(_T("RedisPoolSize"), _nRedisPoolSize, allocator);
		value.AddMember(_T("DbWorkerThreadCnt"), _nDbWorkerThreadCnt, allocator);
		value.AddMember(_T("HeartbeatTtlSec"), _nHeartbeatTtlSec, allocator);
		value.AddMember(_T("HeartbeatIntervalSec"), _nHeartbeatIntervalSec, allocator);
	}

	//***************************************************************************
	// @brief JSON 객체로부터 서버 설정 정보를 역직렬화합니다.
	// @param value 소스 JSON Value 객체 참조
	//***************************************************************************
	void FromJSON(const _tValue& value)
	{
		_tcsncpy_s(_tszServiceName, _countof(_tszServiceName), value[_T("ServiceName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDisplayName, _countof(_tszDisplayName), value[_T("DisplayName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), value[_T("Name")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), value[_T("IP")].GetString(), _TRUNCATE);
		_nServerPort = value[_T("Port")].GetInt();
		_nKeepAliveSec = value[_T("KeepAliveSec")].GetInt();
		_nMaxSessionCount = value[_T("MaxSessionCount")].GetInt();
		_nWorkerThreadCnt = value[_T("WorkerThreadCnt")].GetInt();
		_nRedisPoolSize = value[_T("RedisPoolSize")].GetInt();
		_nDbWorkerThreadCnt = value[_T("DbWorkerThreadCnt")].GetInt();
		_nHeartbeatTtlSec = value[_T("HeartbeatTtlSec")].GetInt();
		_nHeartbeatIntervalSec = value[_T("HeartbeatIntervalSec")].GetInt();
	}

protected:
	void						Clear(void);

private:
	TCHAR						_tszServiceName[MAX_BUFFER_SIZE]; // 서비스 이름
	TCHAR						_tszDisplayName[MAX_BUFFER_SIZE]; // 표시 이름

	TCHAR						_tszServerName[HOSTNAME_STRLEN];  // 서버 이름
	TCHAR						_tszIP[HOSTNAME_STRLEN];          // 서버 IP 주소
	uint16						_nServerPort;                     // 서버 포트 번호

	int32						_nKeepAliveSec;                   // KeepAlive 주기(초)
	int32						_nMaxSessionCount;                // 최대 동시 접속 사용자 수
	int32						_nWorkerThreadCnt;				  // 워커 스레드 수

	int32						_nRedisPoolSize;                  // Redis 커넥션 풀 크기
	int32						_nDbWorkerThreadCnt;              // DB 처리 워커 스레드 수
	int32						_nHeartbeatTtlSec;                // 하트비트 TTL(초)
	int32						_nHeartbeatIntervalSec;           // 하트비트 갱신 주기(초)

	CVector<CServerNode>		_serverNodeVec;                   // 연동 서버 노드 목록
	CVector<CDBNode>			_dbNodeVec;                       // DB 노드 목록
	CVector<CRedisNode>			_redisNodeVec;                    // Redis 노드 목록
};

#endif // ndef UC_SERVERCONFIG_H