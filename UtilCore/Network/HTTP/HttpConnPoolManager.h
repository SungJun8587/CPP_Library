
//***************************************************************************
// HttpConnPoolManager.h : interface for the CHttpConnPoolManager class.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOLMANAGER_H
#define UC_HTTPCONNPOOLMANAGER_H

#include <Network/HTTP/HttpConnPoolCommon.h>
#include <Network/NetAddress.h>

#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <future>
#include <cstdint>

//***************************************************************************
// @brief hostname을 CNetAddress로 변환하는 DNS resolve 콜백.
// @return bool 성공 여부. 실패(호스트를 못 찾음 등) 시 false, outAddr은 미정의.
//***************************************************************************
using HttpDnsResolveFn = std::function<bool(const std::string& hostname, uint16 port, CNetAddress& outAddr)>;

//***************************************************************************
// @brief hostname + resolve된 주소로 CHttpConnPoolT<...> 인스턴스를 만드는 콜백.
// @details hostname을 별도로 받는 이유: HTTPS는 TLS SNI/인증서 검증에 원래
//          hostname 문자열이 필요한데, resolvedAddr(=CNetAddress)은 이미
//          IP로 변환된 뒤라 그 정보가 없다. HttpConnPoolFactory.h의
//          CreateHttpsConnPoolIocp/Rio()가 이 hostname으로 SetTlsConfig()를
//          호출한다.
//***************************************************************************
using HttpConnPoolCreator = std::function<IHttpConnPoolRef(const std::string& hostname, CNetAddress resolvedAddr)>;

//***************************************************************************
// @class CHttpConnPoolManager
// @brief hostname(문자열) 여러 개를 관리하는 상위 HTTP 커넥션 풀 매니저
//
// @details
//      CHttpConnPoolT<...>는 host 하나만 다루는 단위다. 실제 사용 시에는
//      "동일 프로세스에서 여러 외부 API/내부 서버로 HTTP 요청을 보낸다"가
//      일반적인 시나리오라, host별로 CHttpConnPoolT 인스턴스를 필요할 때 만들고
//      캐싱해서 재사용하는 이 상위 계층이 필요하다.
//
//      [hostname 문자열로 키잉] host별 풀 캐시는 CNetAddress(IP)가 아니라
//      hostname 문자열로 키잉한다 — DNS resolve는 이 매니저가 내부적으로
//      (주입받은 HttpDnsResolveFn을 통해) 수행하며, HTTPS의 TLS SNI에 필요한
//      원래 hostname을 그대로 유지하기 위함이다.
//
//      [_pools vs _closingPools — 조기 파괴 방지] 풀의 Close()가 끝나도 세션 정리 완료
//      통지가 뒤늦게 올 수 있고, 풀 객체(와 그 안의 클라이언트 서비스)는 마지막
//      shared_ptr이 사라지는 순간 파괴된다. CloseAll()이 풀을 _pools에서 지우고
//      그대로 놓아 버리면 세션이 남아 있는데도 풀이 파괴돼 GetActiveSessionCount()를
//      물어볼 대상이 사라진다. 그래서 "요청을 받을 수 있는 활성 풀"(_pools)과 "닫혔지만
//      세션이 아직 다 안 빠진 풀"(_closingPools)을 분리하고, 후자는 그 풀이 세션 수 0을
//      보고할 때까지 강한 참조로 붙든다(OnPoolSessionCountChanged() 참고).
//
//      [풀 생성 직렬화] 같은 host로 동시에 첫 요청이 오면 한 스레드만 DNS 해석과 풀 생성을
//      맡고(_creating), 나머지는 그 결과를 기다려 같은 풀을 받는다. host별로 풀이 한 번만
//      만들어지며, 서로 다른 host의 생성은 병렬로 진행된다.
//
//      [폴링 없는 종료 대기] 각 CHttpConnPoolT는 SetSessionCountChangedHandler()로 등록된
//      콜백을 세션 연결/해제 때마다 호출한다. 이 매니저는 모든 풀(활성/정리 중)에 그 훅을
//      걸고, 풀별 마지막 카운트와의 차이만 합계에 반영한 뒤 WaitUntilAllSessionsClosed()의
//      condition_variable을 깨우거나 SetAllSessionsClosedHandler()의 콜백을 부른다.
//
//      [shared_ptr로만 생성해야 함] 위 훅을 걸 때 shared_from_this()를 쓰므로,
//      이 클래스는 반드시 std::make_shared<CHttpConnPoolManager>(...)처럼
//      shared_ptr로 소유된 상태에서만 정상 동작한다(스택 객체로 만들면
//      GetOrCreatePool() 호출 시 std::bad_weak_ptr 예외가 던져진다) —
//      HttpConnPoolFactory.h의 Create* 함수들은 전부 이 방식으로 만든다.
//***************************************************************************
class CHttpConnPoolManager : public std::enable_shared_from_this<CHttpConnPoolManager>
{
public:
	//***************************************************************************
	// @brief CHttpConnPoolManager 생성자
	// @param creator host 하나에 대한 풀을 만드는 콜백 (HttpConnPoolFactory.h의
	//        CreateHttpConnPoolIocp/Rio, CreateHttpsConnPoolIocp/Rio 등으로 만들어 넘기면 됨)
	// @param resolver hostname을 CNetAddress로 바꾸는 콜백 (HttpConnPoolFactory.h의
	//        기본 DNS resolve 헬퍼를 쓰거나, 테스트에서는 mock으로 대체 가능)
	//***************************************************************************
	CHttpConnPoolManager(HttpConnPoolCreator creator, HttpDnsResolveFn resolver)
		: _creator(std::move(creator)), _resolver(std::move(resolver))
	{
	}

