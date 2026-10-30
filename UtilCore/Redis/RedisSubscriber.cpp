
//***************************************************************************
// RedisSubscriber.cpp: implementation of the CRedisSubscriber class.
//
//***************************************************************************

#include "pch.h"
#include "RedisSubscriber.h"
#include <Redis/RedisHelper.h>
#include <Util/DateTimeUtil.h>

// 재연결 백오프/생존 점검 상수는 RedisDefine.h의 Redis 네임스페이스에 있다(풀과 같은 백오프 기준선).

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisSubscriber 생성자
// @param iocpCore 구독 연결을 등록할 IOCP 코어
// @param pJobQueue 메시지 콜백을 이관해 실행할 JobQueue
//***************************************************************************
CRedisSubscriber::CRedisSubscriber(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue)
	: _iocpCore(iocpCore)
	, _jobQueue(pJobQueue)
{
}

//***************************************************************************
// @brief CRedisSubscriber 소멸자 — Shutdown()을 호출해 재연결 스레드와 연결을 정리함
//***************************************************************************
CRedisSubscriber::~CRedisSubscriber()
{
	Shutdown();
}

//***************************************************************************
// @brief 구독 연결을 만들고 재연결 스레드를 시작함
//***************************************************************************
bool CRedisSubscriber::Init(const std::string& strIP, const uint16 nPort, const int32 nReconnectIntervalSec, const std::string& strUserId, const std::string& strPassword)
{
	if( !_iocpCore || !_jobQueue || strIP.empty() || nPort == 0 )
	{
		LOG_ERROR(_T("CRedisSubscriber::Init: invalid argument"));
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(_lock);
		if( _bInitialized )
		{
			LOG_ERROR(_T("CRedisSubscriber::Init: already initialized"));
			return false;
		}
	}

	// 클라이언트가 가진 push 콜백이 이 객체를 강하게 잡으면 순환 참조가 되므로 weak_ptr로 잡는다.
	std::weak_ptr<CRedisSubscriber> weakSelf = shared_from_this();

	auto pClient = std::make_shared<CRedisClient>(_iocpCore);
	pClient->SetPushCallback([weakSelf](RedisValue&& value) {
		if( auto pSelf = weakSelf.lock() )
			pSelf->OnPush(std::move(value));
		});

	// 첫 접속 실패는 치명적이지 않다 — 재연결 스레드가 백오프로 계속 시도하고, 접속되면 등록된 구독을 반영한다.
	if( !pClient->Connect(strIP, nPort, 0, Redis::kConnectTimeoutMs, strUserId, strPassword) )
	{
		LOG_ERROR(_T("CRedisSubscriber::Init: initial connect failed, will keep retrying. host(%hs), port(%d)"), strIP.c_str(), static_cast<int32>(nPort));
	}

	{
		std::lock_guard<std::mutex> lock(_lock);
		_pClient = pClient;
		_strIP = strIP;
		_nPort = nPort;
		_strUserId = strUserId;
		_strPassword = strPassword;
		_nReconnectIntervalSec = (nReconnectIntervalSec > 0) ? nReconnectIntervalSec : Redis::kSubscriberReconnectIntervalSec;
		_nEpoch++;
		_cmdQueue.clear();
		_bCmdInFlight = false;
		_bResyncing = false;
		_bInitialized = true;
	}

	_stopping.store(false);
	_reconnectThread = std::thread([this]() { ReconnectLoop(); });
	return true;
}

//***************************************************************************
// @brief 재연결 스레드를 멈추고 연결을 닫음
// @details 먼저 _bInitialized를 내려 새 Subscribe()를 막고(이 호출이 종료 권한을 갖는다),
//          재연결 스레드를 join한 뒤에 연결을 끊고 구독 목록을 비운다.
//***************************************************************************
void CRedisSubscriber::Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return;

		_bInitialized = false;
	}

	_stopping.store(true);

	// 대기 뮤텍스를 한 번 거친 뒤 알린다. 스레드가 조건을 검사하고 대기에 들어가기 직전에 알림이
	// 끼면 유실되어, join이 점검 주기만큼 지연될 수 있기 때문이다.
	{
		std::lock_guard<std::mutex> lock(_reconnectLock);
	}
	_reconnectCv.notify_all();
	if( _reconnectThread.joinable() )
		_reconnectThread.join();

	CRedisClientRef pClient;
	{
		std::lock_guard<std::mutex> lock(_lock);
		pClient = std::move(_pClient);
		_pClient = nullptr;
		_channelMap.clear();
		_patternMap.clear();
		_cmdQueue.clear();
		_bCmdInFlight = false;
		_bResyncing = false;
		_nEpoch++;
	}

	// 대기 중이던 명령 콜백이 에러로 호출되지만, 에폭이 바뀌었으므로 무시된다.
	if( pClient )
		pClient->Disconnect();
}

