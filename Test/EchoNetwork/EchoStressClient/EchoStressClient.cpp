// EchoStressClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "IocpEchoSession.h"
#include "RioEchoSession.h"

#include <iostream>
#include <conio.h>

constexpr ENetworkEngineType kTestEngineType = ENetworkEngineType::IOCP; // 또는 ENetworkEngineType::RIO

int main()
{
#ifdef	_MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    _tcout.imbue(std::locale("korean")); // 유니코드 출력 설정

    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CNetServiceRef serverService = nullptr;

    CIocpCoreRef iocpCore = nullptr;
    CRioCoreRef rioCore = nullptr;

    CVector<CNetServiceRef> clientServices;

    std::cout << "========================================\n";
    std::cout << " Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();

        // 2. IOCP 코어 Dispatch 스레드를 ThreadManager로 등록
        gpThreadManager->CreateThread([iocpCore]() {
            while( !gpThreadManager->IsShuttingDown() )
            {
                iocpCore->Dispatch();
            }
            });

        // 3. 대량의 클라이언트 서비스 생성 및 연결 시도
        const int32 TOTAL_CLIENTS = 100;
        std::cout << "[Client] Connecting " << TOTAL_CLIENTS << " clients to server...\n";

        clientServices.reserve(TOTAL_CLIENTS);

        for( int32 i = 0; i < TOTAL_CLIENTS; ++i )
        {
            CNetServiceRef clientService = CNetworkFactory::CreateClientService(
                ENetworkEngineType::IOCP, serverAddress,
                []() { return std::make_shared<CIocpClientEchoSession>(); },
                1, &iocpCore
            );

            if( clientService && clientService->Start() )
            {
                clientServices.push_back(clientService);
            }
        }
    }

    std::cout << "[Client] Connections established. Starting traffic generation...\n";

    // 4. 패킷 전송 스레드를 CThreadManager를 통해 생성
    std::atomic<bool> isRunning(true);
    std::atomic<long> totalSent(0);

    gpThreadManager->CreateThread([&clientServices, &isRunning, &totalSent]() {
        const char* msg = "Stress Test Echo Packet";
        uint16_t len = static_cast<uint16_t>(strlen(msg) + 1);

        while( isRunning )
        {
            for( auto& service : clientServices )
            {
                if( service == nullptr ) continue;
                auto session = service->GetSession(0);
                if( session && session->IsConnected() )
                {
                    session->Send(msg, len);
                    totalSent++;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        });

    // 통계 출력용 보조 스레드도 ThreadManager로 관리 가능
    gpThreadManager->CreateThread([&isRunning, &totalSent]() {
        while( isRunning )
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            std::cout << "[Monitor] Total Packets Sent: " << totalSent << "\n";
        }
        });

    std::cout << "[System] Press ESC to stop stress test and exit...\n";
    while( 1 )
    {
        if( _kbhit() )	// 키 입력이 있을 경우
        {
            char key = _getch();	// 입력된 키를 가져옴
            if( key == 27 )			// ESC 키의 ASCII 코드 : 27
            {
                std::cout << "ESC pressed. Exiting..." << std::endl;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // CPU 낭비 방지
    }

    // 5. 종료 처리
    isRunning = false;

    for( auto& service : clientServices )
    {
        if( service ) service->Close();
    }

    // 6. 전역 시스템 파괴 (관리 중인 모든 스레드 자동 Join 처리)[cite: 2, 4]
    BaseGlobal::Destroy();
    CSocketUtils::Clear();

    std::cout << "[System] Stress client terminated safely.\n";
    return 0;
}
