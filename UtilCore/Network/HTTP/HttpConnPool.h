
//***************************************************************************
// HttpConnPool.h : interface for the CHttpConnPoolT class template.
//
//***************************************************************************

#ifndef __HTTPCONNPOOL_H__
#define __HTTPCONNPOOL_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__DELAYEDTASKQUEUE_H__
#include <Containers/Queue/DelayedTaskQueue.h>
#endif

#include <deque>
#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>

//***************************************************************************
// @class CHttpConnPoolT
// @brief host 하나에 대한 HTTP 커넥션 풀 (IOCP/RIO 엔진 비의존)
//
// @details
//      IOCP/RIO 두 엔진에서 재사용하기 위해 세션/서비스 타입을 템플릿
//      파라미터로 받는다(TSession, TSessionRef, TService, TServiceRef) —
//      타입만 맞으면 동작하는 덕타이핑이라, 실제 CIocpSession/CRioSession
//      없이도 이 파일 하나만으로 로직을 단위 테스트할 수 있다(테스트는 이
//      파일과 함께 제공).
//
//      [엔진 비대칭 — 지금은 해소됨]
//      IOCP/RIO 둘 다 이제 ConnectOneMoreSession()이 "게시만 하고" 즉시
//      리턴하고, 실제 연결 완료/실패는 세션의 OnConnected()/OnDisconnected()
//      훅으로 항상 비동기 통지된다(RIO도 CRioConnectDispatcher 도입 이후
//      IOCP와 동일한 계약 — RioSession.h/RioService.h 변경 이력 참고). 그래서
//      이 풀은 두 엔진을 완전히 동일한 코드 경로로 다룬다 —
//      ConnectOneMoreSession()의 반환값은 "게시 시도" 성공 여부 로깅용으로만
//      참고하고, 실제 유휴 목록 등록은 항상 OnSessionConnStateChanged
//      (connected=true) 콜백에서만 수행한다.
//
//      [재연결 정책]
//      세션이 끊기면(OnSessionConnStateChanged(false)) ScheduleReconnect()가
//      CDelayedTaskQueue에 재연결 작업을 예약한다. 지수 백오프(200ms 시작,
//      2배씩 증가, 10초 상한)를 적용하고, 현재 세션 수(TService::
//      GetCurrentSessionCount())가 _maxConnections 이상이면 재연결 자체를
//      시도하지 않는다. 연결이 성공하면(OnSessionConnStateChanged(true))
//      백오프 카운터를 리셋한다. CDelayedTaskQueue::ProcessExpiredTasks()는
//      이 풀이 소유하는 전용 스레드(_taskThread) 하나가 돌린다 — Start()에서
//      시작, Close()/소멸자에서 정지+join.
//
//      [스레드 안전성/수명 메모]
//      _taskThread와 예약된 재연결 작업의 콜백은 모두 "this"를 raw 포인터로
//      캡처한다(shared_from_this()로 self를 캡처하지 않음) — 이 스레드는
//      반드시 소멸자에서 Stop()+join()으로 정리된 뒤에야 나머지 멤버가
//      파괴되도록 보장하므로, self shared_ptr을 캡처하면 오히려 "스레드가
//      스스로를 살려두는" 자기 참조 사이클(Close()를 아무도 안 부르면 영원히
//      안 죽는 leak)이 생긴다. raw this + 소멸자에서 확실한 join이 더 안전한
//      선택이다.
//
//      [아직 반영 안 된 것] min idle 유지: 시작 시 minIdle개를 미리 채우는
//      것은 TService::Start()의 maxSessionCount 설정에 위임(호출부 책임) —
//      이 클래스는 "끊긴 만큼만" 보충한다.
//***************************************************************************
template<typename TSession, typename TSessionRef, typename TService, typename TServiceRef>
class CHttpConnPoolT
	: public IHttpConnPool
	, public std::enable_shared_from_this<CHttpConnPoolT<TSession, TSessionRef, TService, TServiceRef>>
{
	using ThisPool = CHttpConnPoolT<TSession, TSessionRef, TService, TServiceRef>;

public:
	// 재연결 백오프 기준값 — 테스트에서 타이밍 검증에 사용하므로 public.
	static constexpr int32 kBackoffBaseDelayMs = 200; // 최초 재연결 지연(ms)
	static constexpr int32 kBackoffMaxDelayMs = 10000; // 재연결 지연 상한(ms)

	//***************************************************************************
	// @brief 풀을 생성합니다. 세션 팩토리가 이 풀 자신(weak_ptr)을 캡처해야 하는
	//        순환 구조 때문에 생성자 직접 호출 대신 이 정적 팩토리를 사용합니다.
	// @param maxConnections host당 유지할 최대 커넥션 수
	// @param makeService (sessionFactory, maxSessionCount) -> TServiceRef 형태의
	//        콜백. 실제 CIocpClientService/CRioClientService 생성자 호출을
	//        캡슐화해서 넘겨준다(엔진별 생성자 시그니처 차이를 이 템플릿이
	//        몰라도 되게 하기 위함) — 실제 사용 예는 HttpConnPoolFactory.h 참고.
	// @return std::shared_ptr<ThisPool> 생성된 풀 (Start() 호출 전까지는 미구동 상태)
	//***************************************************************************
	template<typename ServiceMakerFn>
	static std::shared_ptr<ThisPool> Create(int32 maxConnections, ServiceMakerFn&& makeService)
	{
		std::shared_ptr<ThisPool> pool(new ThisPool(maxConnections));

		std::weak_ptr<ThisPool> weakPool = pool;
		SessionFactory sessionFactory = [weakPool]() -> CSessionRef
			{
				auto session = std::make_shared<TSession>();
				session->SetConnStateHandler([weakPool](CSessionRef s, bool connected)
					{
						if( auto p = weakPool.lock() )
							p->OnSessionConnStateChanged(s, connected);
					});
				return session;
			};

		pool->_clientService = makeService(sessionFactory, maxConnections);
		return pool;
	}

	//***************************************************************************
	// @brief 소멸자. Close()를 호출하지 않고 소멸되는 경우에 대비한 방어적 정리.
	// @details self shared_ptr을 잡지 않는 raw-this 캡처 설계라 여기서 안전하게 정지 가능.
	//***************************************************************************
	~CHttpConnPoolT() override
	{
		_delayedTaskQueue.Stop();
		if( _taskThread.joinable() )
			_taskThread.join();
	}

	//***************************************************************************
	// @brief 풀을 구동합니다: 클라이언트 서비스 시작 + 재연결 작업 큐 전용 스레드 시작.
	// @return bool 성공 여부 (_clientService가 없거나 그 Start()가 실패하거나,
	//         _taskThread 생성 자체가 실패하면 false)
	//***************************************************************************
	bool Start() override
	{
		if( !_clientService )
			return false;
		if( !_clientService->Start() )
			return false;

		try
		{
			_taskThread = std::thread([this]() { _delayedTaskQueue.ProcessExpiredTasks(); });
		}
		catch( ... )
		{
			return false;
		}

		return true;
	}

	//***************************************************************************
	// @brief 풀을 정지합니다: 재연결 작업 큐 정지+join 후 클라이언트 서비스를 닫습니다.
	//***************************************************************************
	void Close() override
	{
		_delayedTaskQueue.Stop();
		if( _taskThread.joinable() )
			_taskThread.join();

		if( _clientService )
			_clientService->Close();
	}

	//***************************************************************************
	// @brief 완성된 HTTP 요청 패킷을 이 풀이 관리하는 host로 비동기 전송합니다.
	// @param data 요청 패킷 바이트
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @details 즉시 보낼 수 있는 유휴 세션이 있으면 바로 디스패치하고, 없으면
	//          내부 대기열(_pendingRequests)에 쌓아둔다. data는 이 호출 안에서
	//          std::string으로 복사되므로(제로카피 빌더의 "Build() 호출
	//          시점까지만 유효" 전제가 큐잉 상황에서는 성립하지 않기 때문),
	//          호출부가 반환 후 즉시 원본 버퍼를 해제해도 안전하다.
	//***************************************************************************
	void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete) override
	{
		PendingRequest req{ std::string(data, len), std::move(onComplete) };

		TSessionRef idleSession;
		{
			std::lock_guard<std::mutex> guard(_lock);
			if( !_idleSessions.empty() )
			{
				idleSession = _idleSessions.back();
				_idleSessions.pop_back();
			}
			else
			{
				_pendingRequests.push_back(std::move(req));
				return;
			}
		}

		DispatchToSession(idleSession, std::move(req));
	}

	//***************************************************************************
	// @brief 세션의 연결 상태 변화를 통지받습니다.
	// @param sessionBase 상태가 변한 세션 (베이스 CSessionRef로 넘어옴)
	// @param connected true면 방금 연결 완료, false면 끊김(연결 실패 포함)
	// @details 실제 배포 코드에서는 SetConnStateHandler로 등록된 람다를 통해서만
	//          호출되지만, 단위 테스트에서 직접 연결/해제 이벤트를 시뮬레이션할
	//          수 있도록 public으로 둔다(DisconnectHandler와 동일한 패턴).
	//          connected==false면 ScheduleReconnect()로 재연결을 예약하고,
	//          true면 백오프 카운터를 리셋한 뒤 DispatchOrIdle()로 넘긴다.
	//***************************************************************************
	void OnSessionConnStateChanged(CSessionRef sessionBase, bool connected)
	{
		TSessionRef session = std::static_pointer_cast<TSession>(sessionBase);

		if( !connected )
		{
			ScheduleReconnect();
			return;
		}

		_consecutiveFailCount.store(0, std::memory_order_relaxed); // 연결 성공 -> 백오프 리셋
		DispatchOrIdle(session);
	}

	//***************************************************************************
	// @brief 실패 횟수에 따른 재연결 지연 시간(ms)을 계산합니다.
	// @param failCount 연속 실패 횟수 (0=최초 재연결)
	// @return int32 지연 시간(ms). kBackoffBaseDelayMs에서 시작해 2배씩 증가,
	//         kBackoffMaxDelayMs에서 상한(오버플로 방지를 위해 shift를 6으로
	//         clamp — 200<<6=12800이 이미 상한을 넘으므로 그 이상은 항상
	//         상한과 동일).
	// @details 순수 함수라 타이머/스레드 없이 단위 테스트 가능하도록 public
	//          static으로 노출한다.
	//***************************************************************************
	static int32 ComputeBackoffDelay(uint32_t failCount) noexcept
	{
		uint32_t shift = (std::min<uint32_t>)(failCount, 6);
		int64_t delay = static_cast<int64_t>(kBackoffBaseDelayMs) << shift;
		if( delay > kBackoffMaxDelayMs )
			delay = kBackoffMaxDelayMs;
		return static_cast<int32>(delay);
	}

