// EchoServer.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "IocpEchoServerSession.h"
#include "RioEchoServerSession.h"

#include <iostream>
#include <conio.h>

constexpr ENetworkEngineType kTestEngineType = ENetworkEngineType::IOCP; // 또는 ENetworkEngineType::RIO

int main()
{
#ifdef	_MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    _tcout.imbue(std::locale("korean")); // 유니코드 출력 설정

    // 1. 전역 시스템 초기화 (gpThreadManager 및 gpMemory 생성)
    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CNetServiceRef serverService = nullptr;
 
    CIocpCoreRef iocpCore = nullptr;
    CRioCoreRef rioCore = nullptr;

    std::cout << "========================================\n";
    std::cout << " Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();

        // 2. CThreadManager를 이용한 IOCP 워커 스레드 생성
        //    (스레드 시작/종료 시 자동으로 InitTLS()와 DestroyTLS()가 호출됩니다)
        int32 workerThreadCount = 4;
        for( int32 i = 0; i < workerThreadCount; ++i )
        {
            gpThreadManager->CreateThread([iocpCore]() {
                while( !gpThreadManager->IsShuttingDown() )
                {
                    iocpCore->Dispatch();
                }
                });
        }

        serverService = CNetworkFactory::CreateServerService(
            ENetworkEngineType::IOCP, serverAddress,
            []() { return std::make_shared<CIocpEchoServerSession>(); },
            2000, &iocpCore
        );
    }

    if( serverService && serverService->Start() )
    {
        std::cout << "[Server] Started successfully with CThreadManager.\n";
    }
    else
    {
        std::cout << "[Error] Server Start failed!\n";
        BaseGlobal::Destroy();
        return 1;
    }

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

    // 3. 종료 처리
    std::cout << "[System] Closing server service...\n";
    if( serverService ) serverService->Close();

    // 4. 전역 시스템 파괴
    //    - BaseGlobal::Destroy 내부에서 gpThreadManager가 먼저 파괴되며 모든 워커 스레드를 안전하게 Join합니다[cite: 4].
    //    - 메인 스레드의 TLS 캐시 정리 및 메모리 풀 해제가 올바른 순서로 진행됩니다[cite: 4].
    std::cout << "[System] Calling BaseGlobal::Destroy()...\n";
    BaseGlobal::Destroy();
    std::cout << "[System] BaseGlobal::Destroy() finished.\n";

    CSocketUtils::Clear();

    std::cout << "[System] Server terminated safely.\n";

    system("pause");

    return 0;
}

