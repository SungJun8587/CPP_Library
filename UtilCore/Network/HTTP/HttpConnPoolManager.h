
//***************************************************************************
// HttpConnPoolManager.h : interface for the CHttpConnPoolManager class.
//
//***************************************************************************

#ifndef __HTTPCONNPOOLMANAGER_H__
#define __HTTPCONNPOOLMANAGER_H__

#ifndef	__NETADDRESS_H__
#include <Network/NetAddress.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

//***************************************************************************
// @brief CNetAddress(IP/Port) 하나에 대한 CHttpConnPoolT<...> 인스턴스를 만드는
//        콜백. 실제 CIocpClientService/CRioClientService 생성자 호출은 이
//        콜백 안(HttpConnPoolFactory.h의 CreateHttpConnPoolIocp/Rio)에
//        캡슐화돼 있어, CHttpConnPoolManager 자신은 엔진(IOCP/RIO) 시그니처를
//        전혀 몰라도 된다.
//***************************************************************************
using HttpConnPoolCreator = std::function<IHttpConnPoolRef(CNetAddress)>;

//***************************************************************************
// @struct HostKey
// @brief CHttpConnPoolManager가 host별 풀을 캐싱할 때 쓰는 키 (IP + Port).
// @details 문자열(호스트네임)이 아니라 CNetAddress(이미 resolve된 IP)를 키로
//          쓴다 — 이 매니저는 DNS 조회를 하지 않으므로 호출부가 이미 resolve된
//          주소를 넘겨야 한다. TCHAR/_tstring 변환을 거치지 않고 SOCKADDR_IN의
//          원시 필드(sin_addr.s_addr, 포트)를 그대로 키로 써서 문자열 비교/
//          변환 비용 없이 빠르게 비교한다.
//***************************************************************************
struct HostKey
{
	uint32_t ip;   // CNetAddress::GetSockAddr().sin_addr.s_addr (네트워크 바이트 오더 그대로, 키 용도라 순서 무관)
	uint16_t port; // CNetAddress::GetPort() (호스트 바이트 오더)

	bool operator==(const HostKey& other) const noexcept
	{
		return ip == other.ip && port == other.port;
	}
};

//***************************************************************************
// @struct HostKeyHash
// @brief std::unordered_map<HostKey, ...>용 해시 함수 객체.
//***************************************************************************
struct HostKeyHash
{
	size_t operator()(const HostKey& key) const noexcept
	{
		return (static_cast<size_t>(key.ip) << 16) ^ static_cast<size_t>(key.port);
	}
};

//***************************************************************************
// @class CHttpConnPoolManager
// @brief host(IP:Port) 여러 개를 관리하는 상위 HTTP 커넥션 풀 매니저
//
// @details
//      CHttpConnPoolT<...>는 host 하나만 다루는 단위다. 실제 사용 시에는
//      "동일 프로세스에서 여러 외부 API/내부 서버로 HTTP 요청을 보낸다"가
//      일반적인 시나리오라, host별로 CHttpConnPoolT 인스턴스를 필요할 때 만들고
//      캐싱해서 재사용하는 이 상위 계층이 필요하다.
//
//      [엔진 비의존] 생성자는 HttpConnPoolCreator 콜백만 받는다 — 실제
//      CIocpClientService/CRioClientService 생성자 호출은 이 콜백 안에
//      캡슐화돼 있어(HttpConnPoolFactory.h의 CreateHttpConnPoolIocp/Rio가
//      만들어주는 콜백을 넘겨받음), 이 클래스 자신은 어느 엔진을 쓰는지 전혀
//      몰라도 된다. 덕분에 실제 IOCP/RIO 네트워크 스택 없이도 mock creator로
//      이 파일 하나만 단위 테스트할 수 있다.
//
//      [지연 생성 + 캐싱] host별 풀은 그 host로 첫 SendRequest()가 올 때
//      지연 생성(lazy)되고, 이후로는 캐시에서 재사용된다. 여러 스레드가
//      동시에 같은 host로 처음 요청하면 풀이 중복 생성될 수 있는데, 그 경우
//      캐시에 먼저 등록된 쪽(승자)만 남기고 나중에 만든 쪽(패자)은 Close()로
//      정리한다(GetOrCreatePool() 참고) — 풀 생성/Start() 자체를 락 밖에서
//      수행해 락 보유 시간을 짧게 유지하기 위한 트레이드오프.
//
//      [수명] 매니저가 소멸(또는 CloseAll() 호출)되면 캐싱된 모든 풀을
//      Close()한다. 개별 풀 하나만 선택적으로 닫는 기능은 없다(필요해지면
//      추가 가능).
//***************************************************************************
class CHttpConnPoolManager
{
public:
	//***************************************************************************
	// @brief CHttpConnPoolManager 생성자
	// @param creator host 하나에 대한 풀을 만드는 콜백 (HttpConnPoolFactory.h의
	//        CreateHttpConnPoolIocp/Rio 등으로 만들어 넘기면 됨)
	//***************************************************************************
	explicit CHttpConnPoolManager(HttpConnPoolCreator creator)
		: _creator(std::move(creator))
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
	// @param hostAddr 목적지 주소 (이미 DNS resolve된 IP:Port — 이 매니저는
	//        DNS 조회를 하지 않는다)
	// @param data 요청 패킷 바이트
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @details 해당 host의 풀이 없으면 새로 만들고 Start()한다. 풀 생성/Start()
	//          자체가 실패하면(예: 소켓 리소스 고갈) onComplete를 즉시
	//          success=false로 호출한다.
	//***************************************************************************
	void SendRequest(CNetAddress hostAddr, const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		IHttpConnPoolRef pool = GetOrCreatePool(hostAddr);
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
		std::unordered_map<HostKey, IHttpConnPoolRef, HostKeyHash> poolsToClose;
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
	// @brief hostAddr에 해당하는 풀을 찾거나, 없으면 새로 만들어 캐시에 등록합니다.
	// @param hostAddr 목적지 주소
	// @return IHttpConnPoolRef 찾았거나 새로 만든 풀. 생성/Start() 실패 시 nullptr.
	//***************************************************************************
	IHttpConnPoolRef GetOrCreatePool(CNetAddress hostAddr)
	{
		HostKey key = MakeKey(hostAddr);

		{
			std::lock_guard<std::mutex> guard(_lock);
			auto it = _pools.find(key);
			if( it != _pools.end() )
				return it->second;
		}

		// 풀 생성 + Start()는 락 밖에서 수행한다(소켓 연결 게시 등 시간이 걸릴
		// 수 있는 작업이라 락을 오래 쥐지 않기 위함). 그 대가로 여러 스레드가
		// 동시에 같은 host로 처음 요청하면 풀이 중복 생성될 수 있으므로, 아래
		// 재확인(double-checked) 등록 시점에 패자를 정리한다.
		IHttpConnPoolRef newPool = _creator(hostAddr);
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
	// @brief CNetAddress로부터 캐시 키(IP+Port)를 만듭니다.
	//***************************************************************************
	static HostKey MakeKey(CNetAddress hostAddr)
	{
		return HostKey{ hostAddr.GetSockAddr().sin_addr.s_addr, hostAddr.GetPort() };
	}

private:
	HttpConnPoolCreator _creator; // host 하나에 대한 풀을 만드는 콜백 (엔진별 생성자 호출을 캡슐화)

	mutable std::mutex _lock;                                              // _pools 보호
	std::unordered_map<HostKey, IHttpConnPoolRef, HostKeyHash> _pools;     // host별 풀 캐시
};

#endif // ndef __HTTPCONNPOOLMANAGER_H__