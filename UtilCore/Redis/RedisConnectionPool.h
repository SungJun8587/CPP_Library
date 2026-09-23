
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
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <random>

//***************************************************************************
// @brief 여러 개의 CRedisClient 연결 객체를 동시 분배/관리하는 커넥션 풀 클래스
// @details 요청 시 놀고 있는(Free) 커넥션을 대여하여 명령을 전송하고,
//          응답 콜백 시 해당 커넥션을 다시 큐로 자동 반납 처리합니다.
//
//          반납되는 커넥션은 연결 상태를 확인해 살아있으면 Free 큐로,
//          끊어져 있으면 격리 큐(_queueBroken)로 나뉘어 들어갑니다. 전용
//          백그라운드 스레드가 일정 주기로 격리 큐를 훑어 재연결을 시도하고,
//          성공한 커넥션만 다시 Free 큐로 돌려보냅니다 — 죽은 커넥션이
//          Free 큐에 섞여 매번 요청을 실패시키는 일이 없도록 합니다.
//
//          재연결 시도는 클라이언트별로 지수 백오프(+지터)를 적용합니다.
//          연속으로 실패할수록 다음 시도까지의 대기 시간이 늘어나(500ms에서
//          시작해 최대 30초까지), 서버가 오래 죽어있는 동안 스윕 주기마다
//          무의미하게 계속 두드리는 것을 막습니다.
//
//          [수정 — 버그 수정] 예전엔 모든 커넥션이 사용 중일 때(풀이
//          바닥났을 때) SendCommand()가 그냥 false를 돌려주고 그 요청은
//          조용히 사라졌다 — 대부분의 호출부(CChatServerMain의 여러
//          Request*() 등)가 SendCommand()의 반환값을 확인하지 않아서,
//          콜백이 영원히 안 불리는데도 에러 로그 하나 없이 "응답이 그냥
//          안 오는" 것처럼 보이는 버그였다(짧은 시간에 Redis 요청이
//          몰리면 — 예: 여러 방에 거의 동시에 입장 — 재현됨). 이제 풀이
//          바닥나면 요청을 대기 큐(_queuePending)에 넣어두고,
//          PushConnection()이 커넥션을 돌려받는 즉시 대기 큐에서 꺼내
//          그 커넥션에 바로 태워 보낸다 — 요청이 유실되지 않고 순서대로
//          처리된다(다만 그만큼 지연될 수 있음). 대기 큐도 무한정 쌓이면
//          안 되므로 상한(kMaxPendingCommands)을 두고, 넘으면 그때는
//          진짜로 실패(false + 콜백 미호출)로 처리한다.
//
// @code
// // 사용 예시:
// auto pPool = std::make_shared<CRedisConnectionPool>(pIocpCore);
// pPool->Init("127.0.0.1", 6379, /*nDbIndex*/ 0, 10); // 10개 커넥션 생성, 재연결 스레드 자동 시작
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

	//***************************************************************************
	// @brief 지정된 풀 크기만큼 CRedisClient를 생성 및 연결하고, 백그라운드
	//        재연결 스레드를 시작함
	// @details 이미 초기화된 풀에 다시 호출해도 안전하다(재진입 시 내부적으로
	//          먼저 Clear()를 호출해 기존 재연결 스레드/커넥션을 완전히
	//          정리한 뒤 처음부터 다시 채운다).
	// @param strIP 서버 IP
	// @param nPort 서버 포트
	// @param nDbIndex 각 커넥션이 접속 직후 SELECT로 고정할 Redis 논리 DB
	//        인덱스(0이면 기본 DB, SELECT 생략). 재연결된 커넥션에도 동일하게
	//        적용된다
	// @param nPoolSize 생성할 커넥션 개수
	// @param nReconnectIntervalSec 격리 큐를 훑어 재연결을 시도하는 주기(초)
	// @return 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool        Init(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nPoolSize, const int32 nReconnectIntervalSec = 3);

	//***************************************************************************
	// @brief 재연결 스레드를 정지시키고 모든 커넥션을 끊어 풀 리소스를 정리함
	//***************************************************************************
	void        Clear();

	//***************************************************************************
	// @brief Redis 명령을 비동기 실행함.
	// @details [수정] 풀에 놀고 있는 커넥션이 없으면(모두 사용 중) 더 이상
	//          즉시 실패하지 않는다 — 대기 큐에 넣어두고 true를 반환한다
	//          ("요청이 접수됨"이라는 뜻 — 실제 전송은 커넥션이 반납되는
	//          대로 이어짐). 대기 큐가 상한(kMaxPendingCommands)을 넘겼을
	//          때만 진짜로 실패(false, 콜백 미호출)한다.
	// @return 요청이 접수(즉시 전송 또는 대기 큐 등록)됐는지 여부. false면
	//         fnCallback은 절대 호출되지 않는다(대기 큐 포화, 또는 풀
	//         미초기화).
	//***************************************************************************
	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);

