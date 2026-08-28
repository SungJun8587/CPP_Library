
//***************************************************************************
// RedisConnectionPool.cpp: implementation of the CRedisConnectionPool class.
//
//***************************************************************************

#include "pch.h"
#include "RedisConnectionPool.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisConnectionPool 생성자
// @param iocpCore IOCP 코어 참조 객체
//***************************************************************************
CRedisConnectionPool::CRedisConnectionPool(CIocpCoreRef iocpCore)
	: _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief CRedisConnectionPool 소멸자
//***************************************************************************
CRedisConnectionPool::~CRedisConnectionPool()
{
	Clear();
}

//***************************************************************************
// @brief 지정된 풀 크기만큼 CRedisClient를 생성 및 연결함
// @param strIP 서버 IP
// @param nPort 서버 포트
// @param nPoolSize 생성할 커넥션 개수
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisConnectionPool::Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize)
{
	std::lock_guard<std::mutex> lock(_lock);

	_strIP = strIP;
	_nPort = nPort;

	for( int32 i = 0; i < nPoolSize; ++i )
	{
		auto pClient = std::make_shared<CRedisClient>(_iocpCore);
		if( !pClient->Connect(_strIP, _nPort) )
		{
			Clear();
			return false;
		}

		_vecAllClients.push_back(pClient);
		_queueFree.push(pClient);
	}

	_bInitialized = true;
	return true;
}

//***************************************************************************
// @brief 모든 커넥션을 끊고 풀 리소스를 정리함
//***************************************************************************
void CRedisConnectionPool::Clear()
{
	std::lock_guard<std::mutex> lock(_lock);

	while( !_queueFree.empty() )
		_queueFree.pop();

	for( auto& pClient : _vecAllClients )
	{
		if( pClient )
			pClient->Disconnect();
	}

	_vecAllClients.clear();
	_bInitialized = false;
}

//***************************************************************************
// @brief 풀에서 놀고 있는(Free) 커넥션을 팝함
// @return 클라이언트 객체 포인터 (없을 시 nullptr)
//***************************************************************************
CRedisClientRef CRedisConnectionPool::PopConnection()
{
	std::lock_guard<std::mutex> lock(_lock);

	if( _queueFree.empty() )
		return nullptr;

	auto pClient = _queueFree.front();
	_queueFree.pop();
	return pClient;
}

//***************************************************************************
// @brief 사용이 끝난 커넥션을 풀로 반납함
// @param pClient 반납할 클라이언트 객체 포인터
//***************************************************************************
void CRedisConnectionPool::PushConnection(CRedisClientRef pClient)
{
	if( !pClient ) return;

	std::lock_guard<std::mutex> lock(_lock);
	_queueFree.push(pClient);
}

//***************************************************************************
// @brief 풀에서 커넥션을 대여하여 명령을 전송하고 완료 시 자동 반납함
// @param vecArgs Redis 명령어 및 인자
// @param fnCallback 결과 처리 콜백 함수
// @return 전송 요청 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisConnectionPool::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	if( !_bInitialized ) return false;

	auto pClient = PopConnection();
	if( !pClient ) return false;

	std::weak_ptr<CRedisConnectionPool> weakSelf = shared_from_this();

	auto fnWrappedCallback = [weakSelf, pClient, fnCallback](const RedisValue& res) {
		if( fnCallback )
			fnCallback(res);

		if( auto pPool = weakSelf.lock() )
		{
			pPool->PushConnection(pClient);
		}
		};

	if( !pClient->SendCommand(vecArgs, fnWrappedCallback) )
	{
		PushConnection(pClient);
		return false;
	}

	return true;
}