	//***************************************************************************
	// @brief 소멸자. 캐싱된 모든 풀을 CloseAll()로 정리합니다.
	//***************************************************************************
	~CHttpConnPoolManager()
	{
		CloseAll();
	}

	CHttpConnPoolManager(const CHttpConnPoolManager&) = delete;
	CHttpConnPoolManager& operator=(const CHttpConnPoolManager&) = delete;

	//***************************************************************************
	// @brief 완성된 HTTP 요청 패킷을 지정된 host로 비동기 전송합니다.
	// @param hostname 목적지 hostname (DNS resolve는 이 매니저가 내부적으로 수행)
	// @param port 목적지 포트
	// @param data 요청 패킷 바이트
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @details 해당 host의 풀이 없으면 DNS resolve부터 하고 풀을 새로 만들어
	//          Start()한다. resolve 실패, 풀 생성 실패, Start() 실패 중 어느
	//          것이든 onComplete를 즉시 success=false로 호출한다.
	//***************************************************************************
	void SendRequest(const std::string& hostname, uint16 port, const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
	{
		IHttpConnPoolRef pool = GetOrCreatePool(hostname, port);
		if( !pool )
		{
			if( onComplete )
			{
				CHttpResponseParser dummy;
				onComplete(false, dummy);
			}
			return;
		}

		pool->SendRequest(data, len, std::move(onComplete), timeout);
	}

	//***************************************************************************
	// @brief 이후(그리고 이미 만들어진) 모든 호스트 풀의 기본 요청 타임아웃(무응답 시간)을 설정합니다.
	// @param timeout 0 이하면 기본적으로 타임아웃을 두지 않는다. 설정하지 않으면 풀의 기본값(30초)이 적용된다.
	//***************************************************************************
	void SetDefaultRequestTimeout(std::chrono::milliseconds timeout)
	{
		_defaultTimeoutMs.store(timeout.count(), std::memory_order_release);

		std::vector<IHttpConnPoolRef> pools;
		{
			std::lock_guard<std::mutex> guard(_lock);
			pools.reserve(_pools.size());
			for( auto& entry : _pools )
				pools.push_back(entry.second);
		}
		for( auto& pool : pools )
		{
			if( pool )
				pool->SetDefaultRequestTimeout(timeout);
		}
	}

	//***************************************************************************
	// @brief 호스트 주소를 백그라운드에서 다시 해석하는 주기를 설정합니다 (기본 60초).
	// @param interval 0 이하면 다시 해석하지 않는다(풀을 만들 때 해석한 주소를 계속 쓴다).
	// @details 풀은 처음 요청이 올 때 한 번 해석한 주소로 연결을 만든다. 장기 실행 서버에서 대상 서버의
	//          IP가 바뀌면(DNS 기반 장애 조치, 로드밸런서 교체) 그 주소를 계속 쓰게 되므로, 별도 스레드가
	//          주기적으로 다시 해석해 주소가 바뀌었으면 풀에 알린다. 이후 새로 만드는 연결만 새 주소를 쓰고
	//          기존 연결은 그대로 둔다. 해석은 요청 경로와 무관한 스레드에서 하므로 요청을 막지 않으며,
	//          해석에 실패하면 기존 주소를 유지한다.
	//***************************************************************************
	void SetDnsRefreshInterval(std::chrono::milliseconds interval)
	{
		_dnsRefreshIntervalMs.store(interval.count(), std::memory_order_release);
		_dnsCv.notify_all(); // 대기 중인 갱신 스레드가 새 주기를 다시 읽게 한다
		EnsureDnsRefresher();
	}

	//***************************************************************************
	// @brief 캐싱된 모든 host 풀을 닫습니다. 소멸자에서도 호출되며 여러 번 호출해도 안전합니다
	//        (두 번째 호출은 빈 맵을 순회할 뿐).
	// @details 풀 객체는 세션이 실제로 다 빠질 때까지 _closingPools에 남아 살아 있다 —
	//          클래스 설명의 [_pools vs _closingPools] 참고.
	//***************************************************************************
	void CloseAll()
	{
		StopDnsRefresher(); // 갱신 스레드를 먼저 멈춘다 (풀을 닫는 동안 주소를 건드리지 않게)

		std::vector<IHttpConnPoolRef> poolsToClose;
		{
			std::lock_guard<std::mutex> guard(_lock);
			poolsToClose.reserve(_pools.size());
			for( auto& [key, pool] : _pools )
				poolsToClose.push_back(pool);
			_pools.clear();
			_poolMeta.clear();
		}

		for( auto& pool : poolsToClose )
		{
			if( !pool )
				continue;
			TrackClosing(pool);
			pool->Close();
			// 세션이 하나도 없던 풀은 세션 수 통지가 오지 않으므로, 닫은 뒤의 실제 값을 한 번 반영해
			// _closingPools에서 내보낸다.
			OnPoolSessionCountChanged(pool.get(), pool->GetActiveSessionCount());
		}
	}

	//***************************************************************************
	// @brief 현재 요청을 받을 수 있는(정리 중이 아닌) host 풀의 개수를 반환합니다.
	//***************************************************************************
	size_t GetPoolCount() const
	{
		std::lock_guard<std::mutex> guard(_lock);
		return _pools.size();
	}

	//***************************************************************************
	// @brief 활성 풀 + 정리 중인 풀을 통틀어 현재 세션 수 합계를 반환합니다.
	// @return size_t 마지막으로 갱신된 합계 (풀들이 세션 수 변화를 통지할 때마다
	//         갱신되는 캐시값 — 이 호출 자체가 풀들을 순회하며 다시 계산하지
	//         않는다, 즉 폴링이 아니다).
	//***************************************************************************
	size_t GetTotalActiveSessionCount() const
	{
		std::lock_guard<std::mutex> guard(_countLock);
		return _cachedTotalActiveCount;
	}

	//***************************************************************************
	// @brief 활성+정리 중인 풀의 세션 수 합이 0이 될 때까지 블로킹 대기합니다.
	// @details condition_variable 기반이라 폴링하지 않는다 — 마지막 세션이 실제로
	//          정리되는 순간 즉시 깨어난다. 종료 시퀀스: CHttpClient::CloseAll() 호출 후 이 함수로
	//          대기한 다음 IOCP/RIO 워커 스레드를 멈춘다.
	//***************************************************************************
	void WaitUntilAllSessionsClosed()
	{
		std::unique_lock<std::mutex> guard(_countLock);
		_countChangedCv.wait(guard, [this] { return _cachedTotalActiveCount == 0; });
	}

	//***************************************************************************
	// @brief 세션 수 합이 0이 되거나 timeout이 지날 때까지 블로킹 대기합니다.
	// @param timeout 최대 대기 시간
	// @return bool true면 0에 도달해서 반환, false면 timeout으로 반환(아직 세션이 남아있음)
	//***************************************************************************
	bool WaitUntilAllSessionsClosed(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> guard(_countLock);
		return _countChangedCv.wait_for(guard, timeout, [this] { return _cachedTotalActiveCount == 0; });
	}

	//***************************************************************************
	// @brief 세션 수 합이 0에 도달하는 순간(비동기·1회) 호출되는 콜백을 등록합니다.
	// @param handler 세션 수 0 도달 시 호출될 콜백
	// @details 블로킹 대기 대신 비동기로 종료 흐름을 짜고 싶을 때 쓴다(예: 콜백 안에서 워커 풀 정지).
	//          등록 시점에 이미 합이 0이면 저장하지 않고 이 함수를 부른 스레드에서 즉시 호출한다.
	//          그렇지 않으면 합이 0이 되는 순간 한 번만 호출되고 핸들러는 버려진다.
	//***************************************************************************
	void SetAllSessionsClosedHandler(std::function<void()> handler)
	{
		bool fireNow = false;
		{
			std::lock_guard<std::mutex> guard(_countLock);
			fireNow = (_cachedTotalActiveCount == 0);
			if( !fireNow )
				_allSessionsClosedHandler = std::move(handler);
		}
		if( fireNow && handler )
			handler();
	}

private:
	//***************************************************************************
	// @brief hostname:port에 해당하는 풀을 찾거나, 없으면 DNS resolve 후 새로
	//        만들어 캐시에 등록합니다.
	// @param hostname 목적지 hostname
	// @param port 목적지 포트
	// @return IHttpConnPoolRef 찾았거나 새로 만든 풀. resolve/생성/Start() 실패 시 nullptr.
	//***************************************************************************
	IHttpConnPoolRef GetOrCreatePool(const std::string& hostname, uint16 port)
	{
		const std::string key = MakeKey(hostname, port);

		std::promise<IHttpConnPoolRef> promise;
		std::shared_future<IHttpConnPoolRef> inFlight;
		{
			std::lock_guard<std::mutex> guard(_lock);
			auto it = _pools.find(key);
			if( it != _pools.end() )
				return it->second;

			auto creating = _creating.find(key);
			if( creating != _creating.end() )
				inFlight = creating->second; // 다른 스레드가 만드는 중 — 결과를 기다린다
			else
				_creating.emplace(key, promise.get_future().share());
		}

		if( inFlight.valid() )
			return inFlight.get();

		// 이 스레드가 생성 담당이다. DNS 해석/풀 생성/Start()는 시간이 걸릴 수 있어 락 밖에서 한다.
		IHttpConnPoolRef pool;
		try
		{
			CNetAddress resolvedAddr;
			pool = CreateAndStartPool(hostname, port, resolvedAddr);

			std::lock_guard<std::mutex> guard(_lock);
			if( pool )
			{
				_pools.emplace(key, pool);
				_poolMeta[key] = PoolMeta{ hostname, port, resolvedAddr };
			}
			_creating.erase(key);
		}
		catch( ... )
		{
			{
				std::lock_guard<std::mutex> guard(_lock);
				_creating.erase(key);
			}
			promise.set_value(nullptr); // 기다리던 스레드가 영원히 막히지 않게 한다
			throw;
		}

		promise.set_value(pool);

		if( pool )
			EnsureDnsRefresher();

		return pool;
	}

	//***************************************************************************
	// @brief DNS 해석 -> 풀 생성 -> 세션 수 훅 설치 -> Start()를 수행합니다.
	// @details 훅은 Start() 전에 건다 — Start()가 게시한 연결의 완료 통지를 하나도 놓치지 않는다.
	//          Start()가 실패하면 풀을 닫고 훅이 반영해 둔 카운트를 되돌린다.
	// @return 구동된 풀. 해석/생성/Start() 실패 시 nullptr.
	//***************************************************************************
	IHttpConnPoolRef CreateAndStartPool(const std::string& hostname, uint16 port, CNetAddress& outResolvedAddr)
	{
		if( !_resolver || !_resolver(hostname, port, outResolvedAddr) )
			return nullptr;

		IHttpConnPoolRef pool = _creator ? _creator(hostname, outResolvedAddr) : nullptr;
		if( !pool )
			return nullptr;

		const int64_t configured = _defaultTimeoutMs.load(std::memory_order_acquire);
		if( configured != kTimeoutNotConfigured )
			pool->SetDefaultRequestTimeout(std::chrono::milliseconds(configured));

		InstallCountChangedHook(pool);

		if( !pool->Start() )
		{
			pool->Close();
			OnPoolSessionCountChanged(pool.get(), 0);
			return nullptr;
		}

		return pool;
	}

	//***************************************************************************
	// @brief hostname:port로 캐시 키 문자열을 만듭니다.
	//***************************************************************************
	static std::string MakeKey(const std::string& hostname, uint16 port)
	{
		return hostname + ":" + std::to_string(port);
	}

	//***************************************************************************
	// @brief 풀에 세션 수 변화 통지 훅을 겁니다. weak_ptr로 매니저 자신을
	//        캡처해 순환 참조를 만들지 않는다(풀 -> 매니저 강한 참조가 생기면
	//        매니저 -> 풀(강한 참조, _pools/_closingPools) -> 매니저(방금 건
	//        콜백) 순환이 되어 매니저 자신도 절대 안 없어지게 됨).
	//***************************************************************************
	void InstallCountChangedHook(const IHttpConnPoolRef& pool)
	{
		std::weak_ptr<CHttpConnPoolManager> weakSelf = weak_from_this();
		IHttpConnPool* rawPool = pool.get();

		pool->SetSessionCountChangedHandler([weakSelf, rawPool](size_t newCount)
			{
				if( auto self = weakSelf.lock() )
					self->OnPoolSessionCountChanged(rawPool, newCount);
			});
	}

	//***************************************************************************
	// @brief 정리 중인 풀을 세션이 실제로 0이 될 때까지 강한 참조로 붙들어 둡니다.
	//***************************************************************************
	void TrackClosing(const IHttpConnPoolRef& pool)
	{
		std::lock_guard<std::mutex> guard(_lock);
		_closingPools.emplace(pool.get(), pool);
	}

	//***************************************************************************
	// @brief 어떤 풀이든 활성 세션 수가 바뀔 때마다 호출됩니다(InstallCountChangedHook() 참고).
	// @param rawPool 세션 수가 바뀐 풀 (식별 + 이전 카운트 조회용)
	// @param newCount 그 풀의 새 활성 세션 수
	// @details (1) 정리 중이던 풀이 0이 됐으면 _closingPools에서 내보내 파괴될 수 있게 하고,
	//          (2) 이 풀의 마지막 반영 카운트와 newCount의 차이(델타)만 전체 합계에 더해 O(1)로
	//          갱신한 뒤, (3) condition_variable을 깨우고 합이 0이 됐으면 등록된 1회성 콜백을 부른다.
	//***************************************************************************
	void OnPoolSessionCountChanged(IHttpConnPool* rawPool, size_t newCount)
	{
		int64 delta = 0;
		{
			std::lock_guard<std::mutex> guard(_lock);

			auto it = _lastKnownCounts.find(rawPool);
			size_t previousCount = (it != _lastKnownCounts.end()) ? it->second : 0;
			delta = static_cast<int64>(newCount) - static_cast<int64>(previousCount);
			_lastKnownCounts[rawPool] = newCount;

			if( newCount == 0 )
			{
				auto closingIt = _closingPools.find(rawPool);
				if( closingIt != _closingPools.end() )
					_closingPools.erase(closingIt); // 여기서 마지막 강한 참조가 풀려 풀 객체가 파괴될 수 있음
				// 풀 객체 자체가 사라지므로, 다음 신규 풀이 같은 주소를 재사용할 때
				// 옛 카운트가 잘못 델타 계산에 섞이지 않도록 캐시도 함께 지운다.
				_lastKnownCounts.erase(rawPool);
			}
		}

		bool justReachedZero = false;
		std::function<void()> handlerCopy;
		{
			std::lock_guard<std::mutex> countGuard(_countLock);
			bool wasNonZero = (_cachedTotalActiveCount != 0);
			_cachedTotalActiveCount = static_cast<size_t>(static_cast<int64>(_cachedTotalActiveCount) + delta);
			if( wasNonZero && _cachedTotalActiveCount == 0 )
			{
				justReachedZero = true;
				handlerCopy = std::move(_allSessionsClosedHandler); // 1회성 — 호출 후 다시 불리지 않는다
				_allSessionsClosedHandler = nullptr;
			}
		}
		_countChangedCv.notify_all();

		if( justReachedZero && handlerCopy )
			handlerCopy();
	}

private:
	struct PoolMeta
	{
		std::string hostname;
		uint16 port = 0;
		CNetAddress lastAddr;   // 풀이 지금 쓰고 있는(마지막으로 알린) 주소
	};

	static bool SameAddress(const CNetAddress& a, const CNetAddress& b)
	{
		const auto sa = a.GetSockAddr();
		const auto sb = b.GetSockAddr();
		return sa.sin_addr.s_addr == sb.sin_addr.s_addr && sa.sin_port == sb.sin_port;
	}

	// 첫 풀이 만들어질 때(또는 주기가 설정될 때) 갱신 스레드를 시작한다. 주기가 0 이하이면 시작하지 않는다.
	void EnsureDnsRefresher()
	{
		if( _dnsRefreshIntervalMs.load(std::memory_order_acquire) <= 0 )
			return;

		std::lock_guard<std::mutex> guard(_dnsLock);
		if( _dnsStarted || _dnsStop )
			return;

		try
		{
			_dnsThread = std::thread([this]() { DnsRefreshLoop(); });
			_dnsStarted = true;
		}
		catch( ... )
		{
			// 스레드를 만들지 못하면 주소 갱신만 하지 않는다 (요청 처리에는 영향 없음).
		}
	}

	void StopDnsRefresher()
	{
		std::thread toJoin;
		{
			std::lock_guard<std::mutex> guard(_dnsLock);
			if( !_dnsStarted )
				return;
			_dnsStop = true;
			toJoin = std::move(_dnsThread);
		}
		_dnsCv.notify_all();
		if( toJoin.joinable() )
			toJoin.join();

		std::lock_guard<std::mutex> guard(_dnsLock);
		_dnsStop = false;     // CloseAll() 이후 새 풀이 만들어지면 다시 시작할 수 있게 한다
		_dnsStarted = false;
	}

	bool DnsStopRequested()
	{
		std::lock_guard<std::mutex> guard(_dnsLock);
		return _dnsStop;
	}

	void DnsRefreshLoop()
	{
		for( ;; )
		{
			{
				std::unique_lock<std::mutex> lk(_dnsLock);
				const int64_t ms = _dnsRefreshIntervalMs.load(std::memory_order_acquire);
				const auto wait = (ms > 0) ? std::chrono::milliseconds(ms) : std::chrono::hours(1);
				if( _dnsCv.wait_for(lk, wait, [this]() { return _dnsStop; }) )
					return; // 중지 요청
				if( _dnsRefreshIntervalMs.load(std::memory_order_acquire) <= 0 )
					continue; // 그 사이 꺼졌다
			}
			RefreshAddressesOnce();
		}
	}

	// 모든 풀의 호스트를 다시 해석해, 주소가 바뀐 풀에만 새 주소를 알린다. DNS 해석은 느릴 수 있으므로 락 밖에서 한다.
	void RefreshAddressesOnce()
	{
		struct Item { std::string key; std::string host; uint16 port; CNetAddress last; IHttpConnPoolRef pool; };
		std::vector<Item> items;
		{
			std::lock_guard<std::mutex> guard(_lock);
			items.reserve(_poolMeta.size());
			for( auto& [key, meta] : _poolMeta )
			{
				auto it = _pools.find(key);
				if( it != _pools.end() && it->second )
					items.push_back(Item{ key, meta.hostname, meta.port, meta.lastAddr, it->second });
			}
		}

		for( Item& item : items )
		{
			if( DnsStopRequested() )
				return;

			CNetAddress fresh;
			if( !_resolver || !_resolver(item.host, item.port, fresh) )
				continue; // 해석 실패 — 기존 주소를 유지한다
			if( SameAddress(fresh, item.last) )
				continue;

			item.pool->SetRemoteAddress(fresh);

			std::lock_guard<std::mutex> guard(_lock);
			auto meta = _poolMeta.find(item.key);
			if( meta != _poolMeta.end() )
				meta->second.lastAddr = fresh;
		}
	}

	static constexpr int64_t kDefaultDnsRefreshIntervalMs = 60000;
	std::atomic<int64_t> _dnsRefreshIntervalMs{ kDefaultDnsRefreshIntervalMs }; // 주소 재해석 주기(ms), 0 이하면 하지 않음
	std::mutex _dnsLock;                  // 아래 갱신 스레드 상태 보호
	std::condition_variable _dnsCv;       // 갱신 스레드의 대기/중지 신호
	std::thread _dnsThread;               // 주소 재해석 전용 스레드 (첫 풀이 만들어질 때 시작)
	bool _dnsStarted = false;
	bool _dnsStop = false;

	std::unordered_map<std::string, PoolMeta> _poolMeta; // _pools와 같은 키 — 풀을 어떻게 해석했는지 (_lock으로 보호)

	static constexpr int64_t kTimeoutNotConfigured = INT64_MIN;
	std::atomic<int64_t> _defaultTimeoutMs{ kTimeoutNotConfigured }; // SetDefaultRequestTimeout()으로 지정한 기본 타임아웃(ms)

	HttpConnPoolCreator _creator;   // host 하나에 대한 풀을 만드는 콜백
	HttpDnsResolveFn _resolver;     // hostname -> CNetAddress DNS resolve 콜백

	mutable std::mutex _lock;                                          // _pools/_creating/_closingPools/_lastKnownCounts/_poolMeta 보호
	std::unordered_map<std::string, IHttpConnPoolRef> _pools;          // "hostname:port" -> 요청을 받을 수 있는 활성 풀
	std::unordered_map<IHttpConnPool*, IHttpConnPoolRef> _closingPools; // 닫혔지만 세션이 아직 안 빠진 풀 (0 될 때까지 강한 참조로 유지)
	std::unordered_map<std::string, std::shared_future<IHttpConnPoolRef>> _creating; // 생성 중인 풀의 결과 (같은 host의 중복 생성 방지)
	std::unordered_map<IHttpConnPool*, size_t> _lastKnownCounts;        // 풀별로 마지막 반영한 세션 수 (OnPoolSessionCountChanged()의 델타 계산용)

	mutable std::mutex _countLock;                  // 아래 캐시/콜백/condition_variable 보호
	std::condition_variable _countChangedCv;        // WaitUntilAllSessionsClosed()가 대기하는 조건 변수
	size_t _cachedTotalActiveCount = 0;              // 마지막으로 계산된 전체 세션 수 합
	std::function<void()> _allSessionsClosedHandler; // 합이 0이 되는 순간 한 번 호출되고 버려지는 콜백
};

#endif // ndef UC_HTTPCONNPOOLMANAGER_H