
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
//***************************************************************************
CIocpServerService::CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief 서버 시작 및 Listener 초기화
// @return bool 성공 여부 성공 여부 반환
// @note 람다 캡처 시 순환 참조를 방지하기 위해 weak_ptr을 활용합니다.
//***************************************************************************
bool CIocpServerService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	_listener = MakeShared<CIocpListener>();
	if( _listener == nullptr )
		return false;

	// 순환 참조 방지를 위해 shared_from_this()를 weak_ptr로 전환
	std::weak_ptr<CIocpServerService> serviceWeak = std::static_pointer_cast<CIocpServerService>(shared_from_this());

	// Listener 시작 설정
	bool result = _listener->StartAccept(
		_iocpCore,
		_address,
		//***************************************************************************
		// @brief IocpSessionFactory 람다 설정
		// @return CIocpObjectRef 생성된 세션을 CIocpObject 캐스팅한 객체
		//***************************************************************************
		[serviceWeak]() -> CIocpObjectRef
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return nullptr;

			CSessionRef session = service->CreateSession();
			return std::static_pointer_cast<CIocpSession>(session);
		},
		10, // AcceptEx 동시 대기 갯수
		//***************************************************************************
		// @brief OnAcceptCallback 람다 설정
		// @param session Accept 완료된 CIocpObject 객체
		// @param netAddr 클라이언트 접속 네트워크 주소
		//***************************************************************************
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

				// 서비스가 소유한 SessionManager에 SessionId 발급 및 등록
				uint64_t sessionId = service->GetSessionManager().GenerateSessionId();
				iocpSession->SetSessionId(sessionId);
				service->GetSessionManager().AddSession(sessionId, iocpSession);

				iocpSession->ProcessConnect();
			}
		}
	);

	return result;
}

//***************************************************************************
// @brief 서버 종료 처리
// @note 소속된 세션 매니저의 모든 세션을 일괄 종료하고 Listener 소켓을 닫습니다.
//***************************************************************************
void CIocpServerService::Close()
{
	// 소속된 세션 매니저의 모든 세션 연결 해제
	_sessionManager.BeginCloseAllSessions();

	if( _listener )
	{
		_listener->CloseSocket();
		_listener = nullptr;
	}

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
//***************************************************************************
CIocpClientService::CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief 클라이언트 구동 및 세션 IOCP 등록
// @return bool 성공 여부 성공
// @note _maxSessionCount 만큼 세션을 할당 후 IOCP Core에 Register 합니다.
//***************************************************************************
bool CIocpClientService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		CSessionRef session = CreateSession();
		if( session == nullptr )
			return false;

		CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
		if( iocpSession == nullptr )
			return false;

		// IOCP Core에 세션 강제 등록
		if( _iocpCore->Register(iocpSession) == false )
			return false;

		// 클라이언트 소켓에 대상 주소(_address)로 접속 시도 (Connect)
		SOCKET sock = iocpSession->GetSocket();

		SOCKADDR_IN sockAddr = _address.GetSockAddr();
		if( ::connect(sock, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr)) == SOCKET_ERROR )
		{
			int32 err = ::WSAGetLastError();
			if( err != WSAEWOULDBLOCK && err != WSA_IO_PENDING )
			{
				return false;
			}
		}

		// 연결 설정 완료 처리
		iocpSession->SetNetAddress(_address);
		iocpSession->ProcessConnect();

		AddSession(iocpSession);
	}

	return true;
}