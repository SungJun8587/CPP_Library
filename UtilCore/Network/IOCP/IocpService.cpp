
//***************************************************************************
// IocpService.cpp: implementation of the CIocpService classes.
//
//***************************************************************************

#include "pch.h"
#include "IocpService.h"

//***************************************************************************
// CIocpServerService Implementation
//***************************************************************************

//***************************************************************************
// @brief CIocpServerService 생성자 구현
// @param address 서버 바인딩 주소
// @param iocpCore IOCP 코어 객체
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
// @param workerThreadCount 워커 스레드 개수
//***************************************************************************
CIocpServerService::CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32_t workerThreadCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _iocpCore(iocpCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 서버 시작, 워커 스레드 풀 구동 및 Listener 초기화
// @return bool 구동 성공 여부
// @details
// - 1. workerThreadCount가 0인 경우 하드웨어 코어 수를 기반으로 워커 스레드 수를 자동 산정합니다.
// - 2. CThreadManager를 통해 스레드를 생성하여 DispatchBatch 루프를 구동합니다.
// - 3. CIocpListener를 생성하고 비동기 AcceptEx 작업을 시작합니다.
//***************************************************************************
bool CIocpServerService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	uint32_t workerThreadCount = _workerThreadCount;
	if( workerThreadCount == 0 )
	{
		unsigned int hwThreads = std::thread::hardware_concurrency();
		workerThreadCount = (hwThreads > 0) ? hwThreads : 2;
	}

	// 1. CThreadManager를 통해 워커 스레드 풀 구동 (자동 TLS 초기화 및 종료 감지 적용)
	for( uint32_t i = 0; i < workerThreadCount; ++i )
	{
		bool created = _threadManager.CreateThread([this]() {
			while( !_threadManager.IsShuttingDown() )
			{
				_iocpCore->DispatchBatch(10);
			}
			});

		if( !created )
		{
			Close();
			return false;
		}
	}

	// 2. Listener 생성 및 AcceptEx 개시
	_listener = MakeShared<CIocpListener>();
	if( _listener == nullptr )
	{
		Close();
		return false;
	}

	std::weak_ptr<CIocpServerService> serviceWeak = std::static_pointer_cast<CIocpServerService>(shared_from_this());

	bool result = _listener->StartAccept(
		_iocpCore,
		_address,
		[serviceWeak]() -> CIocpObjectRef
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return nullptr;

			CSessionRef session = service->CreateSession();
			return std::static_pointer_cast<CIocpSession>(session);
		},
		10,
		[serviceWeak](CIocpObjectRef session, CNetAddress netAddr)
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return;

			CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
			if( iocpSession )
			{
				iocpSession->SetNetAddress(netAddr);
				service->AddSession(iocpSession);

				uint64_t sessionId = service->GetSessionManager().GenerateSessionId();
				iocpSession->SetSessionId(sessionId);
				service->GetSessionManager().AddSession(sessionId, iocpSession);

				iocpSession->ProcessConnect();
			}
		}
	);

	if( !result )
	{
		Close();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 서버 종료 처리
// @details
// 순서:
// 1. 소속된 세션 매니저의 모든 세션 일괄 종료
// 2. Listener 소켓 닫기
// 3. PostQueuedCompletionStatus를 호출하여 대기 중인 워커 스레드들을 깨우고 Join 대기
//***************************************************************************
void CIocpServerService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	if( _listener )
	{
		_listener->CloseSocket();
		_listener = nullptr;
	}

	if( _iocpCore && _iocpCore->GetHandle() != INVALID_HANDLE_VALUE )
	{
		size_t threadCount = _threadManager.GetThreadCount();
		for( size_t i = 0; i < threadCount; ++i )
		{
			::PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, 0, nullptr);
		}
	}

	_threadManager.JoinThreads();

	CNetService::Close();
}


//***************************************************************************
// CIocpClientService Implementation
//***************************************************************************

