
//***************************************************************************
// HttpConnPoolFactory.h : CHttpConnPoolT<...>를 IOCP/RIO 실제 클라이언트
// 서비스로 인스턴스화하는 팩토리 함수 모음.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOLFACTORY_H
#define UC_HTTPCONNPOOLFACTORY_H

#include <Network/NetworkRedefineDataType.h>
#include <Network/Session.h>
#include <Network/IOCP/IocpService.h>
#include <Network/Rio/RioService.h>
#include <Network/HTTP/HttpConnPool.h>
#include <Network/HTTP/HttpConnPoolManager.h>
#include <Network/HTTP/HttpSessionIocp.h>
#include <Network/HTTP/HttpSessionRio.h>
#include <Network/SocketUtils.h>

#include <list>
#include <memory>

// CIocpClientService/CRioClientService 생성자 시그니처(CNetAddress,
// CIocpCoreRef/CRioCoreRef, SessionFactory, maxSessionCount, workerThreadCount)를
// 여기서만 알고, CHttpConnPoolT 자신은 이 시그니처를 몰라도 되게 분리한다.
//
// [SessionType 템플릿 파라미터] 아래 Create* 함수들은 전부 SessionType을
// 템플릿 파라미터로 받는다(기본값 CHttpSessionIocp/CHttpSessionRio) — 호출부가
// CHttpSessionIocp/CHttpSessionRio를 상속한 커스텀 세션(예: OnConnected()/
// OnRecv()를 오버라이드해 로깅을 추가하거나 멤버를 더 갖는 세션)을 대신 꽂아
// 넣을 수 있게 하기 위함이다(HttpSessionIocp.h/HttpSessionRio.h가 더 이상
// final이 아닌 이유도 이것). 커스텀 SessionType은 CHttpSessionIocp/Rio와 같은
// 인터페이스(SetConnStateHandler/SendRequest/IsConnectionCloseRequested/
// GetHttpState, HTTPS 풀을 쓰려면 SetTlsConfig까지)를 가져야 한다 — 덕타이핑이라
// 만족하지 못하면 템플릿 인스턴스화 시점에 컴파일 에러로 드러난다.
// 기본값을 지정해뒀으므로 기존 호출부(SessionType을 명시하지 않는 코드)는
// 전부 그대로 동작한다.
using CHttpConnPoolIocp = CHttpConnPoolT<CHttpSessionIocp, CHttpSessionIocpRef, CIocpClientService, CIocpClientServiceRef>;
using CHttpConnPoolIocpRef = std::shared_ptr<CHttpConnPoolIocp>;

using CHttpConnPoolRio = CHttpConnPoolT<CHttpSessionRio, CHttpSessionRioRef, CRioClientService, CRioClientServiceRef>;
using CHttpConnPoolRioRef = std::shared_ptr<CHttpConnPoolRio>;

//***************************************************************************
// @brief hostname을 CNetAddress로 변환하는 기본 DNS resolve 함수 (CSocketUtils::
//        GetSockAddrIn 기반). CHttpConnPoolManager 생성 시 HttpDnsResolveFn으로
//        그대로 넘기면 된다.
// @param hostname 조회할 hostname (DNS 이름은 RFC 1035상 항상 ASCII이므로,
//        UNICODE 빌드에서도 바이트를 그대로 TCHAR로 widen하는 게 안전하다 —
//        임의의 유니코드 텍스트일 수 있는 폼 필드 값과는 다른 경우.)
// @param port 조회할 포트
// @param outAddr [OUT] 조회 성공 시 채워지는 주소 (IPv4 첫 결과만 사용)
// @return bool 성공 여부
//***************************************************************************
inline bool ResolveHostnameToNetAddress(const std::string& hostname, uint16 port, CNetAddress& outAddr)
{
	_tstring tHostname;
	tHostname.reserve(hostname.size());
	for( char c : hostname )
		tHostname.push_back(static_cast<TCHAR>(static_cast<unsigned char>(c)));

	std::list<addrinfo> results;
	if( !CSocketUtils::GetSockAddrIn(tHostname.c_str(), static_cast<int>(port), results) )
		return false;

	for( const addrinfo& info : results )
	{
		if( info.ai_family == AF_INET && info.ai_addr != nullptr )
		{
			SOCKADDR_IN sockAddr = *reinterpret_cast<SOCKADDR_IN*>(info.ai_addr);
			outAddr = CNetAddress(sockAddr);
			return true;
		}
	}

	return false;
}

