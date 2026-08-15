
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
// @param iocpCore IOCP 코어 참조
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
//***************************************************************************
CIocpServerService::CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief 서버 구동 및 Listener 초기화
// @return bool 서버 가동 성공 여부
// @note IocpSessionFactory와 OnAcceptCallback 람다를 작성할 때 스마트 포인터 업/다운 캐스팅을 수행합니다.
//***************************************************************************
bool CIocpServerService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	_listener = MakeShared<CIocpListener>();
	if( _listener == nullptr )
		return false;

	CIocpServerServiceRef service = std::static_pointer_cast<CIocpServerService>(shared_from_this());

	// Listener 구동 시작
	bool result = _listener->StartAccept(
		_iocpCore,
		_address,
		//***************************************************************************
		// @brief IocpSessionFactory 람다 구현
		// @return CIocpObjectRef 생성된 세션의 CIocpObject 업캐스팅 객체
		//***************************************************************************
		[service]() -> CIocpObjectRef
		{
			CSessionRef session = service->CreateSession();
			return std::static_pointer_cast<CIocpSession>(session);
		},
		10, // AcceptEx 사전 발주 개수
		//***************************************************************************
		// @brief OnAcceptCallback 람다 구현
		// @param session Accept 완료 통지된 CIocpObject 객체
		// @param netAddr 클라이언트 원격 네트워크 주소
		//***************************************************************************
		[service](CIocpObjectRef session, CNetAddress netAddr)
		{
			CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
			if( iocpSession )
			{
				iocpSession->SetNetAddress(netAddr);
				service->AddSession(iocpSession);
				iocpSession->ProcessConnect();
			}
		}
	);

	return result;
}

//***************************************************************************
// @brief 서버 종료 처리
// @note Listener 소켓을 먼저 닫아 신규 연결 수신을 중단하고 부모 Close를 호출합니다.
//***************************************************************************
void CIocpServerService::Close()
{
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
// @param address 접속 대상 주소
// @param iocpCore IOCP 코어 참조
// @param factory 세션 생성 팩토리
// @param maxSessionCount 생성할 세션 개수
//***************************************************************************
CIocpClientService::CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief 클라이언트 구동 및 세션 IOCP 등록
// @return bool 시작 성공 여부
// @note _maxSessionCount 수만큼 세션을 할당 후 IOCP Core에 Register 합니다.
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

		// IOCP Core에 소켓 핸들 등록
		if( _iocpCore->Register(iocpSession) == false )
			return false;

		AddSession(iocpSession);
	}

	return true;
}