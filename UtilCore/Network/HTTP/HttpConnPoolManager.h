
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
	// 각 멤버 함수의 상세 설명은 HttpConnPoolManager.cpp 참고.

	CHttpConnPoolManager(HttpConnPoolCreator creator, HttpDnsResolveFn resolver);
	~CHttpConnPoolManager();

	CHttpConnPoolManager(const CHttpConnPoolManager&) = delete;
	CHttpConnPoolManager& operator=(const CHttpConnPoolManager&) = delete;

	// 지정된 host로 요청을 비동기 전송한다(풀이 없으면 DNS 해석 후 생성). 실패하면 onComplete(false)를 즉시 호출한다.
	void SendRequest(const std::string& hostname, uint16 port, const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

	// 이후(그리고 이미 만들어진) 모든 host 풀의 기본 요청 타임아웃을 설정한다. 0 이하면 타임아웃 없음.
	void SetDefaultRequestTimeout(std::chrono::milliseconds timeout);

	// 호스트 주소 재해석 주기를 설정한다(기본 60초, 0 이하면 하지 않음).
	void SetDnsRefreshInterval(std::chrono::milliseconds interval);

	// 캐싱된 모든 host 풀을 닫는다(여러 번 호출해도 안전).
	void CloseAll();

	// 요청을 받을 수 있는(정리 중이 아닌) host 풀의 개수.
	size_t GetPoolCount() const;

	// 활성 풀 + 정리 중인 풀의 세션 수 합계(통지로 갱신되는 캐시값).
	size_t GetTotalActiveSessionCount() const;

	// 세션 수 합이 0이 될 때까지(또는 timeout까지) 블로킹 대기한다.
	void WaitUntilAllSessionsClosed();
	bool WaitUntilAllSessionsClosed(std::chrono::milliseconds timeout);

	// 세션 수 합이 0에 도달하는 순간 한 번 호출되는 콜백을 등록한다(이미 0이면 즉시 호출).
	void SetAllSessionsClosedHandler(std::function<void()> handler);

private:
	struct PoolMeta
	{
		std::string hostname;
		uint16 port = 0;
		CNetAddress lastAddr;   // 풀이 지금 쓰고 있는(마지막으로 알린) 주소
	};

	// --- 풀 생성/추적 ---
	IHttpConnPoolRef GetOrCreatePool(const std::string& hostname, uint16 port);
	IHttpConnPoolRef CreateAndStartPool(const std::string& hostname, uint16 port, CNetAddress& outResolvedAddr);
	static std::string MakeKey(const std::string& hostname, uint16 port);
	void InstallCountChangedHook(const IHttpConnPoolRef& pool);
	void TrackClosing(const IHttpConnPoolRef& pool);
	void OnPoolSessionCountChanged(IHttpConnPool* rawPool, size_t newCount);

	// --- 주소 재해석 ---
	static bool SameAddress(const CNetAddress& a, const CNetAddress& b);
	void EnsureDnsRefresher();
	void StopDnsRefresher();
	bool DnsStopRequested();
	void DnsRefreshLoop();
	void RefreshAddressesOnce();

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