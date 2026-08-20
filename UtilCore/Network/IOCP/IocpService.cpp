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
// @brief 클라이언트 구동, 워커 스레드 시작 및 세션 IOCP 등록
// @return bool 성공 여부
// @details
// - 1. 워커 스레드 풀을 구동하여 완료 이벤트를 처리할 준비를 합니다.
// - 2. 요청된 수만큼 세션을 생성하고 IOCP Core에 등록한 뒤 원격 서버와 연결을 시도합니다.
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

	// 2. 세션 생성 및 IOCP 등록 후 접속 시도
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		CSessionRef session = CreateSession();
		if( session == nullptr )
		{
			Close();
			return false;
		}

		CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
		if( iocpSession == nullptr )
			return false;

		if( _iocpCore->Register(iocpSession) == false )
		{
			Close();
			return false;
		}

		SOCKET sock = iocpSession->GetSocket();
		SOCKADDR_IN sockAddr = _address.GetSockAddr();
		if( ::connect(sock, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr)) == SOCKET_ERROR )
		{
			int32 err = ::WSAGetLastError();
			if( err != WSAEWOULDBLOCK && err != WSA_IO_PENDING )
			{
				Close();
				return false;
			}
		}

		iocpSession->SetNetAddress(_address);
		iocpSession->ProcessConnect();
		AddSession(iocpSession);
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