//***************************************************************************
// @brief 채널 구독 등록
//***************************************************************************
bool CRedisSubscriber::Subscribe(const std::string& strChannel, RedisMessageCallback fnCallback)
{
	if( strChannel.empty() || !fnCallback )
		return false;

	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return false;

		_channelMap[strChannel] = std::make_shared<const RedisMessageCallback>(std::move(fnCallback));
		_cmdQueue.push_back(Redis::MakeArgs("SUBSCRIBE", strChannel));
	}

	PumpCommands();
	return true;
}

//***************************************************************************
// @brief 채널 구독 해제
//***************************************************************************
bool CRedisSubscriber::Unsubscribe(const std::string& strChannel)
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return false;

		if( _channelMap.erase(strChannel) == 0 )
			return false;

		_cmdQueue.push_back(Redis::MakeArgs("UNSUBSCRIBE", strChannel));
	}

	PumpCommands();
	return true;
}

//***************************************************************************
// @brief 패턴 구독 등록
//***************************************************************************
bool CRedisSubscriber::PSubscribe(const std::string& strPattern, RedisMessageCallback fnCallback)
{
	if( strPattern.empty() || !fnCallback )
		return false;

	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return false;

		_patternMap[strPattern] = std::make_shared<const RedisMessageCallback>(std::move(fnCallback));
		_cmdQueue.push_back(Redis::MakeArgs("PSUBSCRIBE", strPattern));
	}

	PumpCommands();
	return true;
}

//***************************************************************************
// @brief 패턴 구독 해제
//***************************************************************************
bool CRedisSubscriber::PUnsubscribe(const std::string& strPattern)
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return false;

		if( _patternMap.erase(strPattern) == 0 )
			return false;

		_cmdQueue.push_back(Redis::MakeArgs("PUNSUBSCRIBE", strPattern));
	}

	PumpCommands();
	return true;
}

//***************************************************************************
// @brief 현재 구독 연결이 살아 있는지 확인함
//***************************************************************************
bool CRedisSubscriber::IsConnected() const
{
	std::lock_guard<std::mutex> lock(_lock);
	return _bInitialized && _pClient && _pClient->IsConnected();
}

//***************************************************************************
// @brief 서버가 보낸 message/pmessage를 구독 목록에서 찾아 콜백을 JobQueue로 이관함
// @details IOCP 워커 스레드에서 호출된다. 형식이 맞지 않거나 해제된 구독이면 버린다.
//***************************************************************************
void CRedisSubscriber::OnPush(RedisValue&& value)
{
	if( value.eType != ERedisType::Array || value.arrayVal.empty() )
		return;

	const std::string& strKind = value.arrayVal[0].strVal;

	std::string strChannel;
	std::string strPayload;
	RedisMessageCallbackPtr pCallback;

	// 채널/본문은 수신한 값에서 move한다(복사 없음). 콜백 조회는 락 안에서 shared_ptr만 복사한다.
	if( strKind == "message" && value.arrayVal.size() >= 3 )
	{
		strChannel = std::move(value.arrayVal[1].strVal);
		strPayload = std::move(value.arrayVal[2].strVal);

		std::lock_guard<std::mutex> lock(_lock);
		auto it = _channelMap.find(strChannel);
		if( it != _channelMap.end() )
			pCallback = it->second;
	}
	else if( strKind == "pmessage" && value.arrayVal.size() >= 4 )
	{
		const std::string& strPattern = value.arrayVal[1].strVal;
		strChannel = std::move(value.arrayVal[2].strVal);
		strPayload = std::move(value.arrayVal[3].strVal);

		std::lock_guard<std::mutex> lock(_lock);
		auto it = _patternMap.find(strPattern);
		if( it != _patternMap.end() )
			pCallback = it->second;
	}

	if( !pCallback )
		return;

	_jobQueue->DoAsync([pCallback = std::move(pCallback), strChannel = std::move(strChannel), strPayload = std::move(strPayload)]() {
		(*pCallback)(strChannel, strPayload);
		});
}

