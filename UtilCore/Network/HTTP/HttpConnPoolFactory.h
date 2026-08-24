
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

#ifndef	__HTTPSESSIONIOCP_H__
#include <Network/HTTP/HttpSessionIocp.h>
#endif

#ifndef	__HTTPSESSIONRIO_H__
#include <Network/HTTP/HttpSessionRio.h>
#endif

#ifndef	__HTTPCONNPOOL_H__
#include <Network/HTTP/HttpConnPool.h>
#endif

#ifndef	__HTTPCONNPOOLMANAGER_H__
#include <Network/HTTP/HttpConnPoolManager.h>
#endif

// CIocpClientService/CRioClientService 생성자 시그니처(CNetAddress,
// CIocpCoreRef/CRioCoreRef, SessionFactory, maxSessionCount, workerThreadCount)를
// 여기서만 알고, CHttpConnPoolT 자신은 이 시그니처를 몰라도 되게 분리한다.
using CHttpConnPoolIocp = CHttpConnPoolT<CHttpSessionIocp, CHttpSessionIocpRef, CIocpClientService, CIocpClientServiceRef>;
using CHttpConnPoolIocpRef = std::shared_ptr<CHttpConnPoolIocp>;

using CHttpConnPoolRio = CHttpConnPoolT<CHttpSessionRio, CHttpSessionRioRef, CRioClientService, CRioClientServiceRef>;
using CHttpConnPoolRioRef = std::shared_ptr<CHttpConnPoolRio>;

//***************************************************************************
// @brief IOCP 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
// @param hostAddr 접속할 원격 host 주소
// @param iocpCore 이 풀의 서비스가 사용할 IOCP 코어(공유 소유 — 여러 풀이 코어를
//        공유해도 되지만, 그럴 경우 워커 스레드도 공유됨을 감안할 것)
// @param maxConnections host당 유지할 최대 커넥션 수
// @param workerThreadCount IOCP 완료 처리용 워커 스레드 개수 (기본 1 — 클라이언트
//        권장값. NetworkFactory.h의 "[클라이언트 호출 시 권장값]" 가이드를 그대로
//        따름 — 일반 단일/소수 세션 클라이언트는 1(많아야 2) 권장. 이 풀은 host
//        하나당 서비스 인스턴스 하나이므로 기본값 1로 둠. 부하가 큰 host라면
//        호출부가 명시적으로 늘릴 것)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
inline IHttpConnPoolRef CreateHttpConnPoolIocp(CNetAddress hostAddr, CIocpCoreRef iocpCore,
	int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpConnPoolIocp::Create(maxConnections,
		[hostAddr, iocpCore, workerThreadCount](SessionFactory factory, int32 maxSessionCount)
		{
			return std::make_shared<CIocpClientService>(hostAddr, iocpCore, factory, maxSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief RIO 엔진으로 host 하나에 대한 HTTP 커넥션 풀을 만듭니다.
// @param hostAddr 접속할 원격 host 주소
// @param rioCore 이 풀의 서비스가 사용할 RIO 코어(공유 소유)
// @param maxConnections host당 유지할 최대 커넥션 수
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본 1 — 클라이언트 권장값)
// @return IHttpConnPoolRef 생성된 풀 (Start() 호출 전까지는 미구동 상태)
//***************************************************************************
inline IHttpConnPoolRef CreateHttpConnPoolRio(CNetAddress hostAddr, CRioCoreRef rioCore,
	int32 maxConnections = 8, uint32_t workerThreadCount = 1)
{
	return CHttpConnPoolRio::Create(maxConnections,
		[hostAddr, rioCore, workerThreadCount](SessionFactory factory, int32 maxSessionCount)
		{
			return std::make_shared<CRioClientService>(hostAddr, rioCore, factory, maxSessionCount, workerThreadCount);
		});
}

//***************************************************************************
// @brief IOCP 엔진으로 여러 host를 관리하는 CHttpConnPoolManager를 만듭니다.
// @param iocpCore 이 매니저가 생성하는 모든 host 풀이 공유할 IOCP 코어
// @param maxConnectionsPerHost host당 유지할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 IOCP 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저 (host별 풀은
//         첫 SendRequest() 시점에 지연 생성됨 — HttpConnPoolManager.h 참고)
//***************************************************************************
inline std::shared_ptr<CHttpConnPoolManager> CreateHttpConnPoolManagerIocp(CIocpCoreRef iocpCore,
	int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[iocpCore, maxConnectionsPerHost, workerThreadCountPerHost](CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolIocp(hostAddr, iocpCore, maxConnectionsPerHost, workerThreadCountPerHost);
		});
}

//***************************************************************************
// @brief RIO 엔진으로 여러 host를 관리하는 CHttpConnPoolManager를 만듭니다.
// @param rioCore 이 매니저가 생성하는 모든 host 풀이 공유할 RIO 코어
// @param maxConnectionsPerHost host당 유지할 최대 커넥션 수 (풀마다 동일하게 적용)
// @param workerThreadCountPerHost host 풀 하나당 RIO 워커 스레드 개수 (기본 1)
// @return std::shared_ptr<CHttpConnPoolManager> 생성된 매니저 (host별 풀은
//         첫 SendRequest() 시점에 지연 생성됨 — HttpConnPoolManager.h 참고)
//***************************************************************************
inline std::shared_ptr<CHttpConnPoolManager> CreateHttpConnPoolManagerRio(CRioCoreRef rioCore,
	int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
{
	return std::make_shared<CHttpConnPoolManager>(
		[rioCore, maxConnectionsPerHost, workerThreadCountPerHost](CNetAddress hostAddr) -> IHttpConnPoolRef
		{
			return CreateHttpConnPoolRio(hostAddr, rioCore, maxConnectionsPerHost, workerThreadCountPerHost);
		});
}

#endif // ndef __HTTPCONNPOOLFACTORY_H__
