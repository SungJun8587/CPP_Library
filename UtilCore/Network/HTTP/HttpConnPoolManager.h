
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
#include <mutex>
#include <condition_variable>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

//***************************************************************************
// @brief hostname을 CNetAddress로 변환하는 DNS resolve 콜백.
// @return bool 성공 여부. 실패(호스트를 못 찾음 등) 시 false, outAddr은 미정의.
//***************************************************************************
using HttpDnsResolveFn = std::function<bool(const std::string& hostname, uint16_t port, CNetAddress& outAddr)>;

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
//      [_pools vs _closingPools — 조기 파괴 방지] Close()는 세션 정리를
//      "비동기로 게시"만 하고 즉시 리턴한다 — 실제 세션 정리 완료는 IOCP/RIO
//      워커 스레드가 완료 통지를 처리해야 끝난다(IHttpConnPool::
//      GetActiveSessionCount() 설명 참고). 그런데 풀 객체 자신(과 그 안의
//      클라이언트 서비스)의 C++ 수명은 그 비동기 정리와 무관하게, 마지막
//      shared_ptr이 사라지는 순간 즉시 끝난다 — 만약 CloseAll()이 풀을
//      _pools에서 지우고 로컬 변수 스코프만 벗어나게 두면, 세션이 하나도
//      안 빠졌어도 풀 객체 자체가 그 자리에서 파괴돼버려 이후로는
//      GetActiveSessionCount()를 물어볼 대상 자체가 사라진다(실제로는 세션이
//      남아있는데 "0개"로 잘못 보고하는 심각한 오검출이 생김). 그래서 이
//      매니저는 "요청을 받을 수 있는 활성 풀"(_pools)과 "닫혔지만 세션이 아직
//      다 안 빠진 풀"(_closingPools)을 분리해서, 후자는 그 풀 자신이 세션 수
//      0을 보고할 때까지 강한 참조로 붙들어 둔다 — 그제서야 진짜로 놓아준다
//      (OnPoolSessionCountChanged() 참고).
//
//      [폴링 없는 종료 대기] 각 CHttpConnPoolT는 SetSessionCountChangedHandler()
//      로 등록한 콜백을 세션 연결/해제가 있을 때마다 스스로 호출해준다 — 이
//      매니저는 그 훅을 모든 풀(활성/정리 중 가리지 않고)에 걸어두고, 호출될
//      때마다 전체 합을 다시 계산해 캐싱한 뒤 WaitUntilAllSessionsClosed()의
//      condition_variable을 깨우거나 SetAllSessionsClosedHandler()의 콜백을
//      부른다 — 별도 스레드로 주기적으로 확인하는 폴링이 전혀 없다.
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
	void SendRequest(const std::string& hostname, uint16_t port, const char* data, size_t len, HttpRequestCompletionHandler onComplete)
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

		pool->SendRequest(data, len, std::move(onComplete));
	}

	//***************************************************************************
	// @brief 캐싱된 모든 host 풀을 닫습니다. 소멸자에서도 호출되며 여러 번
	//        호출해도 안전합니다(idempotent — 두 번째 호출은 빈 맵을 순회할 뿐).
	// @details 풀 객체 자체는 세션이 실제로 다 빠질 때까지 _closingPools에
	//          남아 계속 살아있는다(GetActiveSessionCount()가 정확하게 유지되도록)
	//          — 클래스 설명의 [_pools vs _closingPools] 참고.
	//***************************************************************************
	void CloseAll()
	{
		std::vector<IHttpConnPoolRef> poolsToClose;
		{
			std::lock_guard<std::mutex> guard(_lock);
			poolsToClose.reserve(_pools.size());
			for( auto& [key, pool] : _pools )
				poolsToClose.push_back(pool);
			_pools.clear();
		}

		for( auto& pool : poolsToClose )
		{
			if( !pool )
				continue;
			TrackClosing(pool);
			pool->Close();
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
	//          정리되는 순간 즉시 깨어난다. 종료 시퀀스 예시: CHttpClient::
	//          CloseAll() 호출 후 이 함수로 대기한 뒤에야 IOCP/RIO 워커 스레드를
	//          멈추면, "세션 정리 완료 패킷보다 종료 신호가 먼저 큐에서 뽑히는"
	//          레이스를 완전히 피할 수 있다.
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
	// @details 블로킹 대기 대신 완전 비동기로 종료 흐름을 짜고 싶을 때 쓴다
	//          (예: 콜백 안에서 iocpCore->PostQuit() 호출). 등록 시점에 이미
	//          세션 수 합이 0이면 즉시(이 함수를 부른 스레드에서 동기적으로) 호출된다.
	//***************************************************************************
	void SetAllSessionsClosedHandler(std::function<void()> handler)
	{
		bool fireNow = false;
		{
			std::lock_guard<std::mutex> guard(_countLock);
			_allSessionsClosedHandler = handler;
			fireNow = (_cachedTotalActiveCount == 0);
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
	IHttpConnPoolRef GetOrCreatePool(const std::string& hostname, uint16_t port)
	{
		std::string key = MakeKey(hostname, port);

		{
			std::lock_guard<std::mutex> guard(_lock);
			auto it = _pools.find(key);
			if( it != _pools.end() )
				return it->second;
		}

		// DNS resolve + 풀 생성 + Start()는 전부 락 밖에서 수행한다(전부 시간이
		// 걸릴 수 있는 작업이라 락을 오래 쥐지 않기 위함). 그 대가로 여러 스레드가
		// 동시에 같은 host로 처음 요청하면 풀이 중복 생성될 수 있으므로, 아래
		// 재확인(double-checked) 등록 시점에 패자를 정리한다.
		CNetAddress resolvedAddr;
		if( !_resolver || !_resolver(hostname, port, resolvedAddr) )
			return nullptr;

		IHttpConnPoolRef newPool = _creator(hostname, resolvedAddr);
		if( !newPool || !newPool->Start() )
			return nullptr;

		InstallCountChangedHook(newPool);

		// 훅을 건 시점과 세션 카운트가 실제로 처음 바뀌는 시점 사이에 이미
		// 카운트가 0이 아닐 가능성을 배제하기 위해, 지금 값으로 캐시를 한 번
		// "프라이밍"한다 — Start()가 비동기라 보통은 문제없지만(연결 완료
		// 통지는 항상 이 훅이 걸린 뒤에 옴), 그 가정 자체에 기대지 않기 위한
		// 방어적 조치다.
		IHttpConnPoolRef result;
		{
			std::lock_guard<std::mutex> guard(_lock);
			auto it = _pools.find(key);
			if( it != _pools.end() )
			{
				// 다른 스레드가 이 사이에 먼저 캐시에 등록함 — 이번에 만든
				// 풀(패자)은 그대로 버리면 안 되고, 세션이 실제로 빠질 때까지
				// _closingPools로 옮겨서 정리한다(Start()로 이미 소켓/스레드
				// 리소스를 소비한 상태이므로 — 활성 풀과 동일한 조기 파괴 방지
				// 원칙이 여기도 그대로 적용됨).
				_closingPools.emplace(newPool.get(), newPool);
				newPool->Close();
				result = it->second;
			}
			else
			{
				_pools.emplace(key, newPool);
				result = newPool;
			}
		}

		// 훅을 건 시점과 세션 카운트가 실제로 처음 바뀌는 시점 사이에 이미
		// 카운트가 0이 아닐 가능성을 배제하기 위해, _pools/_closingPools 등록이
		// 끝난 뒤(= 합산 대상에 이 풀이 포함된 뒤) 지금 값으로 캐시를 한 번
		// "프라이밍"한다 — Start()가 비동기라 보통은 문제없지만(연결 완료
		// 통지는 항상 이 훅이 걸린 뒤에 옴), 그 가정 자체에 기대지 않기 위한
		// 방어적 조치다.
		OnPoolSessionCountChanged(newPool.get(), newPool->GetActiveSessionCount());

		return result;
	}

	//***************************************************************************
	// @brief hostname:port로 캐시 키 문자열을 만듭니다.
	//***************************************************************************
	static std::string MakeKey(const std::string& hostname, uint16_t port)
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
	// @param rawPool 세션 수가 바뀐 풀 (식별용 — _closingPools에서 찾을 때만 사용)
	// @param newCount 그 풀의 새 활성 세션 수
	// @details (1) 정리 중이던 풀이 방금 0이 됐으면 이제서야 _closingPools에서
	//          내보내 진짜로 파괴될 수 있게 하고, (2) 활성+정리 중 풀 전체를
	//          다시 합산해 캐시를 갱신한 뒤, (3) condition_variable을 깨우고
	//          필요하면 SetAllSessionsClosedHandler()의 콜백을 부른다.
	//***************************************************************************
	void OnPoolSessionCountChanged(IHttpConnPool* rawPool, size_t newCount)
	{
		size_t total = 0;
		{
			std::lock_guard<std::mutex> guard(_lock);

			if( newCount == 0 )
			{
				auto it = _closingPools.find(rawPool);
				if( it != _closingPools.end() )
					_closingPools.erase(it); // 여기서 마지막 강한 참조가 풀려 풀 객체가 파괴될 수 있음
			}

			for( auto& [key, pool] : _pools )
				if( pool ) total += pool->GetActiveSessionCount();
			for( auto& [key, pool] : _closingPools )
				if( pool ) total += pool->GetActiveSessionCount();
		}

		bool justReachedZero = false;
		std::function<void()> handlerCopy;
		{
			std::lock_guard<std::mutex> countGuard(_countLock);
			bool wasNonZero = (_cachedTotalActiveCount != 0);
			_cachedTotalActiveCount = total;
			if( wasNonZero && total == 0 )
			{
				justReachedZero = true;
				handlerCopy = _allSessionsClosedHandler;
			}
		}
		_countChangedCv.notify_all();

		if( justReachedZero && handlerCopy )
			handlerCopy();
	}

private:
	HttpConnPoolCreator _creator;   // host 하나에 대한 풀을 만드는 콜백
	HttpDnsResolveFn _resolver;     // hostname -> CNetAddress DNS resolve 콜백

	mutable std::mutex _lock;                                          // _pools/_closingPools 보호
	std::unordered_map<std::string, IHttpConnPoolRef> _pools;          // "hostname:port" -> 요청을 받을 수 있는 활성 풀
	std::unordered_map<IHttpConnPool*, IHttpConnPoolRef> _closingPools; // 닫혔지만 세션이 아직 안 빠진 풀 (0 될 때까지 강한 참조로 유지)

	mutable std::mutex _countLock;                  // 아래 캐시/콜백/condition_variable 보호
	std::condition_variable _countChangedCv;        // WaitUntilAllSessionsClosed()가 대기하는 조건 변수
	size_t _cachedTotalActiveCount = 0;              // 마지막으로 계산된 전체 세션 수 합
	std::function<void()> _allSessionsClosedHandler; // 세션 수 0 도달 시 1회 호출될 콜백
};

#endif // ndef UC_HTTPCONNPOOLMANAGER_H