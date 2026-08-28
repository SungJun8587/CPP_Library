
//***************************************************************************
// HttpConnPool.h : interface for the CHttpConnPoolT class template.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOL_H
#define UC_HTTPCONNPOOL_H

#include <Network/HTTP/HttpConnPoolCommon.h>
#include <Containers/Queue/DelayedTaskQueue.h>

#include <deque>
#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>
#include <functional>

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
//      [_minIdle vs _maxConnections — 책임 분리]
//      이 풀은 커넥션 수 관리를 두 가지 독립된 메커니즘으로 나눈다:
//      - ScheduleReconnect() (백오프 경로): 세션이 끊겼을 때(OnSessionConnStateChanged
//        (false)) 호출된다. 목적은 "기준선(_minIdle) 유지"다 — 현재 세션 수가
//        _minIdle 미만일 때만 지수 백오프(200ms 시작, 2배씩 증가, 10초 상한)를
//        걸어 CDelayedTaskQueue에 재연결을 예약한다. 이미 _minIdle을 채우고
//        있으면(끊긴 게 여분의 버스트 커넥션이었다면) 여기서는 더 만들지 않는다.
//        디스커넥트는 원격 서버의 일시적 장애를 의미할 수 있어 백오프로 과도한
//        재시도를 피한다.
//      - TryGrow() (즉시 경로): SendRequest()가 유휴 세션을 못 찾아 요청을
//        대기열에 쌓아야 할 때 호출된다. 이건 장애 복구가 아니라 "지금 수요가
//        기준선을 넘었다"는 정상적인 신호이므로, 백오프 없이 즉시
//        ConnectOneMoreSession()을 시도하되 _maxConnections 상한까지만 늘린다.
//      Start()는 초기 연결을 _maxConnections가 아니라 _minIdle개만 게시한다 —
//      나머지(_maxConnections까지의 여유분)는 실제로 버스트 수요가 생겼을 때
//      TryGrow()가 채운다.
//
//      [증설 시도의 자연스러운 상한 도달] ConnectOneMoreSession()은 세션을
//      생성하자마자(실제 연결 완료 전에) AddSession()으로 즉시 등록하므로
//      GetCurrentSessionCount()가 그 자리에서 바로 반영된다. 그래서
//      ScheduleReconnect()/TryGrow() 양쪽 다 "카운트 확인 -> 필요하면 1개
//      요청"만 하면, 짧은 시간에 여러 번 호출돼도(예: 요청 폭주로 TryGrow()가
//      연달아 불려도) 상한에 도달하는 순간 자동으로 더 이상 늘지 않는다 —
//      별도의 동시성 카운터 관리가 필요 없다.
//
//      [스레드 안전성/수명 메모]
//      _taskThread와 예약된 재연결 작업의 콜백은 모두 "this"를 raw 포인터로
//      캡처한다(shared_from_this()로 self를 캡처하지 않음) — 이 스레드는
//      반드시 소멸자에서 Stop()+join()으로 정리된 뒤에야 나머지 멤버가
//      파괴되도록 보장하므로, self shared_ptr을 캡처하면 오히려 "스레드가
//      스스로를 살려두는" 자기 참조 사이클(Close()를 아무도 안 부르면 영원히
//      안 죽는 leak)이 생긴다. raw this + 소멸자에서 확실한 join이 더 안전한
//      선택이다.
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
	// @param minIdle host당 항상 유지할 기준 커넥션 수. Start()가 이 개수만큼
	//        초기 연결을 게시하며, 디스커넥트로 이 아래로 떨어지면
	//        ScheduleReconnect()가 백오프를 걸어 다시 채운다.
	// @param maxConnections host당 허용할 최대 커넥션 수. minIdle을 넘는
	//        추가분은 TryGrow()가 실제 수요(대기열 발생)에 반응해서만 만든다.
	// @param makeService (sessionFactory, initialSessionCount) -> TServiceRef
	//        형태의 콜백. 실제 CIocpClientService/CRioClientService 생성자
	//        호출을 캡슐화해서 넘겨준다(엔진별 생성자 시그니처 차이를 이
	//        템플릿이 몰라도 되게 하기 위함). initialSessionCount에는 minIdle이
	//        전달된다 — 실제 사용 예는 HttpConnPoolFactory.h 참고.
	// @param initSession 세션이 새로 생성될 때마다(SetConnStateHandler 등록보다
	//        먼저) 호출되는 선택적 훅. HTTPS 세션처럼 생성 직후 추가 설정
	//        (SSL_CTX/SNI 호스트네임 주입 등)이 필요한 경우에 쓴다. 평문 HTTP는
	//        생략(nullptr)하면 됨 — 기존 호출부는 그대로 동작한다.
	// @return std::shared_ptr<ThisPool> 생성된 풀 (Start() 호출 전까지는 미구동 상태)
	//***************************************************************************
	template<typename ServiceMakerFn>
	static std::shared_ptr<ThisPool> Create(int32 minIdle, int32 maxConnections, ServiceMakerFn&& makeService,
		std::function<void(TSessionRef)> initSession = nullptr)
	{
		std::shared_ptr<ThisPool> pool(new ThisPool(minIdle, maxConnections));

		std::weak_ptr<ThisPool> weakPool = pool;
		SessionFactory sessionFactory = [weakPool, initSession]() -> CSessionRef
			{
				auto session = std::make_shared<TSession>();
				if( initSession )
					initSession(session);
				session->SetConnStateHandler([weakPool](CSessionRef s, bool connected)
					{
						if( auto p = weakPool.lock() )
							p->OnSessionConnStateChanged(s, connected);
					});
				return session;
			};

		pool->_clientService = makeService(sessionFactory, minIdle);
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
	// @brief 풀을 구동합니다: 클라이언트 서비스 시작(_minIdle개 초기 연결 게시) +
	//        재연결 작업 큐 전용 스레드 시작.
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
	// @brief 이 풀이 현재 붙들고 있는 세션 수를 반환합니다.
	// @return size_t _clientService->GetCurrentSessionCount()를 그대로 전달.
	//         _clientService가 없으면(생성 실패 등) 0.
	//***************************************************************************
	size_t GetActiveSessionCount() override
	{
		if( !_clientService )
			return 0;
		return static_cast<size_t>(_clientService->GetCurrentSessionCount());
	}

	//***************************************************************************
	// @brief 활성 세션 수가 변할 때마다 호출될 콜백을 등록합니다.
	// @details CHttpConnPoolManager가 폴링 없이 "세션 수가 0이 됐다"를 알아채기
	//          위해 쓴다 — IHttpConnPool::SetSessionCountChangedHandler() 참고.
	//***************************************************************************
	void SetSessionCountChangedHandler(std::function<void(size_t)> handler) override
	{
		_sessionCountChangedHandler = std::move(handler);
	}

	//***************************************************************************
	// @brief 완성된 HTTP 요청 패킷을 이 풀이 관리하는 host로 비동기 전송합니다.
	// @param data 요청 패킷 바이트
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @details 즉시 보낼 수 있는 유휴 세션이 있으면 바로 디스패치하고, 없으면
	//          내부 대기열(_pendingRequests)에 쌓아둔 뒤 TryGrow()로 즉시(백오프
	//          없이) 증설을 시도한다(_maxConnections 상한까지만). data는 이
	//          호출 안에서 std::string으로 복사되므로(제로카피 빌더의 "Build()
	//          호출 시점까지만 유효" 전제가 큐잉 상황에서는 성립하지 않기
	//          때문), 호출부가 반환 후 즉시 원본 버퍼를 해제해도 안전하다.
	//***************************************************************************
	void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete) override
	{
		PendingRequest req{ std::string(data, len), std::move(onComplete) };

		TSessionRef idleSession;
		bool queued = false;
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
				queued = true;
			}
		}

		if( queued )
		{
			// 유휴 세션이 없어 대기열로 갔다 = 지금 수요가 기준선(_minIdle)을
			// 넘어섰을 수 있다는 신호 — 락 밖에서(ConnectOneMoreSession() 호출이
			// 걸릴 수 있으므로) 즉시 증설을 시도한다.
			TryGrow();
			return;
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
	//          connected==false면 ScheduleReconnect()로 기준선(_minIdle) 유지를
	//          시도하고, true면 백오프 카운터를 리셋한 뒤 DispatchOrIdle()로 넘긴다.
	//
	//          [_sessionCountChangedHandler에 _notifiedSessionCount를 쓰는 이유]
	//          처음에는 여기서 GetActiveSessionCount()(=_clientService->
	//          GetCurrentSessionCount(), 즉 하위 CNetService::_sessions.size()를
	//          실시간 조회)를 그대로 넘겼는데, 실제로 돌려보니 CNetService의
	//          disconnect 처리 순서가 "OnDisconnected() 훅을 먼저 호출하고,
	//          그 세션을 _sessions에서 실제로 빼는 건 그 다음"이라, 마지막
	//          세션이 끊길 때 우리가 읽는 값이 항상 실제보다 1 많게(예: 세션
	//          3개면 통지값이 3,2,1로만 오고 0은 절대 안 옴) 나오는 레이스가
	//          있었다. CNetService::_sessions를 우리가 직접 건드릴 수 없으니,
	//          _sessions.size()에 의존하는 대신 "연결 성공/해제 통지를 우리가
	//          직접 받은 횟수"만으로 순수하게 세는 별도 카운터
	//          (_notifiedSessionCount)를 둬서 이 레이스 자체를 회피한다 —
	//          OnConnected()/OnDisconnected() 쌍은 이 프로젝트 전반에서 세션당
	//          정확히 1:1로 호출되는 게 기존 불변식이므로, 이 카운터는 하위
	//          컨테이너의 정리 타이밍과 무관하게 항상 정확하다.
	//***************************************************************************
	void OnSessionConnStateChanged(CSessionRef sessionBase, bool connected)
	{
		TSessionRef session = std::static_pointer_cast<TSession>(sessionBase);

		int64_t notifiedCount;
		if( !connected )
		{
			notifiedCount = _notifiedSessionCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
			ScheduleReconnect();
		}
		else
		{
			notifiedCount = _notifiedSessionCount.fetch_add(1, std::memory_order_acq_rel) + 1;
			_consecutiveFailCount.store(0, std::memory_order_relaxed); // 연결 성공 -> 백오프 리셋
			DispatchOrIdle(session);
		}

		if( _sessionCountChangedHandler )
			_sessionCountChangedHandler(static_cast<size_t>((std::max)(notifiedCount, static_cast<int64_t>(0))));
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
	// @param minIdle host당 항상 유지할 기준 커넥션 수
	// @param maxConnections host당 허용할 최대 커넥션 수
	//***************************************************************************
	explicit CHttpConnPoolT(int32 minIdle, int32 maxConnections)
		: _minIdle(minIdle), _maxConnections(maxConnections)
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
	//
	//          [순환 참조 방지] 이 완료 콜백은 session->SendRequest() 내부에서
	//          CHttpClientCore::m_onComplete로 저장된다 — 즉 콜백이 "그 세션
	//          자신의 멤버 안"에 들어간다. 예전에는 이 콜백이 TSessionRef(강한
	//          shared_ptr)를 캡처했는데, 그러면 "세션이 자기 자신을 가리키는
	//          shared_ptr을 자기 멤버 안에 들고 있는" 자기 참조 순환이 생겨서,
	//          응답이 끝까지 안 오는 경우(연결 중간에 끊김, TLS 핸드셰이크 실패
	//          등 FeedRecv()가 끝내 m_onComplete를 비우지 못하는 모든 경로)
	//          세션이 영원히 해제되지 않는 메모리 릭이 됐다. 지금은 원시 포인터
	//          (sessionRaw)만 캡처한다 — 이 콜백은 세션 자신이 자기 멤버 안에서
	//          호출해주는 것이라 불리는 시점엔 세션이 반드시 살아있음이 보장되고
	//          (자기가 자기를 부르는 구조라 댕글링 가능성 자체가 없음), 세션의
	//          실제 수명은 어차피 CNetService::_sessions(서비스가 소유)가 별도로
	//          보장하므로 이 콜백이 강한 참조를 들고 있을 필요가 애초에 없었다.
	//          TSessionRef가 진짜로 필요한 시점(OnRequestComplete()가 유휴
	//          목록에 반납/폐기 판단할 때)에만 shared_from_this()로 다시 만든다.
	//***************************************************************************
	void DispatchToSession(TSessionRef session, PendingRequest&& req)
	{
		std::string dataOwned = std::move(req.data);
		HttpRequestCompletionHandler userCb = std::move(req.onComplete);

		auto self = this->shared_from_this();
		TSession* sessionRaw = session.get(); // 강한 참조 대신 원시 포인터만 캡처 (위 설명 참고)

		bool began = session->SendRequest(dataOwned.data(), dataOwned.size(),
			[self, sessionRaw, userCb](bool success, CHttpResponseParser& parser)
			{
				if( userCb )
					userCb(success, parser);
				// 이 시점에 sessionRaw는 반드시 유효하다(자기 자신이 자기 콜백을
				// 호출하는 구조). TSessionRef가 필요한 곳(OnRequestComplete)에만
				// 여기서 다시 만들어 넘긴다 — 콜백 자체는 강한 참조를 들고 있지 않음.
				TSessionRef sessionRef = std::static_pointer_cast<TSession>(sessionRaw->shared_from_this());
				self->OnRequestComplete(sessionRef, success);
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
	// @brief 세션을 폐기하고, 기준선(_minIdle) 유지를 위한 재연결을 예약합니다.
	// @param session 폐기할 세션
	// @details Disconnect()가 비동기로 OnDisconnected()를 유발해
	//          ScheduleReconnect()가 다시 불릴 수 있음 — ScheduleReconnect()
	//          자체가 _minIdle 체크로 중복 호출에도 안전하게 설계돼 있으므로
	//          (currentSessionCount >= minIdle이면 그냥 무시) 여기서도 호출해
	//          두는 것이 안전하다(세션이 이미 Disconnected라 콜백이 다시 안
	//          오는 경로까지 커버).
	//***************************************************************************
	void DiscardSession(TSessionRef session)
	{
		session->Disconnect(_T("HttpConnPool discard"));
		ScheduleReconnect();
	}

	//***************************************************************************
	// @brief 재연결 작업을 지수 백오프 지연을 걸어 CDelayedTaskQueue에 예약합니다.
	// @details "기준선(_minIdle) 유지"가 목적이다 — 현재 세션 수가 이미 _minIdle
	//          이상이면(끊긴 게 여분의 버스트 커넥션이었다면) 여기서는 아무것도
	//          하지 않는다. _maxConnections까지의 추가 증설은 TryGrow()가 실제
	//          수요에 반응해서 담당한다(이 함수의 책임이 아님). GetCurrentSessionCount()는
	//          "연결 완료된" 세션뿐 아니라 "연결 시도 중"(AddSession()이
	//          ConnectAsync() 게시 이전에 먼저 호출됨 — IocpService/RioService의
	//          ConnectOneMoreSession() 설계 참고)인 세션도 포함하므로, 짧은
	//          시간에 여러 번 불려도 자연스럽게 과다 재연결을 막아준다.
	//***************************************************************************
	void ScheduleReconnect()
	{
		if( !_clientService )
			return;

		if( _clientService->GetCurrentSessionCount() >= _minIdle )
			return;

		uint32_t failCount = _consecutiveFailCount.fetch_add(1, std::memory_order_relaxed);
		int32 delayMs = ComputeBackoffDelay(failCount);

		_delayedTaskQueue.Reserve(delayMs, [this]()
			{
				if( _clientService )
					_clientService->ConnectOneMoreSession();
			});
	}

	//***************************************************************************
	// @brief 실제 수요(대기열 발생)에 반응해 커넥션을 즉시(백오프 없이) 증설합니다.
	// @details ScheduleReconnect()와 달리 이건 장애 복구가 아니라 정상적인 용량
	//          확장이므로 지연을 걸지 않는다. _maxConnections 상한을 넘어서는
	//          증설은 하지 않는다 — ConnectOneMoreSession()이 AddSession()을
	//          즉시(연결 완료 전에) 수행해 GetCurrentSessionCount()에 바로
	//          반영되므로, 짧은 시간에 여러 번 호출돼도(요청 폭주 등) 상한에
	//          도달하는 순간 자연스럽게 멈춘다.
	//***************************************************************************
	void TryGrow()
	{
		if( !_clientService )
			return;

		if( _clientService->GetCurrentSessionCount() < _maxConnections )
			_clientService->ConnectOneMoreSession();
	}

private:
	TServiceRef _clientService; // 이 풀이 소유하는 IOCP/RIO 클라이언트 서비스
	std::function<void(size_t)> _sessionCountChangedHandler; // 활성 세션 수 변화 통지 콜백 (주로 CHttpConnPoolManager가 등록)
	std::atomic<int64_t> _notifiedSessionCount{ 0 }; // OnSessionConnStateChanged()로 직접 받은 연결/해제 통지만으로 세는 카운터
	// (하위 CNetService::_sessions의 정리 타이밍 레이스를 피하기 위해
	// GetActiveSessionCount() 대신 이 값을 _sessionCountChangedHandler에 씀 — 위 설명 참고)
	int32 _minIdle;             // host당 항상 유지할 기준 커넥션 수 (ScheduleReconnect()가 지킴)
	int32 _maxConnections;      // host당 허용할 최대 커넥션 수 (TryGrow()의 상한)

	std::mutex _lock;                            // _idleSessions/_pendingRequests 보호
	std::vector<TSessionRef> _idleSessions;      // 요청을 받을 수 있는 유휴 세션 목록
	std::deque<PendingRequest> _pendingRequests; // 유휴 세션이 없을 때 대기 중인 요청 큐

	std::atomic<uint32_t> _consecutiveFailCount{ 0 }; // 지수 백오프용 연속 연결 실패 횟수
	CDelayedTaskQueue _delayedTaskQueue;              // 재연결 작업 예약 큐
	std::thread _taskThread;                          // _delayedTaskQueue.ProcessExpiredTasks() 전용 스레드
};

#endif // ndef UC_HTTPCONNPOOL_H