//***************************************************************************
// @brief 구독 명령의 확인 응답(또는 에러/연결 끊김)을 처리하고 다음 명령을 보냄
// @param nEpoch 이 명령을 보낸 시점의 연결 에폭. 재연결 뒤에 도착한 이전 연결의 응답은 무시한다.
//***************************************************************************
void CRedisSubscriber::OnCommandReply(const uint64 nEpoch, const RedisValue& value)
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( nEpoch != _nEpoch )
			return;

		_bCmdInFlight = false;
	}

	// 연결이 끊겨서 온 에러는 재연결 스레드가 다루므로 로그를 남기지 않는다.
	if( value.eType == ERedisType::Error && IsConnected() )
		LOG_ERROR(_T("CRedisSubscriber: command error reply - %hs"), value.strVal.c_str());

	PumpCommands();
}

//***************************************************************************
// @brief 대기 큐의 명령을 하나씩 전송함
// @details 한 시점에 미완료 명령은 하나뿐이다. 전송 실패 시 CRedisClient::SendCommand()가
//          콜백을 동기로 호출하고 그 안에서 이 함수가 다시 불리므로, _bPumping으로
//          재진입을 막고 바깥 루프가 큐를 소진하게 한다(재귀 깊이가 큐 길이에 비례하지 않게).
//***************************************************************************
void CRedisSubscriber::PumpCommands()
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( _bPumping )
			return;

		_bPumping = true;
	}

	for( ;; )
	{
		CVector<std::string> vecArgs;
		CRedisClientRef pClient;
		uint64 nEpoch = 0;

		// "게이트 확인 → 명령 꺼내기 → SendCommand 호출"을 _sendLock으로 묶는다. ReconnectLoop()가 _bResyncing을
		// 켠 뒤 이 락을 한 번 거치므로, 게이트를 이미 통과해 전송 중인 명령은 Connect()가 시작되기 전에 끝난다.
		// SendCommand()가 콜백을 동기 호출해도 콜백(OnCommandReply → PumpCommands)은 _bPumping 때문에 이 락을 다시 잡지 않는다.
		std::lock_guard<std::mutex> sendGuard(_sendLock);
		{
			std::lock_guard<std::mutex> lock(_lock);
			if( !_bInitialized || _bResyncing || _bCmdInFlight || _cmdQueue.empty() || !_pClient )
			{
				_bPumping = false;
				return;
			}

			vecArgs = std::move(_cmdQueue.front());
			_cmdQueue.pop_front();
			_bCmdInFlight = true;
			_nCmdSentMs = ptime::MonotonicNowMillis();
			pClient = _pClient;
			nEpoch = _nEpoch;
		}

		std::weak_ptr<CRedisSubscriber> weakSelf = shared_from_this();
		pClient->SendCommand(vecArgs, [weakSelf, nEpoch](const RedisValue& res) {
			if( auto pSelf = weakSelf.lock() )
				pSelf->OnCommandReply(nEpoch, res);
			});
	}
}

//***************************************************************************
// @brief 재연결에 성공한 뒤 등록된 모든 채널·패턴을 다시 구독함
// @details 새 에폭을 시작하고(이전 연결의 지연 응답 무시), 전송 대기 큐를 구독 목록
//          전체로 다시 채운다. 이전 연결 시절의 대기 명령은 전부 버린다.
//***************************************************************************
void CRedisSubscriber::RebuildSubscriptions()
{
	{
		std::lock_guard<std::mutex> lock(_lock);
		_nEpoch++;
		_bCmdInFlight = false;
		_bResyncing = false;
		_cmdQueue.clear();

		for( const auto& kv : _channelMap )
			_cmdQueue.push_back(Redis::MakeArgs("SUBSCRIBE", kv.first));

		for( const auto& kv : _patternMap )
			_cmdQueue.push_back(Redis::MakeArgs("PSUBSCRIBE", kv.first));
	}

	PumpCommands();
}

