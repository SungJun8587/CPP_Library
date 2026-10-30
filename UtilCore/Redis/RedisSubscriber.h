
//***************************************************************************
// RedisSubscriber.h : interface for the CRedisSubscriber class.
//
//***************************************************************************

#ifndef UC_REDISSUBSCRIBER_H
#define UC_REDISSUBSCRIBER_H

#include <Network/IOCP/IocpCore.h>
#include <Redis/RedisRedefineDataType.h>
#include <Redis/RedisClient.h>
#include <Job/JobQueue.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

// RedisMessageCallback은 RedisProtocol.h에 정의되어 있다(RedisService.h와 이 헤더가 포함 순서와 무관하게 공유).

// 구독 목록에 보관하는 콜백. 메시지마다 std::function을 복사하지 않고 참조만 공유한다.
using RedisMessageCallbackPtr = std::shared_ptr<const RedisMessageCallback>;

//***************************************************************************
// @brief Redis Pub/Sub 구독 전용 연결을 관리하는 클래스
// @details Redis는 구독 상태가 된 연결에서 일반 명령을 받지 않고, 서버가
//          메시지를 자발적으로 밀어 보낸다. 그래서 요청 하나에 응답 하나가
//          대응하는 CRedisConnectionPool의 연결과 섞어 쓸 수 없고, 구독
//          전용 연결을 하나 따로 둔다. 이 클래스가 그 연결(CRedisClient의
//          push 수신 모드)을 하나 소유한다.
//
// @details 스레드 계약
//          - 메시지 콜백은 CRedisService와 같은 방식으로 생성자에 넘긴
//            CJobQueue로 이관되어 호출된다(IOCP 워커 스레드에서 직접 호출되지 않음).
//            같은 연결에서 온 메시지는 수신 순서대로 JobQueue에 쌓인다.
//          - Subscribe()/Unsubscribe()는 어느 스레드에서나 호출할 수 있다.
//          - 이 클래스는 std::make_shared로 만들어야 한다(내부 콜백이
//            weak_ptr(shared_from_this)를 쓴다).
//
// @details 명령 직렬화
//          구독 연결은 한 시점에 미완료 명령을 하나만 가질 수 있는 CRedisClient
//          계약을 따른다. SUBSCRIBE/UNSUBSCRIBE 등은 내부 큐에 쌓였다가 앞 명령의
//          확인 응답이 온 뒤에 하나씩 전송된다. 명령 하나에 채널 하나만 담으므로
//          확인 응답이 명령과 1:1로 대응한다.
//
// @details 재연결과 재구독
//          연결이 끊기면 재연결 스레드가 지수 백오프로 다시 접속하고, 접속에
//          성공하면 등록된 모든 채널·패턴을 다시 구독한다. 구독 목록은 이
//          클래스가 보관하므로 호출자가 다시 등록할 필요가 없다.
//          수신 쪽 TCP 연결이 조용히 죽는 경우(중간 장비가 연결을 버렸는데 RST가 오지
//          않음)를 잡기 위해 재연결 스레드가 유휴 시 주기적으로 PING을 보내고, 응답이
//          kCommandTimeoutMs 안에 오지 않으면 연결을 끊어 재연결 경로로 넘긴다.
//          단절 중에 발행된 메시지는 Redis Pub/Sub 특성상 받을 수 없다(복구되지
//          않는다). 유실되면 안 되는 데이터는 Pub/Sub을 "알림"으로만 쓰고,
//          원본은 DB나 Redis 자료구조에서 다시 읽는 방식으로 설계해야 한다.
//
// @code
//	auto pSub = std::make_shared<CRedisSubscriber>(pIocpCore, pJobQueue);
//	pSub->Init("127.0.0.1", 16379);
//	pSub->Subscribe("Chat:1:2", [](const std::string& channel, const std::string& payload) {
//     // pJobQueue를 처리하는 스레드에서 호출됨
//	});
// @endcode
//***************************************************************************
class CRedisSubscriber : public std::enable_shared_from_this<CRedisSubscriber>
{
public:
	CRedisSubscriber(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue);
	~CRedisSubscriber();

	//***************************************************************************
	// @brief 구독 연결을 만들고 재연결 스레드를 시작함
	// @param strIP Redis 서버 주소
	// @param nPort Redis 서버 포트
	// @param nReconnectIntervalSec 재연결·생존 점검 스레드가 깨어나는 주기(초, 0 이하면 Redis::kSubscriberReconnectIntervalSec)
	// @param strUserId AUTH 사용자(Redis 6 ACL). 비어 있고 strPassword만 있으면 AUTH <password>
	// @param strPassword AUTH 비밀번호. 둘 다 비어 있으면 AUTH를 보내지 않는다
	// @return 성공 여부 (false: 이미 초기화됨, 잘못된 인자)
	// @details 첫 접속이 실패해도 true를 반환하고 재연결 스레드가 백오프로 계속 시도한다.
	//          서버 기동 순서와 무관하게 Subscribe()를 미리 등록해 둘 수 있고, 접속이
	//          되는 순간 등록된 구독이 모두 서버에 반영된다. 접속 여부는 IsConnected()로 본다.
	// @details Pub/Sub은 논리 DB(SELECT)와 무관하게 서버 전체에서 공유되므로
	//          DB 인덱스를 받지 않는다.
	//***************************************************************************
	bool Init(const std::string& strIP, const uint16 nPort, const int32 nReconnectIntervalSec = Redis::kSubscriberReconnectIntervalSec, const std::string& strUserId = std::string(), const std::string& strPassword = std::string());