//***************************************************************************
// @brief CIocpClientService 생성자 구현
// @param address 접속할 서버 주소
// @param iocpCore IOCP 코어 객체
// @param factory 세션 생성 팩토리
// @param maxSessionCount 생성할 세션 개수
// @param workerThreadCount 워커 스레드 개수
//***************************************************************************
CIocpClientService::CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32_t workerThreadCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _iocpCore(iocpCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
// @details Start()의 접속 루프 본체와 동일한 절차(세션 생성 → IOCP Core 등록 →
//          서비스에 즉시 등록 → ConnectEx 비동기 게시)를 그대로 수행합니다.
//
//          [비동기 전환] 과거 버전은 동기 connect()를 사용해 이 함수가 "연결까지
//          끝난 세션"을 그 자리에서 반환했습니다. 그 과정에서 connect()가
//          WSAEWOULDBLOCK을 반환해도 곧바로 ProcessConnect()를 호출해버려, TCP
//          핸드셰이크가 실제로 끝나기 전에 WSARecv를 거는 버그가 있었습니다
//          (select() 기반 유계 대기로 임시 수정했던 이력 있음). 지금은
//          CIocpSession::ConnectAsync()가 ConnectEx 기반 진짜 비동기라 그 문제
//          자체가 사라졌습니다 — 대신 이 함수의 반환값 의미가 바뀌었습니다.
//          자세한 계약은 헤더의 ConnectOneMoreSession() 주석 참고.
//
//          Register()나 CreateSession() 실패 시 로컬 shared_ptr(session/
//          iocpSession)이 스코프를 벗어나며 소멸자가 소켓을 정리하므로 별도
//          정리가 필요 없습니다. ConnectAsync() 자신의 모든 실패 경로는 내부에서
//          FailConnect()를 호출해 정리 및 OnDisconnected() 통지까지 완료합니다.
// @return CIocpSessionRef 세션 생성 + IOCP 등록 + ConnectEx 게시까지 성공하면
//         세션 참조(연결 완료 보장 아님), 그 전 단계 실패 시 nullptr.
//***************************************************************************
CIocpSessionRef CIocpClientService::ConnectOneMoreSession()
{
	if( _iocpCore == nullptr )
		return nullptr;

	CSessionRef session = CreateSession();
	if( session == nullptr )
		return nullptr;

	CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
	if( iocpSession == nullptr )
		return nullptr;

	if( _iocpCore->Register(iocpSession) == false )
		return nullptr;

	iocpSession->SetNetAddress(_address);

	// 연결 완료를 기다리지 않고 즉시 추적 목록에 등록합니다. 연결이 실패하면
	// CIocpSession::FailConnect()가 호출하는 CSession::OnDisconnected()가
	// DisconnectHandler(ReleaseSession 콜백)를 통해 자동으로 제거하므로,
	// "연결 시도 중" 세션이 목록에 남는 leak은 없습니다.
	AddSession(iocpSession);

	// ConnectAsync()의 반환값은 "게시 시도" 성공 여부일 뿐입니다 — false든 true든
	// 최종 연결 결과는 세션의 OnConnected()/OnDisconnected()로 비동기 통지됩니다.
	// 여기서는 게시 자체의 성공 여부만 보고합니다.
	if( !iocpSession->ConnectAsync(_address) )
		return nullptr;

	return iocpSession;
}

//***************************************************************************
// @brief 클라이언트 구동, 워커 스레드 시작 및 세션 IOCP 등록 + 연결 게시
// @return bool 성공 여부. [중요] true를 반환해도 각 세션의 실제 TCP 연결이
//         완료됐다는 뜻이 아닙니다 — Start()의 doc 주석 참고.
// @details
// - 1. 워커 스레드 풀을 구동하여 완료 이벤트를 처리할 준비를 합니다.
// - 2. _maxSessionCount 개수만큼 ConnectOneMoreSession()을 호출해 세션을 생성하고
//      IOCP Core에 등록한 뒤 ConnectEx 비동기 연결을 게시합니다. 세션 하나라도
//      "게시 시도" 자체가 실패하면(세션 생성/IOCP 등록 실패 등) 즉시 Close()로
//      전체를 정리하고 false를 반환합니다.
//***************************************************************************
bool CIocpClientService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	uint32_t workerThreadCount = _workerThreadCount;
	if( workerThreadCount == 0 )
	{
		unsigned int hwThreads = std::thread::hardware_concurrency();
		workerThreadCount = (hwThreads > 0) ? hwThreads : 2;
	}

	// 1. 클라이언트 워커 스레드 풀 구동
	for( uint32_t i = 0; i < workerThreadCount; ++i )
	{
		bool created = _threadManager.CreateThread([this]() {
			while( !_threadManager.IsShuttingDown() )
			{
				_iocpCore->DispatchBatch(10);
			}
			});

		if( !created )
		{
			Close();
			return false;
		}
	}

	// 2. 세션 연결 게시 (실제 절차는 ConnectOneMoreSession()에 위임)
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		if( ConnectOneMoreSession() == nullptr )
		{
			Close();
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 클라이언트 서비스 종료 처리
// @details 세션을 정리하고, 대기 중인 워커 스레드들을 깨운 뒤 Join을 수행합니다.
//***************************************************************************
void CIocpClientService::Close()
{
	if( _iocpCore && _iocpCore->GetHandle() != INVALID_HANDLE_VALUE )
	{
		size_t threadCount = _threadManager.GetThreadCount();
		for( size_t i = 0; i < threadCount; ++i )
		{
			::PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, 0, nullptr);
		}
	}

	_threadManager.JoinThreads();

	CNetService::Close();
}