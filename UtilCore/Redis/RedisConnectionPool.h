
//***************************************************************************
// RedisConnectionPool.h : interface for the RedisConnectionPool class.
//
//***************************************************************************

#ifndef UC_REDISCONNECTIONPOOL_H
#define UC_REDISCONNECTIONPOOL_H

#include <Network/IOCP/IocpCore.h>
#include <Redis/RedisRedefineDataType.h>
#include <Redis/RedisClient.h>

#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

//***************************************************************************
// @brief 여러 개의 CRedisClient 연결 객체를 동시 분배/관리하는 커넥션 풀 클래스
// @details 요철 시 놀고 있는(Free) 커넥션을 대여하여 명령을 전송하고,
//          응답 콜백 시 해당 커넥션을 다시 큐로 자동 반납 처리합니다.
//
// @code
// // 사용 예시:
// auto pPool = std::make_shared<CRedisConnectionPool>(pIocpCore);
// pPool->Init("127.0.0.1", 6379, 10); // 10개 커넥션 생성
// pPool->SendCommand({"INCR", "VisitorCount"}, [](const RedisValue& res) {
//     // 완료 처리
// });
// @endcode
//***************************************************************************
class CRedisConnectionPool : public std::enable_shared_from_this<CRedisConnectionPool>
{
public:
	CRedisConnectionPool(CIocpCoreRef iocpCore);
	~CRedisConnectionPool();

	bool        Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize);
	void        Clear();

	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);

private:
	CRedisClientRef		PopConnection();
	void                PushConnection(CRedisClientRef pClient);

private:
	CIocpCoreRef                    _iocpCore;               // IOCP 코어 참조
	std::string                     _strIP;                  // 연결 대상 IP
	uint16                          _nPort = 0;              // 연결 대상 포트

	std::mutex                      _lock;					  // 풀 동기화 락
	CVector<CRedisClientRef>		_vecAllClients;          // 생성된 전체 클라이언트 리스트
	CQueue<CRedisClientRef>			_queueFree;              // 사용 가능한 클라이언트 큐

	std::atomic<bool>               _bInitialized{ false };  // 풀 초기화 여부 플래그
};

#endif // ndef UC_REDISCONNECTIONPOOL_H