	//***************************************************************************
	// @brief 재연결 스레드를 멈추고 연결을 닫음. 소멸자에서도 호출된다.
	// @details 호출 후에는 Init()으로 다시 시작할 수 있다. 등록된 구독 목록은 비워진다.
	//***************************************************************************
	void Shutdown();

	//***************************************************************************
	// @brief 채널을 구독하고 메시지 콜백을 등록함
	// @param strChannel 구독할 채널 이름(비어 있으면 실패)
	// @param fnCallback 메시지가 올 때마다 호출될 콜백(null이면 실패)
	// @return 등록 성공 여부 (false: 초기화 전, 빈 채널, null 콜백)
	// @details 같은 채널을 다시 구독하면 콜백이 교체된다. 연결이 끊긴 상태에서
	//          호출해도 true를 반환하며, 구독 목록에 기록되어 재연결 후 서버에
	//          반영된다. true는 "등록됨"이지 "서버가 확인함"이 아니다.
	//***************************************************************************
	bool Subscribe(const std::string& strChannel, RedisMessageCallback fnCallback);

	//***************************************************************************
	// @brief 채널 구독을 해제함
	// @return 해제할 구독이 있었는지 여부 (false: 등록되지 않은 채널)
	//***************************************************************************
	bool Unsubscribe(const std::string& strChannel);

	//***************************************************************************
	// @brief 글롭 패턴(예: "Chat:*")으로 채널을 구독하고 메시지 콜백을 등록함
	// @details 콜백의 strChannel에는 패턴이 아니라 메시지가 발행된 실제 채널 이름이 온다.
	//          그 외 규칙은 Subscribe()와 같다.
	//***************************************************************************
	bool PSubscribe(const std::string& strPattern, RedisMessageCallback fnCallback);

	//***************************************************************************
	// @brief 패턴 구독을 해제함
	// @return 해제할 구독이 있었는지 여부
	//***************************************************************************
	bool PUnsubscribe(const std::string& strPattern);

	//***************************************************************************
	// @brief 현재 구독 연결이 살아 있는지 확인함
	//***************************************************************************
	bool IsConnected() const;

private:
	void OnPush(RedisValue&& value);
	void OnCommandReply(const uint64 nEpoch, const RedisValue& value);
	void PumpCommands();
	void RebuildSubscriptions();
	void CheckLiveness(const CRedisClientRef& pClient);
	void ReconnectLoop();

private:
	CIocpCoreRef				_iocpCore;
	CJobQueueRef				_jobQueue;			// 메시지 콜백을 이관할 JobQueue

	mutable std::mutex			_lock;				// 아래 상태 전체를 보호(클라이언트 호출은 락 밖에서)
	CRedisClientRef				_pClient;			// 구독 전용 연결(Init()에서 생성)
	bool						_bInitialized = false;
	uint64						_nEpoch = 0;		// 연결(재연결)마다 증가 — 이전 연결의 지연된 확인 응답을 무시하기 위함
	std::unordered_map<std::string, RedisMessageCallbackPtr>	_channelMap;	// 구독 중인 채널 → 콜백
	std::unordered_map<std::string, RedisMessageCallbackPtr>	_patternMap;	// 구독 중인 패턴 → 콜백
	std::deque<CVector<std::string>>	_cmdQueue;	// 전송 대기 중인 구독 명령
	bool						_bCmdInFlight = false;	// 확인 응답을 기다리는 명령이 있는지
	std::mutex					_sendLock;			// PumpCommands()의 "게이트 확인 → SendCommand 호출" 구간 직렬화(재동기화 시작이 이 구간이 끝나길 기다린다)
	bool						_bPumping = false;		// PumpCommands()가 한 스레드에서 실행 중인지(재진입 방지)

	// 재연결 스레드가 Connect()를 시작해서 RebuildSubscriptions()가 끝날 때까지 true. 이 동안 PumpCommands()는
	// 명령을 보내지 않는다 — 접속 직후의 AUTH 응답 대기나 재구독 명령과 같은 연결에 명령이 겹치면
	// "미완료 명령 하나" 계약 위반으로 구독 명령이 유실되기 때문이다. 쌓인 명령은 재구독이 대신한다.
	bool						_bResyncing = false;

	int64						_nCmdSentMs = 0;	// 진행 중인 명령을 보낸 시각(ptime::MonotonicNowMillis, 응답 무응답 감지용)
	int64						_nLastPingMs = 0;	// 마지막으로 PING을 큐에 넣은 시각(ptime::MonotonicNowMillis)

	std::string					_strIP;
	uint16						_nPort = 0;
	std::string					_strUserId;
	std::string					_strPassword;
	int32						_nReconnectIntervalSec = Redis::kSubscriberReconnectIntervalSec;

	std::thread					_reconnectThread;
	std::mutex					_reconnectLock;		// _reconnectCv 대기 전용
	std::condition_variable		_reconnectCv;
	std::atomic<bool>			_stopping{ false };
};

#endif // ndef UC_REDISSUBSCRIBER_H