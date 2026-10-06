
//***************************************************************************
// IocpService.h : interface for the CIocpServerService & CIocpClientService.
//
//***************************************************************************

#ifndef UC_IOCPSERVICE_H
#define UC_IOCPSERVICE_H

#include <Network/NetService.h>
#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpListener.h>
#include <Network/IOCP/IocpSession.h>
#include <Network/IOCP/IocpSessionManager.h>
#include <Network/IOCP/IocpWorkerPool.h>
#include <Thread/SyncValue.h>
#include <Thread/ThreadManager.h>
#include <Containers/Queue/DelayedTaskQueue.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

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
	CIocpServerService(const CNetAddress& address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

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

	//***************************************************************************
	// @brief 동시 접속 수 상한(maxSessionCount) 적용 여부를 설정합니다 (기본값: false).
	// @param enforce true면 동시 접속 수가 maxSessionCount에 도달한 뒤의 신규 연결을
	//        수락 직후 닫습니다(세션 생성/콜백 없이 소켓만 닫힘, OnConnected/OnDisconnected 미호출).
	// @details 기본값이 false인 이유: 생성자/CNetworkFactory의 maxSessionCount 기본값이 1이라,
	//          값을 지정하지 않고 생성한 서버에 상한을 기본 적용하면 동시 접속 1명으로 제한됩니다.
	//          maxSessionCount를 실제 상한으로 지정한 서버에서만 켜십시오. Start() 이전 호출을 권장하며,
	//          이후 호출해도 이후 수락되는 연결부터 적용됩니다. 거부 로그는 첫 건과 1000건마다 남깁니다.
	//***************************************************************************
	void			SetEnforceMaxSessionCount(bool enforce) noexcept { _enforceMaxSessionCount.store(enforce, std::memory_order_relaxed); }
	bool			IsEnforceMaxSessionCount() const noexcept { return _enforceMaxSessionCount.load(std::memory_order_relaxed); }

	//***************************************************************************
	// @brief 현재 수락되어 해제 통지 전인 동시 접속 수를 반환합니다.
	//***************************************************************************
	int32			GetActiveSessionCount() const noexcept { return _activeSessionCount.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 동시에 게시해 둘 AcceptEx 개수를 설정합니다 (기본값: Iocp::kDefaultAcceptPoolSize).
	// @details 반드시 Start() 이전에 호출해야 하며, 이후 호출은 무시됩니다.
	//***************************************************************************
	void			SetAcceptPoolSize(uint32 poolSize) noexcept { if( _listener == nullptr && poolSize > 0 ) _acceptPoolSize = poolSize; }

	//***************************************************************************
	// @brief Listen 소켓의 주소 바인딩 방식을 지정합니다 (기본값: ReuseAddress, 기존 동작). Start() 이전에만 유효합니다.
	// @details ExclusiveAddrUse로 바꾸면 같은 포트로 서버를 이중 기동할 때 두 번째 기동이 "주소 사용 중"으로 실패한다.
	//          다만 재시작 동작이 달라질 수 있으니(ListenAddressMode 설명 참고) 운영 환경에서 재시작을 확인한 뒤 쓴다.
	//***************************************************************************
	void			SetListenAddressMode(ListenAddressMode mode) noexcept { if( _listener == nullptr ) _listenAddressMode = mode; }

	//***************************************************************************
	// @brief 접속하는 모든 세션에 적용할 TCP 소켓 옵션(TCP_NODELAY, keep-alive)을 지정합니다 (기본값: 지정 안 함 = 세션 기본값).
	// @details 지정하면 세션의 ProcessConnect() 직전에 각 세션의 SetSocketOptions()로 전달된다. 이후 접속하는
	//          세션부터 적용된다.
	//***************************************************************************
	void			SetSessionSocketOptions(const CIocpSession::SocketOptions& options)
	{
		_socketOptions.Set(options);
	}

	bool			GetSessionSocketOptions(CIocpSession::SocketOptions& options) const
	{
		const std::optional<CIocpSession::SocketOptions> current = _socketOptions.Get();
		if( !current )
			return false;

		options = *current;
		return true;
	}

private:
	//***************************************************************************
	// @brief _sessionManager에 다음 reap tick을 예약합니다(self-rescheduling).
	// @details 세션은 해제 통지 시점에 OnSessionDisconnected()가 즉시 _sessionManager에서
	//          제거하며, 이 reap은 그 경로를 놓친 엔트리를 정리하는 안전망이다.
	//          CDelayedTaskQueue::Reserve()가 일회성이라 이 함수가 실행될 때마다 자기 자신을
	//          다시 예약하는 self-rescheduling 패턴을 쓴다. Close()가 _sessionReapQueue.Stop()을
	//          호출하면 그 이후의 재예약 시도는 Reserve()가 false를 반환하며 조용히 무시되어
	//          재귀가 자연스럽게 끊긴다. CRioServerService::ScheduleSessionReap()과 동일한 설계이며,
	//          CIocpClientService는 CIocpSessionManager를 소유하지 않으므로(ConnectOneMoreSession()이
	//          CNetService::_sessions만 씀) 서버 전용이다.
	//***************************************************************************
	void ScheduleSessionReap();

	//***************************************************************************
	// @brief 세션 해제 통지(OnDisconnected) 시 호출되는 서버 전용 핸들러입니다.
	// @param session 해제된 세션
	// @details 세션 매니저와 서비스의 추적 목록(_sessions)에서 해당 세션을 즉시 제거하고,
	//          동시 접속 수를 되돌립니다. 수락 콜백이 세션 ID를 부여한 세션(= 동시 접속
	//          슬롯을 예약한 세션)만 슬롯을 반환합니다.
	//***************************************************************************
	void OnSessionDisconnected(CSessionRef session);

	CDelayedTaskQueue	_sessionReapQueue;						// reap tick 예약 큐 (스스로 워커 스레드를 안 가짐)
	std::thread			_sessionReapThread;						// _sessionReapQueue.ProcessExpiredTasks()를 실행하는 전용 스레드

private:
	CIocpCoreRef			_iocpCore = nullptr;    // 연동된 IOCP 코어 객체 참조
	CIocpListenerRef		_listener = nullptr;    // 클라이언트 접속 수락 리스너
	CIocpSessionManager		_sessionManager;        // 서버 서비스가 직접 소유하는 세션 매니저
	uint32					_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
	CIocpWorkerPool			_workers;              // IOCP 워커 스레드 풀
	uint32					_acceptPoolSize = Iocp::kDefaultAcceptPoolSize; // 동시에 게시할 AcceptEx 개수
	ListenAddressMode		_listenAddressMode = ListenAddressMode::ReuseAddress; // Listen 소켓 주소 바인딩 방식
	CSyncValue<std::optional<CIocpSession::SocketOptions>> _socketOptions;  // 세션에 적용할 TCP 옵션 (지정 전에는 비어 있음 = 세션 기본값)
	std::atomic<bool>		_enforceMaxSessionCount{ false };               // maxSessionCount를 실제 상한으로 적용할지 여부
	std::atomic<int32>		_activeSessionCount{ 0 };                       // 수락되어 해제 통지 전인 동시 접속 수
	std::atomic<uint64>		_rejectedSessionCount{ 0 };                     // 상한 초과로 거부한 연결 누계 (로그 샘플링용)
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
	CIocpClientService(const CNetAddress& address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

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

	//***************************************************************************
	// @brief 앞으로 만드는 연결이 접속할 원격 주소를 바꿉니다 (스레드 안전).
	// @details 이미 연결됐거나 연결 중인 세션은 영향받지 않고, 이후의 ConnectOneMoreSession()부터 새 주소를
	//          씁니다. DNS 재해석 결과를 반영하는 용도입니다. GetNetAddress()는 생성 시 주소를 그대로 반환하므로
	//          현재 접속 대상은 GetRemoteAddress()로 확인합니다.
	//***************************************************************************
	void			SetRemoteAddress(const CNetAddress& address)
	{
		_remoteAddress.Set(address);
	}

	CNetAddress		GetRemoteAddress() const
	{
		return _remoteAddress.Get();
	}

	//***************************************************************************
	// @brief 이 서비스가 만드는 모든 연결 세션에 적용할 TCP 소켓 옵션(TCP_NODELAY, keep-alive)을 지정합니다.
	//        기본값은 지정 안 함(= 세션 자신의 기본값). 이후 ConnectOneMoreSession()부터 적용된다.
	//***************************************************************************
	void			SetSessionSocketOptions(const CIocpSession::SocketOptions& options)
	{
		_socketOptions.Set(options);
	}

	bool			GetSessionSocketOptions(CIocpSession::SocketOptions& options) const
	{
		const std::optional<CIocpSession::SocketOptions> current = _socketOptions.Get();
		if( !current )
			return false;

		options = *current;
		return true;
	}

private:
	CIocpCoreRef			_iocpCore = nullptr;    // 연동된 IOCP 코어 객체 참조
	uint32					_workerThreadCount = 0; // 구동할 IOCP 워커 스레드 개수 (0=자동)
	CSyncValue<CNetAddress>	_remoteAddress;   // 새 연결이 접속할 원격 주소 (생성 시 address로 초기화, SetRemoteAddress()는 다른 스레드에서 호출될 수 있음)
	CSyncValue<std::optional<CIocpSession::SocketOptions>> _socketOptions;  // 세션에 적용할 TCP 옵션 (지정 전에는 비어 있음 = 세션 기본값)
	CIocpWorkerPool			_workers;              // IOCP 워커 스레드 풀
};

#endif // ndef UC_IOCPSERVICE_H