
//***************************************************************************
// HttpConnPool.h : interface for the CHttpConnPoolT class template.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOL_H
#define UC_HTTPCONNPOOL_H

#include <Network/Session.h>
#include <Network/HTTP/HttpConnPoolCommon.h>
#include <Containers/Queue/DelayedTaskQueue.h>

#include <deque>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <functional>
#include <type_traits>
#include <utility>
#include <unordered_map>

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
//      [연결 계약] IOCP/RIO 모두 ConnectOneMoreSession()은 연결 시도를 게시만 하고
//      즉시 리턴하며, 실제 연결 완료/실패는 세션의 OnConnected()/OnDisconnected()가
//      항상 비동기로 통지한다. 그래서 이 풀은 두 엔진을 같은 코드 경로로 다룬다 —
//      ConnectOneMoreSession()의 반환값은 로깅용으로만 보고, 유휴 목록 등록은
//      OnSessionConnStateChanged(connected=true)에서만 한다.
//
//      [_minIdle vs _maxConnections — 책임 분리]
//      이 풀은 커넥션 수 관리를 두 가지 독립된 메커니즘으로 나눈다:
//      - ScheduleReconnect() (백오프 경로): 세션이 끊겼거나 연결 게시에 실패했을 때(OnSessionConnStateChanged
//        (false)) 호출된다. 목적은 "기준선(_minIdle) 유지"다 — 현재 세션 수가
//        _minIdle 미만일 때만 지수 백오프(200ms 시작, 2배씩 증가, 10초 상한)를
//        걸어 CDelayedTaskQueue에 재연결을 예약한다. 이미 _minIdle을 채우고
//        있으면(끊긴 게 여분의 버스트 커넥션이었다면) 여기서는 더 만들지 않는다.
//        지연은 연속 "연결 실패"(연결에 한 번도 성공하지 못한 세션의 종료, 연결 게시 실패)가
//        쌓일 때만 커지고, 연결에 성공하면 다시 기본값으로 돌아간다. 연결됐다가 끊긴 세션이나
//        Connection: close 폐기는 기본 지연으로 바로 다시 채운다.
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
// 서비스가 SetRemoteAddress()를 지원하는지 컴파일 타임에 판별한다 (지원하지 않는 서비스는 주소 갱신을 무시한다).
template<typename T, typename = void>
struct HttpSvcHasSetRemoteAddress : std::false_type {};
template<typename T>
struct HttpSvcHasSetRemoteAddress<T, std::void_t<decltype(std::declval<T&>().SetRemoteAddress(std::declval<const CNetAddress&>()))>> : std::true_type {};

