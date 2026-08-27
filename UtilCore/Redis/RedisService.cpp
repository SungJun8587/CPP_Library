
//***************************************************************************
// RedisService.cpp: implementation of the CRedisService class.
//
//***************************************************************************

#include "pch.h"
#include "RedisService.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisService 생성자
// @param iocpCore IOCP 코어 참조 객체
// @param pJobQueue 결과를 전달받을 메인/대상 스레드의 JobQueue 객체 포인터
//***************************************************************************
CRedisService::CRedisService(CIocpCoreRef iocpCore, std::shared_ptr<CJobQueue> pJobQueue)
	: _jobQueue(pJobQueue)
{
	_pool = std::make_shared<CRedisConnectionPool>(iocpCore);
}

//***************************************************************************
// @brief CRedisService 소멸자
//***************************************************************************
CRedisService::~CRedisService()
{
}

//***************************************************************************
// @brief 내부에 위치한 Redis 커넥션 풀을 초기화함
// @param strIP 서버 IP
// @param nPort 서버 포트
// @param nPoolSize 커넥션 풀 개수
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisService::Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize)
{
	if( !_pool ) return false;
	return _pool->Init(strIP, nPort, nPoolSize);
}

//***************************************************************************
// @brief Redis 명령을 비동기로 실행하고, 결과 전달 콜백을 지정된 JobQueue로 넘겨 스레드 안전성을 확보함
// @details IOCP 수신 스레드에서 결과를 받았을 때 weak_ptr을 통해 CJobQueue의 생존 여부를 확인한 후,
//          DoAsync를 호출하여 로직/메인 스레드의 작업 큐에 콜백 실행 태스크를 안전하게 이관합니다.
// @param vecArgs Redis 명령어 인자 목록
// @param fnMainThreadCallback 대상 스레드에서 안전하게 실행될 콜백 함수
// @return 전송 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisService::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	std::weak_ptr<CJobQueue> weakJobQueue = _jobQueue;

	auto fnIocpCallback = [weakJobQueue, fnMainThreadCallback](const RedisValue& res) {
		if( auto pJobQueue = weakJobQueue.lock() )
		{
			// CJobQueue의 DoAsync를 활용하여 로직 스레드 큐로 작업 이관 및 복사 최적화
			pJobQueue->DoAsync([fnMainThreadCallback, res = std::move(res)]() mutable {
				if( fnMainThreadCallback )
					fnMainThreadCallback(res);
				});
		}
		};

	return _pool->SendCommand(vecArgs, fnIocpCallback);
}