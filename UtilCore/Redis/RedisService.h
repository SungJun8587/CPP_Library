
//***************************************************************************
// RedisService.h : interface for the CRedisService class.
//
//***************************************************************************

#ifndef __REDISSERVICE_H__
#define __REDISSERVICE_H__

#ifndef __REDISCONNECTIONPOOL_H__
#include <Redis/RedisConnectionPool.h>
#endif

#ifndef __JOBQUEUE_H__
#include <Job/JobQueue.h>
#endif

//***************************************************************************
// @brief 외부 모듈에 노출되는 최상위 Redis 네트워크 서비스 파사드(Facade) 클래스
// @details IOCP 스레드에서 수신한 응답을 지정된 CJobQueue로 이관시켜 주므로
//          메인/로직 스레드에서 멀티스레드 동기화 문제 없이 안전하게 콜백 결과를 받을 수 있습니다.
//
// @code
//	// 사용 예시:
//	auto pJobQueue = std::make_shared<CJobQueue>();
//	CRedisService redisService(pIocpCore, pJobQueue);
//	redisService.Init("127.0.0.1", 6379, 5);
//
//	redisService.SendCommand({"HGETALL", "User:1"}, [](const RedisValue& res) {
//     // 로직/메인 스레드 안전 구역
//	});
// @endcode
//***************************************************************************
class CRedisService
{
public:
	CRedisService(CIocpCoreRef iocpCore, std::shared_ptr<CJobQueue> pJobQueue);
	~CRedisService();

	bool Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize);
	bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback);

private:
	std::shared_ptr<CRedisConnectionPool>	_pool;     // 내부 커넥션 풀
	std::shared_ptr<CJobQueue>				_jobQueue; // 스레드 디스패칭용 JobQueue 참조
};

#endif // ndef __REDISSERVICE_H__