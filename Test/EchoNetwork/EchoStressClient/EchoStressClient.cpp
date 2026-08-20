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

    // 1. 환경 설정 및 초기화
    // 1-1. 유니코드 출력 로케일 설정
    _tcout.imbue(std::locale("korean"));

    // 1-2. 전역 시스템 초기화
    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CIocpCoreRef iocpCore = nullptr;
    CRioCoreRef rioCore = nullptr;

    CVector<CNetServiceRef> clientServices;

    std::cout << "========================================\n";
    std::cout << " Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    // 2. 네트워크 엔진 코어 객체 생성
    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();
    }
    else if( kTestEngineType == ENetworkEngineType::RIO )
    {
        rioCore = std::make_shared<CRioCore>();
    }

    // 3. 대량의 클라이언트 서비스 생성 및 연결 시도
    // - 대규모 스트레스 테스트이므로 워커 스레드 수는 0으로 설정하여 하드웨어 코어 수 기반 자동 병렬 처리 활용
    const int32 TOTAL_CLIENTS = 100;
    const uint32_t stressWorkerCount = 0;

    std::cout << "[Client] Connecting " << TOTAL_CLIENTS << " clients to server...\n";
    clientServices.reserve(TOTAL_CLIENTS);

    for( int32 i = 0; i < TOTAL_CLIENTS; ++i )
    {
        CNetServiceRef clientService = nullptr;

        if( kTestEngineType == ENetworkEngineType::IOCP )
        {
            // 3-1. IOCP 클라이언트 서비스 생성 (인자 순서: 엔진타입, 주소, 팩토리, 최대세션수, 워커스레드수, 코어참조)
            clientService = CNetworkFactory::CreateClientService(
                ENetworkEngineType::IOCP, serverAddress,
                []() { return std::make_shared<CIocpEchoClientSession>(); },
                1, stressWorkerCount, &iocpCore
            );
        }
        else if( kTestEngineType == ENetworkEngineType::RIO )
        {
            // 3-2. RIO 클라이언트 서비스 생성
            clientService = CNetworkFactory::CreateClientService(
                ENetworkEngineType::RIO, serverAddress,
                []() { return std::make_shared<CRioEchoClientSession>(); },
                1, stressWorkerCount, &rioCore
            );
        }

        if( clientService && clientService->Start() )
        {
            clientServices.push_back(clientService);
        }
    }

    std::cout << "[Client] Connections established. Starting traffic generation...\n";

    // 4. 트래픽 생성 및 모니터링 스레드 등록 (gpThreadManager 활용)
    std::atomic<bool> isRunning(true);
    std::atomic<long> totalSent(0);

    // 4-1. 패킷 전송 스레드 생성
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

    // 4-2. 통계 모니터링 스레드 생성
    gpThreadManager->CreateThread([&isRunning, &totalSent]() {
        while( isRunning )
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            std::cout << "[Monitor] Total Packets Sent: " << totalSent << "\n";
        }
        });

    // 5. 메인 대기 루프 (ESC 키 입력 시 스트레스 테스트 종료)
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

    // 6. 안전한 프로그램 종료 및 자원 해제
    // 6-1. 전송 루프 중지 신호 전달
    isRunning = false;

    // 6-2. 모든 클라이언트 서비스 종료 처리
    for( auto& service : clientServices )
    {
        if( service ) service->Close();
    }

    // 6-3. 전역 시스템 파괴 (관리 중인 모든 스레드 및 스레드 매니저 자동 Join 처리)
    BaseGlobal::Destroy();
    CSocketUtils::Clear();

    std::cout << "[System] Stress client terminated safely.\n";
    return 0;
}