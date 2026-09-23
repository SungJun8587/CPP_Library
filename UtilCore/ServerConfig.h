
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
class CServerConfig
{
public:
	CServerConfig();
	virtual	~CServerConfig();

	//***************************************************************************
	// @brief 서비스 이름을 반환합니다.
	// @return 서비스 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetServiceName() { return _tszServiceName; }

	//***************************************************************************
	// @brief 서비스 표시 이름을 반환합니다.
	// @return 서비스 표시 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetDisplayName(void) { return _tszDisplayName; }

	//***************************************************************************
	// @brief 서버 이름을 반환합니다.
	// @return 서버 이름 문자열 포인터
	//***************************************************************************
	TCHAR* GetServerName() { return _tszServerName; }

	//***************************************************************************
	// @brief 서버 그룹 ID를 반환합니다.
	// @return 서버 그룹 ID
	//***************************************************************************
	uint16 GetServerGroupId() { return _nServerGroupId; }

	//***************************************************************************
	// @brief 서버 채널 ID를 반환합니다.
	// @return 서버 채널 ID
	//***************************************************************************
	uint16 GetServerChannelId() { return _nServerChannelId; }

	//***************************************************************************
	// @brief 서버 IP 주소를 반환합니다.
	// @return 서버 IP 주소 문자열 포인터
	//***************************************************************************
	TCHAR* GetServerIP() { return _tszIP; }

	//***************************************************************************
	// @brief 서버 포트 번호의 참조를 반환합니다.
	// @return 서버 포트 번호 참조
	//***************************************************************************
	uint16& GetServerPort() { return _nServerPort; }

	//***************************************************************************
	// @brief 최대 동시 접속 사용자 수를 반환합니다.
	// @return 최대 동시 접속 사용자 수
	//***************************************************************************
	int32 GetMaxSessionCount() { return _nMaxSessionCount; }

	//***************************************************************************
	// @brief 워커 스레드 수를 반환합니다.
	// @return 워커 스레드 수
	//***************************************************************************
	int32 GetWorkerThreadCnt() { return _nWorkerThreadCnt; }

	//***************************************************************************
	// @brief 서버 노드 목록의 참조를 반환합니다.
	// @return 서버 노드 Vector 참조
	//***************************************************************************
	CVector<CServerNode>& GetServerNodeVec() { return _serverNodeVec; }

	//***************************************************************************
	// @brief DB 노드 목록을 반환합니다.
	// @return DB 노드 Vector 참조
	//***************************************************************************
	CVector<CDBNode>& GetDBNodeVec() { return _dbNodeVec; }

	//***************************************************************************
	// @brief Redis 노드 목록을 반환합니다.
	// @return Redis 노드 Vector 참조
	//***************************************************************************
	CVector<CRedisNode>& GetRedisNodeVec() { return _redisNodeVec; }

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

protected:
	TCHAR						_tszServiceName[MAX_BUFFER_SIZE];	// 서비스 이름
	TCHAR						_tszDisplayName[MAX_BUFFER_SIZE];	// 서비스 표시 이름
	TCHAR						_tszServerName[HOSTNAME_STRLEN];	// 서버 이름
	uint16						_nServerGroupId;					// 서버 그룹 ID				
	uint16						_nServerChannelId;					// 서버 채널 ID

	TCHAR						_tszIP[HOSTNAME_STRLEN];			// 서버 IP 주소
	uint16						_nServerPort;						// 서버 포트 번호

	int32						_nKeepAliveSec;						// KeepAlive 주기(초)
	int32						_nMaxSessionCount;					// 최대 동시 접속 사용자 수
	int32						_nWorkerThreadCnt;					// 워커 스레드 수

	CVector<CServerNode>		_serverNodeVec;						// 연동 서버 노드 목록
	CVector<CDBNode>			_dbNodeVec;							// DB 노드 목록
	CVector<CRedisNode>			_redisNodeVec;						// Redis 노드 목록
};

#endif // ndef UC_SERVERCONFIG_H