//***************************************************************************
// @brief IOCP 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp — 커스텀
//         세션을 쓰려면 CHttpSessionIocp를 상속한 클래스를 명시적으로 지정)
// @param hostAddr 접속할 원격 host 주소
// @param iocpCore 이 풀의 서비스가 사용할 IOCP 코어(공유 소유 — 여러 풀이 코어를
//        공유해도 되지만, 그럴 경우 워커 스레드도 공유됨을 감안할 것)
// @param minIdle host당 항상 유지할 기준 커넥션 수 (Start() 시 이 개수만 초기 게시)
// @param maxConnections host당 허용할 최대 커넥션 수 (minIdle 초과분은 실제
//        수요가 있을 때만 TryGrow()가 만듦 — HttpConnPool.h 클래스 설명 참고)
// @param workerThreadCount IOCP 완료 처리용 워커 스레드 개수 (기본 1 — 클라이언트
//        권장값. NetworkFactory.h의 "[클라이언트 호출 시 권장값]" 가이드를 그대로
//        따름 — 일반 단일/소수 세션 클라이언트는 1(많아야 2) 권장. 이 풀은 host
//        하나당 서비스 인스턴스 하나이므로 기본값 1로 둠. 부하가 큰 host라면
//        호출부가 명시적으로 늘릴 것)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
template<typename SessionType = CHttpSessionIocp>
inline IHttpConnPoolRef CreateHttpConnPoolIocp(CNetAddress hostAddr, CIocpCoreRef iocpCore,
	int32 minIdle = 2, int32 maxConnections = 8, uint32 workerThreadCount = 1)
{
	using PoolType = CHttpConnPoolT<SessionType, std::shared_ptr<SessionType>, CIocpClientService, CIocpClientServiceRef>;

	return PoolType::Create(minIdle, maxConnections,
		[hostAddr, iocpCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CIocpClientService>(hostAddr, iocpCore, factory, initialSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief RIO 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
// @param hostAddr 접속할 원격 host 주소
// @param rioCore 이 풀의 서비스가 사용할 RIO 코어(공유 소유)
// @param minIdle host당 항상 유지할 기준 커넥션 수 (Start() 시 이 개수만 초기 게시)
// @param maxConnections host당 허용할 최대 커넥션 수 (minIdle 초과분은 실제
//        수요가 있을 때만 TryGrow()가 만듦 — HttpConnPool.h 클래스 설명 참고)
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본 1 — 클라이언트 권장값)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
template<typename SessionType = CHttpSessionRio>
inline IHttpConnPoolRef CreateHttpConnPoolRio(CNetAddress hostAddr, CRioCoreRef rioCore,
	int32 minIdle = 2, int32 maxConnections = 8, uint32 workerThreadCount = 1)
{
	using PoolType = CHttpConnPoolT<SessionType, std::shared_ptr<SessionType>, CRioClientService, CRioClientServiceRef>;

	return PoolType::Create(minIdle, maxConnections,
		[hostAddr, rioCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CRioClientService>(hostAddr, rioCore, factory, initialSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief IOCP 엔진 + TLS로 host 하나에 대한 HTTPS 커넥션 풀을 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp). 커스텀
//         타입을 쓰려면 SetTlsConfig(SSL_CTX*, const std::string&)를 상속
//         또는 직접 제공해야 한다.
// @param hostAddr 접속할 원격 host 주소 (이미 DNS resolve된 IP:Port)
// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 원래 hostname
//        문자열(예: "api.example.com") — hostAddr은 resolve된 IP라 SNI에는
//        쓸 수 없어 별도로 받는다.
// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (CreateDefaultClientSslCtx()로
//        생성 권장). 호출부가 소유권을 유지하며, 이 함수가 반환한 풀보다
//        오래 살아있어야 한다(각 세션이 내부적으로 참조만 하고 소유하지 않음).
// @param iocpCore 이 풀의 서비스가 사용할 IOCP 코어(공유 소유)
// @param minIdle host당 항상 유지할 기준 커넥션 수
// @param maxConnections host당 허용할 최대 커넥션 수
// @param workerThreadCount IOCP 완료 처리용 워커 스레드 개수 (기본 1)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
template<typename SessionType = CHttpSessionIocp>
inline IHttpConnPoolRef CreateHttpsConnPoolIocp(CNetAddress hostAddr, const std::string& sniHostname, SSL_CTX* sslCtx,
	CIocpCoreRef iocpCore, int32 minIdle = 2, int32 maxConnections = 8, uint32 workerThreadCount = 1)
{
	using PoolType = CHttpConnPoolT<SessionType, std::shared_ptr<SessionType>, CIocpClientService, CIocpClientServiceRef>;

	return PoolType::Create(minIdle, maxConnections,
		[hostAddr, iocpCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CIocpClientService>(hostAddr, iocpCore, factory, initialSessionCount, workerThreadCount);
		},
		[sslCtx, sniHostname](std::shared_ptr<SessionType> session)
		{
			// CHttpConnPoolT::Create()의 initSession 훅 — 세션 생성 직후,
			// 연결 시도(SetConnStateHandler 등록보다도 먼저) 전에 TLS 설정을 주입한다.
			session->SetTlsConfig(sslCtx, sniHostname);
		});
}

//***************************************************************************
// @brief RIO 엔진 + TLS로 host 하나에 대한 HTTPS 커넥션 풀을 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
// @param hostAddr 접속할 원격 host 주소 (이미 DNS resolve된 IP:Port)
// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 원래 hostname 문자열
// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
// @param rioCore 이 풀의 서비스가 사용할 RIO 코어(공유 소유)
// @param minIdle host당 항상 유지할 기준 커넥션 수
// @param maxConnections host당 허용할 최대 커넥션 수
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본 1)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
template<typename SessionType = CHttpSessionRio>
inline IHttpConnPoolRef CreateHttpsConnPoolRio(CNetAddress hostAddr, const std::string& sniHostname, SSL_CTX* sslCtx,
	CRioCoreRef rioCore, int32 minIdle = 2, int32 maxConnections = 8, uint32 workerThreadCount = 1)
{
	using PoolType = CHttpConnPoolT<SessionType, std::shared_ptr<SessionType>, CRioClientService, CRioClientServiceRef>;

	return PoolType::Create(minIdle, maxConnections,
		[hostAddr, rioCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CRioClientService>(hostAddr, rioCore, factory, initialSessionCount, workerThreadCount);
		},
		[sslCtx, sniHostname](std::shared_ptr<SessionType> session)
		{
			session->SetTlsConfig(sslCtx, sniHostname);
		});
}

//***************************************************************************
// @brief IOCP 엔진으로 여러 host(평문 HTTP)를 관리하는 CHttpConnPoolManager를 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp)
// @param iocpCore 이 매니저가 생성하는 모든 host 풀이 공유할 IOCP 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수 (풀마다 동일하게 적용)
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 IOCP 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저 (host별 풀은
//         첫 SendRequest() 시점에 지연 생성됨 — HttpConnPoolManager.h 참고)
//***************************************************************************
template<typename SessionType = CHttpSessionIocp>
inline CHttpConnPoolManagerRef CreateHttpConnPoolManagerIocp(CIocpCoreRef iocpCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost](const std::string&, CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolIocp<SessionType>(hostAddr, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		},
		&ResolveHostnameToNetAddress);
}

//***************************************************************************
// @brief RIO 엔진으로 여러 host(평문 HTTP)를 관리하는 CHttpConnPoolManager를 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
// @param rioCore 이 매니저가 생성하는 모든 host 풀이 공유할 RIO 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수 (풀마다 동일하게 적용)
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 RIO 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저
//***************************************************************************
template<typename SessionType = CHttpSessionRio>
inline CHttpConnPoolManagerRef CreateHttpConnPoolManagerRio(CRioCoreRef rioCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost](const std::string&, CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolRio<SessionType>(hostAddr, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		},
		&ResolveHostnameToNetAddress);
}

//***************************************************************************
// @brief IOCP 엔진 + TLS로 여러 host(HTTPS)를 관리하는 CHttpConnPoolManager를 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp)
// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지, 매니저보다
//        오래 살아있어야 함)
// @param iocpCore 이 매니저가 생성하는 모든 host 풀이 공유할 IOCP 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수
// @param workerThreadCountPerHost host 풀 하나당 IOCP 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저. hostname 문자열로
//         키잉하므로(HttpConnPoolManager.h 참고) 여러 host에 대해 각자 올바른
//         SNI로 TLS 연결이 이뤄진다.
//***************************************************************************
template<typename SessionType = CHttpSessionIocp>
inline CHttpConnPoolManagerRef CreateHttpsConnPoolManagerIocp(SSL_CTX* sslCtx, CIocpCoreRef iocpCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[sslCtx, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost]
		(const std::string& hostname, CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpsConnPoolIocp<SessionType>(hostAddr, hostname, sslCtx, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		},
		&ResolveHostnameToNetAddress);
}

//***************************************************************************
// @brief RIO 엔진 + TLS로 여러 host(HTTPS)를 관리하는 CHttpConnPoolManager를 만듭니다.
// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
// @param rioCore 이 매니저가 생성하는 모든 host 풀이 공유할 RIO 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수
// @param workerThreadCountPerHost host 풀 하나당 RIO 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저
//***************************************************************************
template<typename SessionType = CHttpSessionRio>
inline CHttpConnPoolManagerRef CreateHttpsConnPoolManagerRio(SSL_CTX* sslCtx, CRioCoreRef rioCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[sslCtx, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost]
		(const std::string& hostname, CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpsConnPoolRio<SessionType>(hostAddr, hostname, sslCtx, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		},
		&ResolveHostnameToNetAddress);
}

#endif // ndef UC_HTTPCONNPOOLFACTORY_H