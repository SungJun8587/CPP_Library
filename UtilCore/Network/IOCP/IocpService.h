
//***************************************************************************
// IocpService.h : interface for the CIocpServerService & CIocpClientService.
//
//***************************************************************************

#ifndef UC_IOCPSERVICE_H
#define UC_IOCPSERVICE_H

#include <Network/NetService.h>
#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpListener.h>
#include <Network/IOCP/IocpSessionManager.h>
#include <Thread/ThreadManager.h>

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
	CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

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
	uint32				_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
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
	CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

	//***************************************************************************
	// @brief CIocpClientService 소멸자
	//***************************************************************************
	virtual ~CIocpClientService() = default;

	//***************************************************************************
	// @brief IOCP 클라이언트 구동 (워커 스레드 시작, 세션 생성 및 ConnectEx 비동기 접속 게시)
	// @return bool _maxSessionCount개 세션 전부의 연결 "게시"가 성공하면 true.
	//         [중요] ConnectAsync()가 진짜 비동기(ConnectEx)로 바뀌면서, 이 함수가
	//         true를 반환해도 실제 TCP 연결이 전부(혹은 하나라도) 완료됐다는 보장이
	//         없습니다 — 연결 성공/실패는 각 세션의 OnConnected()/OnDisconnected()
	//         오버라이드로 나중에 비동기 통지됩니다. Start()는 오직 "N개 세션 생성 +
	//         IOCP 등록 + ConnectEx 게시"까지만 동기로 보장하고 리턴합니다(과거
	//         버전은 동기 connect() 기반이라 true 반환 = 실제 연결 완료를 의미했으나
	//         이제는 아닙니다). 세션 하나를 연결 게시하는 실제 절차는
	//         ConnectOneMoreSession()에 있습니다.
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

	//***************************************************************************
	// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
	// @details Start()의 접속 루프가 세션 하나당 수행하는 것과 동일한 절차(세션
	//          생성 → IOCP Core 등록 → 서비스에 즉시 등록 → ConnectEx 비동기 게시)를
	//          재사용 가능한 단위로 분리한 것입니다. 커넥션 풀의 재연결 로직 등,
	//          서비스 기동 이후 세션을 개별적으로 보충해야 하는 호출부를 위해 존재하며
	//          _maxSessionCount 상한 체크는 하지 않으므로 호출자가 책임집니다.
	//
	//          [중요 — 반환값의 의미가 "연결 완료"가 아님] CIocpSession::ConnectAsync()가
	//          ConnectEx 기반 진짜 비동기이기 때문에, 이 함수는 "세션을 만들고 연결
	//          시도를 게시하는 데까지 성공했는지"만 동기로 알려줍니다. 실제 TCP 연결
	//          성공/실패는 반환된 세션의 OnConnected()/OnDisconnected() 오버라이드로
	//          나중에 비동기 통지됩니다 — 호출부(예: HTTP 커넥션 풀)는 반환된
	//          CIocpSessionRef를 즉시 "사용 가능한 커넥션"으로 취급해서는 안 되고,
	//          세션 서브클래스가 OnConnected()/OnDisconnected()를 오버라이드해 그
	//          결과를 콜백 등으로 상위에 알리는 방식으로 연동해야 합니다.
	//
	//          세션은 ConnectEx 게시 이전에 이미 AddSession()으로 서비스의 추적
	//          목록에 들어갑니다(과거 버전은 연결 성공 후에만 등록했으나, 비동기
	//          전환으로 "연결 시도 중"인 세션도 추적할 필요가 있어짐). 연결이 실패하면
	//          CIocpSession::FailConnect()가 호출하는 CSession::OnDisconnected() →
	//          DisconnectHandler → CNetService::ReleaseSession() 경로로 자동
	//          제거되므로, 실패한 세션이 목록에 남는 leak은 없습니다.
	// @return CIocpSessionRef 세션 생성 + IOCP 등록 + ConnectEx 게시 "시도" 자체가
	//         전부 성공하면 세션 참조(단, 위 설명대로 아직 연결 완료 보장 아님),
	//         그 전 단계(세션 생성, IOCP 등록)에서 실패하면 nullptr.
	//***************************************************************************
	CIocpSessionRef	ConnectOneMoreSession();

private:
	CIocpCoreRef			_iocpCore = nullptr;    // 연동된 IOCP 코어 객체 참조
	uint32				_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
	CThreadManager			_threadManager;         // 워커 스레드 수명 주기 및 TLS 관리자
};

#endif // ndef UC_IOCPSERVICE_H