//***************************************************************************
// @brief 연결이 살아 있는 것으로 보일 때 실제로 응답하는지 점검함
// @details 진행 중인 명령이 kSubscriberCommandTimeoutMs를 넘기도록 응답이 없으면 연결이 조용히
//          죽은 것으로 보고 끊는다 — 끊으면 대기 콜백이 에러로 끝나고 다음 점검에서
//          재연결 경로로 넘어간다. 명령이 없고 대기 큐도 비었으면 kSubscriberPingIntervalMs마다
//          PING을 큐에 넣는다. PING도 다른 구독 명령과 같은 큐로 직렬화되므로 한 연결에
//          미완료 명령이 둘이 되지 않는다.
//***************************************************************************
void CRedisSubscriber::CheckLiveness(const CRedisClientRef& pClient)
{
	const int64 nNowMs = ptime::MonotonicNowMillis();
	bool bTimedOut = false;
	bool bPing = false;
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_bInitialized )
			return;

		if( _bCmdInFlight )
		{
			bTimedOut = nNowMs - _nCmdSentMs > Redis::kSubscriberCommandTimeoutMs;
		}
		else if( _cmdQueue.empty() && !_bResyncing && nNowMs - _nLastPingMs >= Redis::kSubscriberPingIntervalMs )
		{
			_cmdQueue.push_back(Redis::MakeArgs("PING"));
			_nLastPingMs = nNowMs;
			bPing = true;
		}
	}

	if( bTimedOut )
	{
		LOG_ERROR(_T("CRedisSubscriber: 명령 무응답 %lldms 초과 — 연결을 끊고 재연결합니다"), static_cast<long long>(Redis::kSubscriberCommandTimeoutMs));
		pClient->Disconnect();
	}
	else if( bPing )
	{
		PumpCommands();
	}
}

//***************************************************************************
// @brief 연결 상태를 주기적으로 점검하고, 끊겨 있으면 백오프를 두고 다시 접속함
//***************************************************************************
void CRedisSubscriber::ReconnectLoop()
{
	std::string strIP;
	uint16 nPort = 0;
	int32 nIntervalSec = Redis::kSubscriberReconnectIntervalSec;
	std::string strUserId;
	std::string strPassword;
	{
		std::lock_guard<std::mutex> lock(_lock);
		strIP = _strIP;
		nPort = _nPort;
		nIntervalSec = _nReconnectIntervalSec;
		strUserId = _strUserId;
		strPassword = _strPassword;
	}

	Redis::SReconnectBackoff backoff;

	while( !_stopping.load() )
	{
		{
			std::unique_lock<std::mutex> guard(_reconnectLock);
			if( _reconnectCv.wait_for(guard, std::chrono::seconds(nIntervalSec), [this] { return _stopping.load(); }) )
				break;
		}

		CRedisClientRef pClient;
		{
			std::lock_guard<std::mutex> lock(_lock);
			pClient = _pClient;
		}

		if( !pClient )
			continue;

		if( pClient->IsConnected() )
		{
			backoff.Reset();
			CheckLiveness(pClient);
			continue;
		}

		const int64 nNowMs = ptime::MonotonicNowMillis();
		if( backoff.ShouldSkip(nNowMs) )
			continue;

		// 이전 연결의 I/O 완료 통지가 아직 남아 있으면 같은 OVERLAPPED를 재사용할 수 없다.
		if( !pClient->HasNoOutstandingIo() )
			continue;

		// 접속하는 동안(AUTH 응답 대기 포함) PumpCommands()가 같은 연결에 명령을 겹쳐 보내지 못하게 막는다.
		{
			std::lock_guard<std::mutex> lock(_lock);
			if( !_bInitialized )
				break;

			_bResyncing = true;
		}

		// 게이트를 이미 통과해 SendCommand()를 호출 중인 PumpCommands()가 있으면 끝날 때까지 기다린다.
		// 이후의 PumpCommands()는 _bResyncing 때문에 명령을 보내지 않는다.
		{
			std::lock_guard<std::mutex> lock(_sendLock);
		}

		if( pClient->Connect(strIP, nPort, 0, Redis::kConnectTimeoutMs, strUserId, strPassword) )
		{
			LOG_INFO(_T("CRedisSubscriber: reconnected. host(%hs), port(%d)"), strIP.c_str(), static_cast<int32>(nPort));
			backoff.Reset();
			RebuildSubscriptions(); // _bResyncing 해제 + 등록된 구독 전체 재전송
		}
		else
		{
			{
				std::lock_guard<std::mutex> lock(_lock);
				_bResyncing = false;
			}
			backoff.OnFail(ptime::MonotonicNowMillis());
		}
	}
}