private:
	//***************************************************************************
	// @struct PendingRequest
	// @brief 유휴 세션이 없을 때 대기열에 쌓이는 요청 하나를 표현합니다.
	//***************************************************************************
	struct PendingRequest
	{
		std::string data;                        // 전송할 요청 패킷 바이트 (SendRequest()에서 복사됨)
		HttpRequestCompletionHandler onComplete;  // 응답 완결 시 호출할 사용자 콜백
	};

	//***************************************************************************
	// @brief CHttpConnPoolT 생성자 (private — Create() 정적 팩토리를 통해서만 생성 가능).
	// @param maxConnections host당 유지할 최대 커넥션 수
	//***************************************************************************
	explicit CHttpConnPoolT(int32 maxConnections)
		: _maxConnections(maxConnections)
	{
	}

	//***************************************************************************
	// @brief 특정 세션에 대기 중이던(또는 방금 들어온) 요청을 실제로 전송합니다.
	// @param session 요청을 보낼 세션
	// @param req 전송할 요청 (data/onComplete)
	// @details session->SendRequest()가 false를 반환하면(세션이 이미 죽었거나
	//          재진입 등으로 Send 자체가 실패한 경우) 즉시 실패 콜백을 호출하고
	//          세션을 폐기한다. 성공하면 세션의 완료 콜백 안에서 사용자 콜백을
	//          먼저 부른 뒤 OnRequestComplete()로 이어서 세션을 반납/폐기한다.
	//***************************************************************************
	void DispatchToSession(TSessionRef session, PendingRequest&& req)
	{
		std::string dataOwned = std::move(req.data);
		HttpRequestCompletionHandler userCb = std::move(req.onComplete);

		auto self = this->shared_from_this();
		TSessionRef sessionCapture = session;

		bool began = session->SendRequest(dataOwned.data(), dataOwned.size(),
			[self, sessionCapture, userCb](bool success, CHttpResponseParser& parser)
			{
				if( userCb )
					userCb(success, parser);
				self->OnRequestComplete(sessionCapture, success);
			});

		if( !began )
		{
			// 세션이 이미 죽었거나(재진입 등) Send 자체가 실패 — 즉시 실패 통지.
			if( userCb )
			{
				CHttpResponseParser dummy;
				userCb(false, dummy);
			}
			DiscardSession(session);
		}
	}

	//***************************************************************************
	// @brief 세션의 요청 하나가 완료됐을 때(성공/실패 불문) 호출됩니다.
	// @param session 요청을 처리했던 세션
	// @param success 응답이 정상적으로 완결됐는지 여부
	// @details 실패했거나 서버가 "Connection: close"를 명시했으면 세션을
	//          재사용하지 않고 DiscardSession()으로 폐기한다. 그 외에는
	//          DispatchOrIdle()로 넘겨 다음 요청 처리 또는 유휴 반납을 맡긴다.
	//***************************************************************************
	void OnRequestComplete(TSessionRef session, bool success)
	{
		// Connection: close 응답을 받았거나 실패했으면 재사용하지 않고 폐기.
		if( !success || session->IsConnectionCloseRequested() )
		{
			DiscardSession(session);
			return;
		}

		DispatchOrIdle(session);
	}

	//***************************************************************************
	// @brief 세션 하나가 "지금 막 요청을 받을 수 있는 상태"가 됐을 때(연결 직후
	//        또는 이전 요청 완료 직후) 공통으로 타는 경로.
	// @param session 요청을 받을 수 있는 상태가 된 세션
	// @details 대기 중인 요청이 있으면 바로 넘기고, 없으면 유휴 목록에 반납한다.
	//***************************************************************************
	void DispatchOrIdle(TSessionRef session)
	{
		PendingRequest next;
		bool hasNext = false;
		{
			std::lock_guard<std::mutex> guard(_lock);
			if( !_pendingRequests.empty() )
			{
				next = std::move(_pendingRequests.front());
				_pendingRequests.pop_front();
				hasNext = true;
			}
			else
			{
				_idleSessions.push_back(session);
			}
		}

		if( hasNext )
			DispatchToSession(session, std::move(next));
	}

	//***************************************************************************
	// @brief 세션을 폐기하고 재연결을 예약합니다.
	// @param session 폐기할 세션
	// @details Disconnect()가 비동기로 OnDisconnected()를 유발해
	//          ScheduleReconnect()가 다시 불릴 수 있음 — ScheduleReconnect()
	//          자체가 상한 체크로 중복 호출에도 안전하게 설계돼 있으므로
	//          (currentSessionCount >= max면 그냥 무시) 여기서도 호출해 두는
	//          것이 안전하다(세션이 이미 Disconnected라 콜백이 다시 안 오는
	//          경로까지 커버).
	//***************************************************************************
	void DiscardSession(TSessionRef session)
	{
		session->Disconnect(_T("HttpConnPool discard"));
		ScheduleReconnect();
	}

	//***************************************************************************
	// @brief 재연결 작업을 지수 백오프 지연을 걸어 CDelayedTaskQueue에 예약합니다.
	// @details 이미 상한만큼 세션을 보유(또는 연결 시도 중)라면 재연결하지
	//          않는다. GetCurrentSessionCount()는 "연결 완료된" 세션뿐 아니라
	//          "연결 시도 중"(AddSession()이 ConnectAsync() 게시 이전에 먼저
	//          호출됨 — IocpService/RioService의 ConnectOneMoreSession() 설계
	//          참고)인 세션도 포함하므로, 상한을 초과해 동시에 여러 연결을
	//          시도하는 상황을 자연스럽게 막아준다.
	//***************************************************************************
	void ScheduleReconnect()
	{
		if( !_clientService )
			return;

		if( _clientService->GetCurrentSessionCount() >= _maxConnections )
			return;

		uint32_t failCount = _consecutiveFailCount.fetch_add(1, std::memory_order_relaxed);
		int32 delayMs = ComputeBackoffDelay(failCount);

		_delayedTaskQueue.Reserve(delayMs, [this]()
			{
				if( _clientService )
					_clientService->ConnectOneMoreSession();
			});
	}

private:
	TServiceRef _clientService; // 이 풀이 소유하는 IOCP/RIO 클라이언트 서비스
	int32 _maxConnections;      // host당 유지할 최대 커넥션 수

	std::mutex _lock;                            // _idleSessions/_pendingRequests 보호
	std::vector<TSessionRef> _idleSessions;      // 요청을 받을 수 있는 유휴 세션 목록
	std::deque<PendingRequest> _pendingRequests; // 유휴 세션이 없을 때 대기 중인 요청 큐

	std::atomic<uint32_t> _consecutiveFailCount{ 0 }; // 지수 백오프용 연속 연결 실패 횟수
	CDelayedTaskQueue _delayedTaskQueue;              // 재연결 작업 예약 큐
	std::thread _taskThread;                          // _delayedTaskQueue.ProcessExpiredTasks() 전용 스레드
};

#endif // ndef __HTTPCONNPOOL_H__