private:
	CRedisClientRef		PopConnection();
	void                PushConnection(CRedisClientRef pClient);

	//***************************************************************************
	// @brief [추가] pClient 하나를 골라서 실제로 명령을 실어 보낸다 —
	//        SendCommand()의 즉시 전송 경로와 PushConnection()의 "대기
	//        중이던 요청을 반납받은 커넥션에 바로 태우는" 경로가 공유하는
	//        공통 로직.
	//***************************************************************************
	void                DispatchOnClient(CRedisClientRef pClient, const CVector<std::string>& vecArgs, RedisCallback fnCallback);

	void                StartReconnectLoop();
	void                StopReconnectLoop();
	void                ReconnectLoop();

private:
	CIocpCoreRef                    _iocpCore;               // IOCP 코어 참조
	std::string                     _strIP;                  // 연결 대상 IP
	uint16                          _nPort = 0;              // 연결 대상 포트
	int32                           _nDbIndex = 0;           // 각 커넥션이 접속 직후 SELECT로 고정할 논리 DB 인덱스(0이면 생략)

	std::mutex                      _lock;					  // 풀 동기화 락 (_vecAllClients/_queueFree/_queueBroken/_queuePending 공용)
	CVector<CRedisClientRef>		_vecAllClients;          // 생성된 전체 클라이언트 리스트
	CQueue<CRedisClientRef>			_queueFree;              // 사용 가능한(연결된) 클라이언트 큐
	CQueue<CRedisClientRef>			_queueBroken;            // 연결이 끊겨 재연결을 기다리는 격리 큐

	//***************************************************************************
	// @brief [추가] 풀이 바닥났을 때(모든 커넥션이 사용 중) 대기시켜둘
	//        요청 큐. PushConnection()이 커넥션을 돌려받을 때 이 큐를
	//        먼저 확인해서, 있으면 Free 큐에 넣지 않고 바로 그 커넥션에
	//        태워 보낸다(선입선출로 처리 순서 유지).
	//***************************************************************************
	struct TPendingCommand
	{
		CVector<std::string>	vecArgs;
		RedisCallback			fnCallback;
	};
	static constexpr size_t		kMaxPendingCommands = 2000;	// 이 이상 쌓이면 새 요청은 진짜로 실패 처리
	CQueue<TPendingCommand>			_queuePending;				// 풀이 바닥났을 때 대기 중인 요청들

	std::atomic<bool>               _bInitialized{ false };  // 풀 초기화 여부 플래그

	//***************************************************************************
	// @brief 재연결 전용 스레드 및 동기화 객체
	//***************************************************************************
	int32                           _nReconnectIntervalSec = 3;	// 재연결 스레드가 격리 큐를 훑는 주기 (초 단위)
	std::thread                     _reconnectThread;				// 격리 큐 재연결을 담당하는 전용 스레드
	std::mutex                      _reconnectLock;				// _reconnectCv 대기/신호 전달 전용 뮤텍스
	std::condition_variable         _reconnectCv;					// 재연결 주기 대기 및 즉시 종료 신호 수신용 조건 변수
	std::atomic<bool>               _stopping{ false };			// 재연결 스레드 정지 요청 여부 플래그

	//***************************************************************************
	// @brief 클라이언트별 재연결 지수 백오프 상태
	// @details 격리 큐를 스윕할 때마다 무조건 재연결을 시도하면, 서버가 오래
	//          죽어있는 동안 계속 두드리게 된다(COdbcConnPool과 달리 이
	//          풀에는 별도 헬스체크/워커 스레드가 없고 재연결 스윕 자체가
	//          재시도 시점을 겸하므로, 백오프는 "이번 스윕에서 이 클라이언트를
	//          건너뛸지" 판단하는 형태로 구현한다). 오직 재연결 스레드만
	//          이 맵을 건드리므로(격리 큐에 있는 동안은 다른 누구도 해당
	//          클라이언트를 만지지 않음) 별도 락이 필요 없다.
	//***************************************************************************
	struct TReconnectBackoffState
	{
		int32                                   nFailCount = 0;		// 연속 재연결 실패 횟수
		std::chrono::steady_clock::time_point   nextRetryTime{};	// 이 시각 이전에는 재시도를 건너뜀
	};
	std::unordered_map<CRedisClient*, TReconnectBackoffState> _reconnectBackoffMap;	// 클라이언트 포인터 → 백오프 상태
};

#endif // ndef UC_REDISCONNECTIONPOOL_H