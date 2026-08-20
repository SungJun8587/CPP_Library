//***************************************************************************
// IocpService.h : interface for the CIocpServerService & CIocpClientService.
//
//***************************************************************************

#ifndef __IOCPSERVICE_H__
#define __IOCPSERVICE_H__

#ifndef __NET_SERVICE_H__
#include <Network/NetService.h>
#endif

#ifndef __IOCPCORE_H__
#include <Network/IOCP/IocpCore.h>
#endif

#ifndef __IOCPLISTENER_H__
#include <Network/IOCP/IocpListener.h>
#endif

#ifndef __IOCPSESSIONMANAGER_H__
#include <Network/IOCP/IocpSessionManager.h>
#endif

#ifndef __THREADMANAGER_H__
#include <ThreadManager.h>
#endif

//***************************************************************************
// @class CIocpServerService
// @brief IOCP 기반의 서버 전용 서비스 클래스
// @details
// 역할:
//      1. CIocpListener를 내부에 두고 비동기 AcceptEx 작업을 총괄 제어
//      2. CThreadManager를 활용하여 IOCP 워커 스레드 풀 생성 및 관리
//      3. CIocpSessionManager를 직접 소유하여 접속된 세션 관리
//***************************************************************************
class CIocpServerService : public CNetService
{
public:
	//***************************************************************************
	// @brief CIocpServerService 생성자
	// @param address 서버 바인딩 주소
	// @param iocpCore 연동할 IOCP 코어 객체
	// @param factory 세션 생성 팩토리
	// @param maxSessionCount 최대 동시 접속 수 (기본값: 1)
	// @param workerThreadCount IOCP 완료 처리용 워커 스레드 개수 (기본값: 0 = 하드웨어 코어 수 기반 자동 산정)
	//***************************************************************************
	CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32_t workerThreadCount = 0);

	//***************************************************************************
	// @brief CIocpServerService 소멸자
	//***************************************************************************
	virtual ~CIocpServerService() = default;

	//***************************************************************************
	// @brief IOCP 서버 구동 (워커 스레드 풀 시작, Listener 생성 및 AcceptEx 개시)
	// @return bool 모든 초기화 및 구동 성공 시 true, 실패 시 false
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief 서버 서비스 종료 (세션 일괄 종료, Listener 소켓 정리, 워커 스레드 Join)
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 소속된 IOCP Core 객체를 반환합니다.
	// @return CIocpCoreRef IOCP Core 참조
	//***************************************************************************
	CIocpCoreRef	GetIocpCore() const { return _iocpCore; }

	//***************************************************************************
	// @brief 소속된 세션 매니저 참조를 반환합니다.
	// @return CIocpSessionManager& 세션 매니저 객체 참조
	//***************************************************************************
	CIocpSessionManager& GetSessionManager() { return _sessionManager; }

private:
	CIocpCoreRef			_iocpCore = nullptr;    // 연동된 IOCP 코어 객체 참조
	CIocpListenerRef		_listener = nullptr;    // 클라이언트 접속 수락 리스너
	CIocpSessionManager		_sessionManager;        // 서버 서비스가 직접 소유하는 세션 매니저
	uint32_t				_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
	CThreadManager			_threadManager;         // 워커 스레드 수명 주기 및 TLS 관리자
};

//***************************************************************************
// @class CIocpClientService
// @brief IOCP 기반의 클라이언트 전용 서비스 클래스
// @details
// 역할:
//      1. CThreadManager를 통해 워커 스레드 풀 생성 및 관리
//      2. 지정된 개수만큼 세션을 생성하고 IOCP Core에 등록 후 원격 서버 접속 준비
//***************************************************************************
class CIocpClientService : public CNetService
{
public:
	//***************************************************************************
	// @brief CIocpClientService 생성자
	// @param address 접속할 원격 서버 주소
	// @param iocpCore 연동할 IOCP 코어 객체
	// @param factory 세션 생성 팩토리
	// @param maxSessionCount 생성할 세션 개수 (기본값: 1)
	// @param workerThreadCount IOCP 완료 처리용 워커 스레드 개수 (기본값: 0 = 자동 산정)
	//***************************************************************************
	CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32_t workerThreadCount = 0);

	//***************************************************************************
	// @brief CIocpClientService 소멸자
	//***************************************************************************
	virtual ~CIocpClientService() = default;

	//***************************************************************************
	// @brief IOCP 클라이언트 구동 (워커 스레드 시작, 세션 생성 및 IOCP 등록, 접속 시도)
	// @return bool 구동 성공 여부
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief 클라이언트 서비스 종료 (세션 해제, 워커 스레드 Join)
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 소속된 IOCP Core 객체를 반환합니다.
	// @return CIocpCoreRef IOCP Core 참조
	//***************************************************************************
	CIocpCoreRef	GetIocpCore() const { return _iocpCore; }

private:
	CIocpCoreRef			_iocpCore = nullptr;    // 연동된 IOCP 코어 객체 참조
	uint32_t				_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
	CThreadManager			_threadManager;         // 워커 스레드 수명 주기 및 TLS 관리자
};

#endif // ndef __IOCPSERVICE_H__