template<typename TSession, typename TSessionRef, typename TService, typename TServiceRef>
class CHttpConnPoolT
	: public IHttpConnPool
	, public std::enable_shared_from_this<CHttpConnPoolT<TSession, TSessionRef, TService, TServiceRef>>
{
	using ThisPool = CHttpConnPoolT<TSession, TSessionRef, TService, TServiceRef>;
	using Clock = std::chrono::steady_clock;

public:
	// 재연결 백오프 기준값 — 테스트에서 타이밍 검증에 사용하므로 public.
	static constexpr int32 kBackoffBaseDelayMs = 200; // 최초 재연결 지연(ms)
	static constexpr int32 kBackoffMaxDelayMs = 10000; // 재연결 지연 상한(ms)

	// 요청 타임아웃: 기본 무응답 시간, 점검 주기, 멱등 요청 자동 재시도 횟수
	static constexpr int64 kDefaultRequestTimeoutMs = 30000;
	static constexpr int32 kTimeoutScanIntervalMs = 500;
	static constexpr int32 kMaxIdempotentRetries = 1;

	using IHttpConnPool::SendRequest; // 3인자 오버로드(풀 기본 타임아웃 사용)를 가리지 않게 한다

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
			_clientService->Close(); // 이미 게시된 초기 연결을 되돌린다
			return false;
		}

		ScheduleTimeoutScan(); // 요청 타임아웃 주기 점검 시작

		return true;
	}

	//***************************************************************************
	// @brief 풀을 정지합니다: 재연결 작업 큐 정지+join 후 클라이언트 서비스를 닫습니다.
	//***************************************************************************
	void Close() override
	{
		_closed.store(true, std::memory_order_release);
		_delayedTaskQueue.Stop();
		if( _taskThread.joinable() )
			_taskThread.join();

		if( _clientService )
			_clientService->Close();

		// 닫힌 뒤에는 대기 큐의 요청이 영원히 처리되지 않으므로 실패로 통지한다.
		FailPendingRequests();
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
		std::lock_guard<std::recursive_mutex> guard(_notifyLock);
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
	void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout) override
	{
		PendingRequest req;
		req.data.assign(data, len);
		req.onComplete = std::move(onComplete);
		req.submitTime = Clock::now();
		req.timeout = ResolveTimeout(timeout);
		req.idempotent = IsIdempotentRequest(req.data);
		req.retriesLeft = req.idempotent ? kMaxIdempotentRetries : 0;

		Submit(std::move(req), /*front=*/false);
	}

	void SetRemoteAddress(const CNetAddress& address) override
	{
		if constexpr( HttpSvcHasSetRemoteAddress<TService>::value )
		{
			if( _clientService )
				_clientService->SetRemoteAddress(address);
		}
	}

	void SetDefaultRequestTimeout(std::chrono::milliseconds timeout) override
	{
		_defaultTimeoutMs.store(timeout.count(), std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 세션의 연결 상태 변화를 통지받습니다.
	// @param sessionBase 상태가 변한 세션 (베이스 CSessionRef로 넘어옴)
	// @param connected true면 방금 연결 완료, false면 끊김(연결 실패 포함)
	// @details 실제로는 세션 팩토리가 등록한 콜백(SetConnStateHandler)을 통해서만 호출되지만,
	//          단위 테스트가 연결/해제 이벤트를 직접 흉내 낼 수 있게 public으로 둔다.
	//          connected==false면 ScheduleReconnect()로 기준선(_minIdle) 유지를 시도하고,
	//          true면 백오프 카운터를 리셋한 뒤 DispatchOrIdle()로 넘긴다.
	//
	//          [연결 실패 세션] 연결 시도 자체가 실패한 세션은 OnConnected() 없이
	//          OnDisconnected()만 호출한다 — connected==true 통지 없이 false만 온다.
	//          그래서 _connectedSessions에 "connected==true를 받은 세션"만 기록하고, false가
	//          왔을 때 그 집합에 있던 세션에 대해서만 _notifiedSessionCount를 줄인다.
	//
	//          [세션 수 통지는 직접 센 값] 핸들러에는 서비스의 세션 수(GetCurrentSessionCount)가 아니라
	//          이 풀이 받은 연결/해제 통지로 직접 센 연결 완료 세션 수를 넘긴다. 서비스는 OnDisconnected()
	//          훅을 부른 뒤에야 세션을 목록에서 빼므로, 그 값을 읽으면 마지막 세션이 끊길 때 0이 아니라
	//          1이 읽힌다.
	//***************************************************************************
	void OnSessionConnStateChanged(CSessionRef sessionBase, bool connected)
	{
		TSessionRef session = std::static_pointer_cast<TSession>(sessionBase);

		if( !connected )
		{
			bool wasConnected = false;
			{
				std::lock_guard<std::mutex> guard(_lock);
				wasConnected = (_connectedSessions.erase(sessionBase.get()) != 0);

				// 유휴 목록에 있던 세션이 끊겼다면(서버의 유휴 keep-alive 종료) 목록에서 제거한다 —
				// 남겨두면 다음 요청이 이 죽은 세션에 배정돼 즉시 실패한다.
				_idleSessions.erase(std::remove_if(_idleSessions.begin(), _idleSessions.end(),
					[&session](const TSessionRef& s) { return s.get() == session.get(); }), _idleSessions.end());
			}

			// 한 번도 연결된 적 없던 세션(FailConnect() 경로)의 실패 통지는
			// 카운터에 반영하지 않는다 — 매칭되는 increment가 없었으므로.
			if( wasConnected )
				_notifiedSessionCount.fetch_sub(1, std::memory_order_acq_rel);

			// 연결에 한 번도 성공하지 못한 세션의 종료만 "연결 실패"로 세어 백오프를 키운다. 연결됐다가
			// 끊긴 세션(서버의 유휴 종료 등)은 장애가 아니므로 기본 지연으로 바로 다시 채운다.
			ScheduleReconnect(/*countAsFailure=*/!wasConnected);
		}
		else
		{
			{
				std::lock_guard<std::mutex> guard(_lock);
				_connectedSessions.emplace(sessionBase.get(), session);
			}

			_notifiedSessionCount.fetch_add(1, std::memory_order_acq_rel);
			_consecutiveFailCount.store(0, std::memory_order_relaxed); // 연결 성공 -> 백오프 리셋
			DispatchOrIdle(session);
		}

		// 통지 직렬화: 값을 읽는 것과 핸들러 호출을 같은 락 안에서 한다. 두 세션이 서로 다른 스레드에서
		// 거의 동시에 끊기면, 카운트를 먼저 계산한 스레드의 통지가 나중에 도착해 핸들러(매니저)가 오래된
		// 값으로 굳을 수 있다. 이렇게 하면 통지는 항상 "전달 시점의 최신 카운트"를 담고 순서대로 전달되므로
		// 마지막 통지는 언제나 최종 값이다. 호출 스레드가 이 풀을 강한 참조(weakPool.lock())로 쥐고 있어
		// 핸들러 안에서 풀이 파괴되지 않는다. 핸들러 안의 사용자 콜백이 재진입할 수 있어 재귀 뮤텍스를 쓴다.
		{
			std::lock_guard<std::recursive_mutex> guard(_notifyLock);
			if( _sessionCountChangedHandler )
			{
				const int64 count = _notifiedSessionCount.load(std::memory_order_acquire);
				_sessionCountChangedHandler(static_cast<size_t>((std::max)(count, static_cast<int64>(0))));
			}
		}
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
	static int32 ComputeBackoffDelay(uint32 failCount) noexcept
	{
		uint32 shift = (std::min<uint32>)(failCount, 6);
		int64 delay = static_cast<int64>(kBackoffBaseDelayMs) << shift;
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
		Clock::time_point submitTime{};           // 타임아웃 기준 시각 (SendRequest() 호출 시점, 재시도해도 유지)
		std::chrono::milliseconds timeout{ 0 };    // 이 요청의 무응답 타임아웃 (0 이하면 없음)
		bool idempotent = false;                  // GET/HEAD인지 (자동 재시도 대상)
		int32 retriesLeft = 0;                    // 남은 자동 재시도 횟수
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
	//          [순환 참조 방지] 완료 콜백은 session->SendRequest() 안에서 그 세션의 CHttpClientCore에
	//          저장된다 — 즉 콜백이 세션 자신의 멤버 안에 들어간다. 여기에 TSessionRef(강한 참조)를
	//          캡처하면 세션이 자기 자신을 붙드는 순환이 되어, 응답이 끝내 오지 않는 경로(연결 중간 끊김,
	//          TLS 핸드셰이크 실패 등)에서 세션이 영원히 해제되지 않는다. 그래서 원시 포인터(sessionRaw)만
	//          캡처한다. 콜백은 세션이 자기 멤버 안에서 부르므로 호출 시점에 세션은 반드시 살아 있고,
	//          세션의 실제 수명은 서비스(CNetService::_sessions)가 보장한다. 강한 참조가 필요한
	//          OnRequestComplete() 시점에만 shared_from_this()로 다시 만든다.
	//***************************************************************************
	void DispatchToSession(TSessionRef session, PendingRequest&& req)
	{
		HttpRequestCompletionHandler userCb = std::move(req.onComplete);
		const Clock::time_point submitTime = req.submitTime;
		const std::chrono::milliseconds timeout = req.timeout;
		const int32 retriesLeft = req.retriesLeft;

		// 재시도 가능한 요청(멱등, 남은 횟수 있음)은 실패 시 다시 보낼 수 있게 요청 바이트를 공유 버퍼로
		// 보관한다(복사 없이 이동). 재시도 대상이 아니면 전송 호출 동안만 쓰는 지역 버퍼로 충분하다.
		std::shared_ptr<const std::string> retryData;
		std::string localData;
		const std::string* dataPtr = nullptr;
		if( retriesLeft > 0 )
		{
			retryData = std::make_shared<const std::string>(std::move(req.data));
			dataPtr = retryData.get();
		}
		else
		{
			localData = std::move(req.data);
			dataPtr = &localData;
		}

		auto self = this->shared_from_this();
		TSession* sessionRaw = session.get(); // 강한 참조 대신 원시 포인터만 캡처 (세션 -> 코어 -> 콜백 -> 세션 순환 방지)

		bool began = session->SendRequest(dataPtr->data(), dataPtr->size(),
			[self, sessionRaw, userCb, retryData, submitTime, timeout, retriesLeft](bool success, CHttpResponseParser& parser)
			{
				TSessionRef sessionRef = std::static_pointer_cast<TSession>(sessionRaw->shared_from_this());

				// 응답 바이트를 하나도 받기 전에 연결 수준에서 실패한 멱등 요청(예: 서버가 닫은 유휴 연결에 보낸
				// 경우)은 사용자에게 알리지 않고 한 번 다시 시도한다. 타임아웃은 서버가 요청을 이미 처리했을 수
				// 있으므로 재시도하지 않는다.
				if( !success && retryData && !parser.IsTimedOut() && parser.BytesFed() == 0 )
				{
					self->DiscardSession(sessionRef);
					self->Submit(self->MakeRetry(*retryData, userCb, submitTime, timeout, retriesLeft - 1), /*front=*/true);
					return;
				}

				if( userCb )
					userCb(success, parser);

				self->OnRequestComplete(sessionRef, success);
			},
			timeout, submitTime);

		if( !began )
		{
			// 전송을 시작하지 못했다 — 이 경우 위 완료 핸들러는 호출되지 않았으므로 여기서 직접 처리한다.
			if( retryData )
			{
				DiscardSession(session);
				Submit(MakeRetry(*retryData, userCb, submitTime, timeout, retriesLeft - 1), /*front=*/true);
				return;
			}

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
		// Connection: close 응답을 받았거나 실패했거나 이미 끊긴 세션이면 재사용하지 않고 폐기.
		if( !success || session->IsConnectionCloseRequested() || !session->IsConnected() )
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
	// @details Disconnect()가 비동기로 OnDisconnected()를 유발해 ScheduleReconnect()가 다시 불릴 수
	//          있다. 재연결 작업은 실행 시점에 세션 수를 다시 확인하므로 중복 예약은 안전하다. 세션이
	//          이미 끊겨 통지가 다시 오지 않는 경로를 위해 여기서도 예약한다. 폐기는 연결 실패가 아니므로
	//          백오프 카운터는 올리지 않는다.
	//***************************************************************************
	void DiscardSession(TSessionRef session)
	{
		session->Disconnect(_T("HttpConnPool discard"));
		ScheduleReconnect(/*countAsFailure=*/false);
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
	//
	//          [실행 시점 재검증] 이 함수의 _minIdle 체크는 "예약하는 시점"의
	//          스냅샷일 뿐이다 — 세션 여러 개가 짧은 시간에 한꺼번에 끊기면
	//          ScheduleReconnect()가 그만큼 여러 번 예약되고, 그 사이 다른
	//          재연결이 먼저 성공해 기준선이 이미 회복돼도 예약된 작업들은
	//          그대로 실행돼버려 _minIdle은 물론(예약 시 캡을 아예 확인하지
	//          않는) _maxConnections까지 넘어설 수 있었다. 그래서 지연 실행되는
	//          콜백 안에서도 실행 "그 순간"의 카운트로 _minIdle/_maxConnections
	//          둘 다 다시 확인한다 — 어느 한쪽이라도 이미 채워져 있으면 실행을
	//          건너뛴다.
	//***************************************************************************
	void ScheduleReconnect(bool countAsFailure)
	{
		if( !_clientService )
			return;

		// 스케줄 시점의 세션 수로 걸러내지 않는다 — 이 함수는 세션의 OnDisconnected() 통지 안에서
		// (풀로의 연결 상태 통지로) 불리고, 그 시점엔 끊기는 세션이 아직 서비스의 세션 수에 포함돼
		// 있다(CSession::OnDisconnected()의 ReleaseSession이 그 뒤에 실행된다). 여기서 걸러내면
		// 기준선(_minIdle) 아래로 내려가도 재연결이 스케줄되지 않는다. 실행 시점의 세션 수 검사(아래
		// 람다)가 과잉 연결을 막는다.

		// countAsFailure면 연속 실패 횟수를 올려 지연을 키우고, 아니면 현재 횟수(성공 시 0)의 지연을 쓴다.
		const uint32 failCount = countAsFailure
			? _consecutiveFailCount.fetch_add(1, std::memory_order_relaxed)
			: _consecutiveFailCount.load(std::memory_order_relaxed);
		int32 delayMs = ComputeBackoffDelay(failCount);

		_delayedTaskQueue.Reserve(std::chrono::milliseconds(delayMs), [this]()
			{
				if( !_clientService )
					return;
				int32 currentCount = _clientService->GetCurrentSessionCount();
				if( currentCount >= _minIdle || currentCount >= _maxConnections )
					return;
				// 연결 시도를 게시하지 못했다(소켓 생성 실패 등) — 기준선이 비어 있으므로 백오프로 다시 시도한다.
				if( !_clientService->ConnectOneMoreSession() )
					ScheduleReconnect(/*countAsFailure=*/true);
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

		if( _clientService->GetCurrentSessionCount() < _maxConnections && !_clientService->ConnectOneMoreSession() )
			ScheduleReconnect(/*countAsFailure=*/true); // 게시 실패 — 기준선 아래라면 백오프 재시도 (이상이면 아무 일도 안 함)
	}

	//***************************************************************************
	// @brief 요청을 유휴 세션에 배정하거나 대기 큐에 넣습니다 (SendRequest()와 자동 재시도가 공유).
	// @param front true면 대기 큐 맨 앞에 넣는다 (재시도 요청은 이미 오래 기다렸으므로)
	//***************************************************************************
	void Submit(PendingRequest&& req, bool front)
	{
		TSessionRef idleSession;
		bool queued = false;
		bool closed = false;
		{
			std::lock_guard<std::mutex> guard(_lock);

			// 닫힌 풀의 대기 큐에 넣으면 영원히 처리되지 않는다. Close()는 _closed를 먼저 세운 뒤 같은
			// 락으로 대기 큐를 비우므로, 락 안에서 확인하면 닫히는 도중의 요청도 큐에 남지 않는다.
			if( _closed.load(std::memory_order_acquire) )
			{
				closed = true;
			}
			else
			{
				// 이미 끊긴 유휴 세션(서버가 유휴 keep-alive 연결을 닫은 경우)은 건너뛴다 — 그런 세션에
				// 요청을 보내면 Send가 실패한다. 끊김 통지(OnSessionConnStateChanged)가 도착하기 전의 좁은
				// 틈을 위한 방어이며, 통지가 도착한 세션은 거기서 이미 이 목록에서 제거된다.
				while( !_idleSessions.empty() )
				{
					TSessionRef candidate = std::move(_idleSessions.back());
					_idleSessions.pop_back();
					if( candidate && candidate->IsConnected() )
					{
						idleSession = std::move(candidate);
						break;
					}
				}

				if( !idleSession )
				{
					if( front )
						_pendingRequests.push_front(std::move(req));
					else
						_pendingRequests.push_back(std::move(req));
					queued = true;
				}
			}
		}

		if( closed )
		{
			if( req.onComplete )
			{
				CHttpResponseParser dummy;
				req.onComplete(false, dummy);
			}
			return;
		}

		if( queued )
		{
			// 유휴 세션이 없다 — 한도 안이면 연결을 늘려 대기 요청이 곧 처리되게 한다.
			TryGrow();
			return;
		}

		DispatchToSession(idleSession, std::move(req));
	}

	PendingRequest MakeRetry(const std::string& data, const HttpRequestCompletionHandler& userCb,
		Clock::time_point submitTime, std::chrono::milliseconds timeout, int32 retriesLeft) const
	{
		PendingRequest retry;
		retry.data = data;
		retry.onComplete = userCb;
		retry.submitTime = submitTime;
		retry.timeout = timeout;
		retry.idempotent = true;
		retry.retriesLeft = retriesLeft;
		return retry;
	}

	std::chrono::milliseconds ResolveTimeout(std::chrono::milliseconds requested) const
	{
		if( requested.count() < 0 )
			return std::chrono::milliseconds(-1); // 이 요청은 타임아웃 없음
		if( requested.count() > 0 )
			return requested;
		return std::chrono::milliseconds(_defaultTimeoutMs.load(std::memory_order_relaxed)); // 풀 기본값 (0 이하면 없음)
	}

	static bool IsIdempotentRequest(const std::string& data) noexcept
	{
		return data.compare(0, 4, "GET ") == 0 || data.compare(0, 5, "HEAD ") == 0;
	}

	//***************************************************************************
	// @brief 대기 큐와 연결된 세션을 점검해 무응답 타임아웃을 넘긴 요청을 실패로 통지합니다.
	// @details 사용자 콜백은 락 밖에서 호출한다. 진행 중인 요청의 만료 판단은 세션(코어)이 자기 락 안에서
	//          "현재 요청"에 대해 하므로, 점검과 요청 완료/세션 재사용이 겹쳐도 엉뚱한 요청을 끊지 않는다.
	//***************************************************************************
	void ScanTimeouts()
	{
		const Clock::time_point now = Clock::now();
		std::vector<PendingRequest> expired;
		std::vector<TSessionRef> sessions;
		{
			std::lock_guard<std::mutex> guard(_lock);

			for( auto it = _pendingRequests.begin(); it != _pendingRequests.end(); )
			{
				if( it->timeout.count() > 0 && now - it->submitTime >= it->timeout )
				{
					expired.push_back(std::move(*it));
					it = _pendingRequests.erase(it);
				}
				else
				{
					++it;
				}
			}

			sessions.reserve(_connectedSessions.size());
			for( auto& entry : _connectedSessions )
				sessions.push_back(entry.second);
		}

		for( PendingRequest& r : expired )
		{
			if( r.onComplete )
			{
				CHttpResponseParser parser;
				parser.MarkTimedOut();
				r.onComplete(false, parser);
			}
		}

		for( TSessionRef& session : sessions )
			session->CheckRequestTimeout(now);
	}

	void ScheduleTimeoutScan()
	{
		// 일회성 예약이므로 실행될 때마다 자기 자신을 다시 예약한다. Close()가 큐를 Stop()하면
		// Reserve()가 false를 반환하며 조용히 무시되어 재귀가 끊긴다.
		_delayedTaskQueue.Reserve(std::chrono::milliseconds(kTimeoutScanIntervalMs), [this]()
			{
				ScanTimeouts();
				ScheduleTimeoutScan();
			});
	}

	// 풀이 닫힌 뒤 대기 큐에 남은 요청을 실패로 통지한다 (이후 처리될 방법이 없다).
	void FailPendingRequests()
	{
		std::deque<PendingRequest> drained;
		{
			std::lock_guard<std::mutex> guard(_lock);
			drained.swap(_pendingRequests);
		}

		for( PendingRequest& r : drained )
		{
			if( r.onComplete )
			{
				CHttpResponseParser dummy;
				r.onComplete(false, dummy);
			}
		}
	}

private:
	TServiceRef _clientService; // 이 풀이 소유하는 IOCP/RIO 클라이언트 서비스
	std::function<void(size_t)> _sessionCountChangedHandler; // 활성 세션 수 변화 통지 콜백 (주로 CHttpConnPoolManager가 등록)
	std::recursive_mutex _notifyLock;                // 세션 수 통지의 값 읽기+핸들러 호출을 직렬화
	std::atomic<int64> _notifiedSessionCount{ 0 }; // 연결/해제 통지로 직접 센 연결 완료 세션 수 (_sessionCountChangedHandler에 넘기는 값)
	int32 _minIdle;             // host당 항상 유지할 기준 커넥션 수 (ScheduleReconnect()가 지킴)
	int32 _maxConnections;      // host당 허용할 최대 커넥션 수 (TryGrow()의 상한)

	std::mutex _lock;                            // _idleSessions/_pendingRequests/_connectedSessions 보호
	std::vector<TSessionRef> _idleSessions;      // 요청을 받을 수 있는 유휴 세션 목록
	std::deque<PendingRequest> _pendingRequests; // 유휴 세션이 없을 때 대기 중인 요청 큐
	std::unordered_map<CSession*, TSessionRef> _connectedSessions; // connected==true 통지를 받은 세션 (강한 참조 — 타임아웃 점검이 안전하게 호출하도록. 연결 실패 세션은 제외)

	std::atomic<int64> _defaultTimeoutMs{ kDefaultRequestTimeoutMs }; // timeout 인자가 0인 요청에 적용할 기본 무응답 타임아웃(ms), 0 이하면 없음
	std::atomic<bool> _closed{ false };             // Close()가 호출됐는지 (이후 요청은 즉시 실패 처리)
	std::atomic<uint32> _consecutiveFailCount{ 0 }; // 지수 백오프용 연속 연결 실패 횟수
	CDelayedTaskQueue _delayedTaskQueue;              // 재연결 작업 예약 큐
	std::thread _taskThread;                          // _delayedTaskQueue.ProcessExpiredTasks() 전용 스레드
};

#endif // ndef UC_HTTPCONNPOOL_H