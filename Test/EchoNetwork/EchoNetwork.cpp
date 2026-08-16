// EchoNetwork.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

#include "pch.h"
#include "RioEchoSession.h"
#include "IocpEchoSession.h"

constexpr ENetworkEngineType kTestEngineType = ENetworkEngineType::RIO; // 또는 ENetworkEngineType::RIO

int main()
{
	BaseGlobal::Init();
	CSocketUtils::Init();

	CNetAddress serverAddress(L"127.0.0.1", 7777);
	CNetServiceRef serverService = nullptr;
	CNetServiceRef clientService = nullptr;

	CIocpCoreRef iocpCore = nullptr;
	CRioCoreRef rioCore = nullptr;

	std::cout << "========================================\n";
	std::cout << " Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
	std::cout << "========================================\n";

	// 2. 엔진별 코어 및 서비스 생성
	if( kTestEngineType == ENetworkEngineType::IOCP )
	{
		iocpCore = std::make_shared<CIocpCore>();

		// IOCP 워커 스레드 생성 (GetQueuedCompletionStatus 폴링)
		int32 workerThreadCount = 2;
		for( int32 i = 0; i < workerThreadCount; ++i )
		{
			std::thread([iocpCore]() {
				while( true )
				{
					iocpCore->Dispatch();
				}
				}).detach();
		}

		// 서버 서비스 생성
		serverService = CNetworkFactory::CreateServerService(
			ENetworkEngineType::IOCP, serverAddress,
			[]() { return std::make_shared<CIocpEchoSession>(); },
			10, &iocpCore
		);

		// 클라이언트 서비스 생성
		clientService = CNetworkFactory::CreateClientService(
			ENetworkEngineType::IOCP, serverAddress,
			[]() { return std::make_shared<CIocpClientEchoSession>(); },
			1, &iocpCore
		);
	}
	else if( kTestEngineType == ENetworkEngineType::RIO )
	{
		rioCore = std::make_shared<CRioCore>();

		// 서버 서비스 생성 (RIO)
		serverService = CNetworkFactory::CreateServerService(
			ENetworkEngineType::RIO, serverAddress,
			[]() { return std::make_shared<CRioEchoSession>(); },
			10, &rioCore
		);

		// 클라이언트 서비스 생성 (RIO)
		clientService = CNetworkFactory::CreateClientService(
			ENetworkEngineType::RIO, serverAddress,
			[]() { return std::make_shared<CRioClientEchoSession>(); },
			1, &rioCore
		);
	}

	// 3. 서버 구동 Start
	if( serverService && serverService->Start() )
	{
		std::cout << "[Server] Echo Server started successfully on port 7777.\n";
	}
	else
	{
		std::cout << "[Error] Server Start failed!\n";
		return 1;
	}

	// 서버 안정화 대기
	::Sleep(500);

	// 4. 클라이언트 구동 및 접속 테스트
	if( clientService && clientService->Start() )
	{
		std::cout << "[Client] Client Service started. Connecting to server...\n";

		auto session = clientService->GetSession(0);

		// 세션이 연결될 때까지 대기 (최대 약 2초)
		int retryCount = 0;
		while( session && session->IsConnected() == false )
		{
			::Sleep(50);
			if( ++retryCount > 40 )
				break;
		}

		if( session && session->IsConnected() )
		{
			std::cout << "[Client] Connected to server successfully!\n";

			// 데이터 전송 테스트
			const char* msg = "Hello Echo!";
			uint16_t len = static_cast<uint16_t>(strlen(msg) + 1); // 널 종료 문자 포함

			std::cout << "[Client] Sending: " << msg << "\n";
			session->Send(msg, len);
		}
		else
		{
			std::cout << "[Client] Error: Connection failed or timed out!\n";
		}
	}

	// 5. 메인 대기 루프
	std::cout << "[System] Press Enter to shutdown server & client...\n";
	std::cin.get();

	// 6. 종료 처리
	if( clientService ) clientService->Close();
	if( serverService ) serverService->Close();

	CSocketUtils::Clear();
	std::cout << "[System] Program terminated safely.\n";

	BaseGlobal::Destroy();

	return 0;
}