
//***************************************************************************
// HttpConnPoolManager.h : interface for the CHttpConnPoolManager class.
//
//***************************************************************************

#ifndef __HTTPCONNPOOLMANAGER_H__
#define __HTTPCONNPOOLMANAGER_H__

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__NETADDRESS_H__
#include <Network/NetAddress.h>
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

//***************************************************************************
// @brief hostname을 CNetAddress(IP:Port)로 변환하는 DNS resolve 콜백.
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
//      [hostname 문자열로 키잉 — CNetAddress가 아님] 이전 설계는 CNetAddress
//      (이미 resolve된 IP)로 캐시를 키잉했는데, 그러면 HTTPS의 TLS SNI에
//      필요한 원래 hostname 문자열을 이 매니저가 알 방법이 없어서 다중 host
//      HTTPS를 지원할 수 없었다(호출부가 host 하나씩 직접 풀을 만들어야 했음).
//      지금은 hostname 문자열 자체를 키로 쓰고, DNS resolve는 이 매니저가
//      내부적으로(주입받은 HttpDnsResolveFn을 통해) 수행한다 — 그래서
//      SendRequest()가 hostname을 그대로 받고, 새 host를 처음 볼 때만 resolve한
//      뒤 결과를 캐싱된 풀과 함께 재사용한다.
//
//      [엔진 비의존] HttpConnPoolCreator/HttpDnsResolveFn 둘 다 콜백이라, 이
//      클래스 자신은 IOCP/RIO/실제 DNS API를 전혀 몰라도 된다 — mock
//      creator/resolver로 이 파일 하나만 단위 테스트할 수 있다(테스트는 이
//      파일과 함께 제공).
//
//      [지연 생성 + 캐싱] host별 풀은 그 host로 첫 SendRequest()가 올 때
//      지연 생성(lazy)되고, 이후로는 캐시에서 재사용된다. 여러 스레드가
//      동시에 같은 host로 처음 요청하면 풀이 중복 생성될 수 있는데, 그 경우
//      캐시에 먼저 등록된 쪽(승자)만 남기고 나중에 만든 쪽(패자)은 Close()로
//      정리한다(GetOrCreatePool() 참고) — resolve/풀 생성/Start() 자체를 락
//      밖에서 수행해 락 보유 시간을 짧게 유지하기 위한 트레이드오프.
//
//      [수명] 매니저가 소멸(또는 CloseAll() 호출)되면 캐싱된 모든 풀을
//      Close()한다. 개별 풀 하나만 선택적으로 닫는 기능은 없다(필요해지면
//      추가 가능). DNS 재조회(TTL 만료 등)도 하지 않는다 — 한 번 resolve한
//      hostname은 매니저 수명 동안 같은 주소로 고정된다(필요해지면 캐시 항목에
//      만료 시각을 추가하는 방향으로 확장 가능).
//***************************************************************************
class CHttpConnPoolManager
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
	//***************************************************************************
	void CloseAll()
	{
		std::unordered_map<std::string, IHttpConnPoolRef> poolsToClose;
		{
			std::lock_guard<std::mutex> guard(_lock);
			poolsToClose.swap(_pools);
		}

		for( auto& [key, pool] : poolsToClose )
		{
			if( pool )
				pool->Close();
		}
	}

	//***************************************************************************
	// @brief 현재 캐싱된 host 풀의 개수를 반환합니다 (진단/테스트용).
	//***************************************************************************
	size_t GetPoolCount() const
	{
		std::lock_guard<std::mutex> guard(_lock);
		return _pools.size();
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

		{
			std::lock_guard<std::mutex> guard(_lock);
			auto it = _pools.find(key);
			if( it != _pools.end() )
			{
				// 다른 스레드가 이 사이에 먼저 캐시에 등록함 — 이번에 만든
				// 풀(패자)은 그대로 버리면 안 되고 명시적으로 Close()해야 함
				// (Start()로 이미 소켓/스레드 리소스를 소비한 상태이므로).
				newPool->Close();
				return it->second;
			}

			_pools.emplace(key, newPool);
			return newPool;
		}
	}

	//***************************************************************************
	// @brief hostname:port로 캐시 키 문자열을 만듭니다.
	//***************************************************************************
	static std::string MakeKey(const std::string& hostname, uint16_t port)
	{
		return hostname + ":" + std::to_string(port);
	}

private:
	HttpConnPoolCreator _creator;   // host 하나에 대한 풀을 만드는 콜백
	HttpDnsResolveFn _resolver;     // hostname -> CNetAddress DNS resolve 콜백

	mutable std::mutex _lock;                          // _pools 보호
	std::unordered_map<std::string, IHttpConnPoolRef> _pools; // "hostname:port" -> 풀 캐시
};

#endif // ndef __HTTPCONNPOOLMANAGER_H__