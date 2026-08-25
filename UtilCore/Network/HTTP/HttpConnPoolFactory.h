
//***************************************************************************
// HttpConnPoolFactory.h : CHttpConnPoolT<...>를 IOCP/RIO 실제 클라이언트
// 서비스로 인스턴스화하는 팩토리 함수 모음.
//
//***************************************************************************

#ifndef __HTTPCONNPOOLFACTORY_H__
#define __HTTPCONNPOOLFACTORY_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__SESSION_H__
#include <Network/Session.h>
#endif

#ifndef	__IOCPSERVICE_H__
#include <Network/IOCP/IocpService.h>
#endif

#ifndef	__RIOSERVICE_H__
#include <Network/Rio/RioService.h>
#endif

#ifndef	__HTTPCONNPOOL_H__
#include <Network/HTTP/HttpConnPool.h>
#endif

#ifndef	__HTTPCONNPOOLMANAGER_H__
#include <Network/HTTP/HttpConnPoolManager.h>
#endif

#ifndef	__HTTPSESSIONIOCP_H__
#include <Network/HTTP/HttpSessionIocp.h>
#endif

#ifndef	__HTTPSESSIONRIO_H__
#include <Network/HTTP/HttpSessionRio.h>
#endif

#ifndef	__HTTPSSESSIONIOCP_H__
#include <Network/HTTP/HttpsSessionIocp.h>
#endif

#ifndef	__HTTPSSESSIONRIO_H__
#include <Network/HTTP/HttpsSessionRio.h>
#endif

// CIocpClientService/CRioClientService 생성자 시그니처(CNetAddress,
// CIocpCoreRef/CRioCoreRef, SessionFactory, maxSessionCount, workerThreadCount)를
// 여기서만 알고, CHttpConnPoolT 자신은 이 시그니처를 몰라도 되게 분리한다.
using CHttpConnPoolIocp = CHttpConnPoolT<CHttpSessionIocp, CHttpSessionIocpRef, CIocpClientService, CIocpClientServiceRef>;
using CHttpConnPoolIocpRef = std::shared_ptr<CHttpConnPoolIocp>;

using CHttpConnPoolRio = CHttpConnPoolT<CHttpSessionRio, CHttpSessionRioRef, CRioClientService, CRioClientServiceRef>;
using CHttpConnPoolRioRef = std::shared_ptr<CHttpConnPoolRio>;

using CHttpsConnPoolIocp = CHttpConnPoolT<CHttpsSessionIocp, CHttpsSessionIocpRef, CIocpClientService, CIocpClientServiceRef>;
using CHttpsConnPoolIocpRef = std::shared_ptr<CHttpsConnPoolIocp>;

using CHttpsConnPoolRio = CHttpConnPoolT<CHttpsSessionRio, CHttpsSessionRioRef, CRioClientService, CRioClientServiceRef>;
using CHttpsConnPoolRioRef = std::shared_ptr<CHttpsConnPoolRio>;

//***************************************************************************
// @brief IOCP 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
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
inline IHttpConnPoolRef CreateHttpConnPoolIocp(CNetAddress hostAddr, CIocpCoreRef iocpCore,
	int32 minIdle = 2, int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpConnPoolIocp::Create(minIdle, maxConnections,
		[hostAddr, iocpCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CIocpClientService>(hostAddr, iocpCore, factory, initialSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief RIO 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
// @param hostAddr 접속할 원격 host 주소
// @param rioCore 이 풀의 서비스가 사용할 RIO 코어(공유 소유)
// @param minIdle host당 항상 유지할 기준 커넥션 수 (Start() 시 이 개수만 초기 게시)
// @param maxConnections host당 허용할 최대 커넥션 수 (minIdle 초과분은 실제
//        수요가 있을 때만 TryGrow()가 만듦 — HttpConnPool.h 클래스 설명 참고)
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본 1 — 클라이언트 권장값)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
inline IHttpConnPoolRef CreateHttpConnPoolRio(CNetAddress hostAddr, CRioCoreRef rioCore,
	int32 minIdle = 2, int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpConnPoolRio::Create(minIdle, maxConnections,
		[hostAddr, rioCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CRioClientService>(hostAddr, rioCore, factory, initialSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief IOCP 엔진 + TLS로 host 하나에 대한 HTTPS 커넥션 풀을 만듭니다.
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
inline IHttpConnPoolRef CreateHttpsConnPoolIocp(CNetAddress hostAddr, const std::string& sniHostname, SSL_CTX* sslCtx,
	CIocpCoreRef iocpCore, int32 minIdle = 2, int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpsConnPoolIocp::Create(minIdle, maxConnections,
		[hostAddr, iocpCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CIocpClientService>(hostAddr, iocpCore, factory, initialSessionCount, workerThreadCount);
		},
		[sslCtx, sniHostname](std::shared_ptr<CHttpsSessionIocp> session)
		{
			// CHttpConnPoolT::Create()의 initSession 훅 — 세션 생성 직후,
			// 연결 시도(SetConnStateHandler 등록보다도 먼저) 전에 TLS 설정을 주입한다.
			session->SetTlsConfig(sslCtx, sniHostname);
		});
}

//***************************************************************************
// @brief RIO 엔진 + TLS로 host 하나에 대한 HTTPS 커넥션 풀을 만듭니다.
// @param hostAddr 접속할 원격 host 주소 (이미 DNS resolve된 IP:Port)
// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 원래 hostname 문자열
// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
// @param rioCore 이 풀의 서비스가 사용할 RIO 코어(공유 소유)
// @param minIdle host당 항상 유지할 기준 커넥션 수
// @param maxConnections host당 허용할 최대 커넥션 수
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본 1)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
inline IHttpConnPoolRef CreateHttpsConnPoolRio(CNetAddress hostAddr, const std::string& sniHostname, SSL_CTX* sslCtx,
	CRioCoreRef rioCore, int32 minIdle = 2, int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpsConnPoolRio::Create(minIdle, maxConnections,
		[hostAddr, rioCore, workerThreadCount](SessionFactory factory, int32 initialSessionCount)
		{
			return std::make_shared<CRioClientService>(hostAddr, rioCore, factory, initialSessionCount, workerThreadCount);
		},
		[sslCtx, sniHostname](std::shared_ptr<CHttpsSessionRio> session)
		{
			session->SetTlsConfig(sslCtx, sniHostname);
		});
}

//***************************************************************************
// @brief IOCP 엔진으로 여러 host를 관리하는 CHttpConnPoolManager를 만듭니다.
// @param iocpCore 이 매니저가 생성하는 모든 host 풀이 공유할 IOCP 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수 (풀마다 동일하게 적용)
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 IOCP 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저 (host별 풀은
//         첫 SendRequest() 시점에 지연 생성됨 — HttpConnPoolManager.h 참고)
//***************************************************************************
inline std::shared_ptr<CHttpConnPoolManager> CreateHttpConnPoolManagerIocp(CIocpCoreRef iocpCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost](CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolIocp(hostAddr, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		});
}

//***************************************************************************
// @brief RIO 엔진으로 여러 host를 관리하는 CHttpConnPoolManager를 만듭니다.
// @param rioCore 이 매니저가 생성하는 모든 host 풀이 공유할 RIO 코어
// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수 (풀마다 동일하게 적용)
// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 RIO 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저 (host별 풀은
//         첫 SendRequest() 시점에 지연 생성됨 — HttpConnPoolManager.h 참고)
//***************************************************************************
inline std::shared_ptr<CHttpConnPoolManager> CreateHttpConnPoolManagerRio(CRioCoreRef rioCore,
	int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost](CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolRio(hostAddr, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		});
}

// [알려진 제약] CreateHttpsConnPoolManagerIocp/Rio(다중 host HTTPS 매니저)는
// 아직 없다. CHttpConnPoolManager는 host 키를 CNetAddress(=resolve된 IP:Port)
// 로만 관리하는데, TLS SNI/인증서 검증에는 원래 hostname 문자열이 별도로
// 필요해서(CreateHttpsConnPoolIocp/Rio의 sniHostname 파라미터 참고) 지금의
// "IP만 아는" 매니저 설계로는 host별 SNI를 자연스럽게 연결할 방법이 없다.
// 필요해지면 CHttpConnPoolManager의 키를 (hostname, port)로 바꾸고 내부에서
// DNS resolve를 수행하도록 확장하거나, SendRequest() 시그니처에 sniHostname을
// 추가로 받는 방식으로 풀어야 한다 — 지금은 host 하나씩 CreateHttpsConnPoolIocp/
// Rio()로 직접 만들어 호출부가 관리하는 것만 지원한다.

#endif // ndef __HTTPCONNPOOLFACTORY_H__