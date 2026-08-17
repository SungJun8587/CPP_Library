// EchoClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "IocpEchoClientSession.h"
#include "RioEchoClientSession.h"

#include <iostream>
#include <conio.h>

constexpr ENetworkEngineType kTestEngineType = ENetworkEngineType::IOCP; // 또는 ENetworkEngineType::RIO

int main()
{
#ifdef	_MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // 1. 콘솔을 UTF-8 모드로 설정
    system("chcp 65001 > nul");

    // 2. wcout이 UTF-8 변환을 할 수 있도록 로케일 설정 (.65001은 UTF-8 의미)
    std::wcout.imbue(std::locale(".65001"));

    // 3. 전역 시스템 초기화
    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CIocpCoreRef iocpCore = nullptr;

    std::cout << "========================================\n";
    std::cout << " [ECHO CLIENT] Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    // 4. 엔진별 코어 및 IOCP Dispatch 스레드 구동 (CThreadManager 활용)
    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();

        gpThreadManager->CreateThread([iocpCore]() {
            while( !gpThreadManager->IsShuttingDown() )
            {
                iocpCore->Dispatch();
            }
            });
    }

    // 5. 단일 클라이언트 서비스 생성 및 서버 접속 시도
    std::cout << "[Client] Connecting to server (127.0.0.1:7777)...\n";

    CNetServiceRef clientService = nullptr;
    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        clientService = CNetworkFactory::CreateClientService(
            ENetworkEngineType::IOCP, serverAddress,
            []() { return std::make_shared<CIocpEchoClientSession>(); },
            1, &iocpCore
        );
    }

    if( clientService && clientService->Start() )
    {
        std::cout << "[Client] Client Service started successfully.\n";
    }
    else
    {
        std::cout << "[Error] Failed to start client service!\n";
        BaseGlobal::Destroy();
        return 1;
    }

    // 6. 세션 연결 대기 (최대 약 2초)
    auto session = std::static_pointer_cast<CIocpEchoClientSession>(clientService->GetSession(0));
    int retryCount = 0;
    while( session && session->IsConnected() == false )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if( ++retryCount > 40 )
            break;
    }

    if( session == nullptr || session->IsConnected() == false )
    {
        std::cout << "[Error] Could not connect to the server!\n";
        if( clientService ) clientService->Close();
        BaseGlobal::Destroy();
        CSocketUtils::Clear();
        return 1;
    }

    std::cout << "[Client] Connected to server successfully!\n";
    std::cout << "========================================\n";
    std::cout << " Instructions:\n";
    std::cout << " - Type your message and press Enter to send.\n";
    std::cout << " - Type 'exit' or press ESC to quit.\n";
    std::cout << "========================================\n";

    // 7. 사용자 입력 및 메시지 전송 루프
    while( true )
    {
        std::cout << "\n[Client Send] > ";
        std::string input;
        std::getline(std::cin, input);

        // 종료 조건 검사 (exit 명령어 또는 비어있는 입력 중 특정 처리 등)
        if( input == "exit" || input == "ESC" )
        {
            std::cout << "Disconnecting...\n";
            break;
        }

        if( input.empty() ) continue;

        if( session && session->IsConnected() )
        {
            // 1. 응답 대기 상태로 설정
            session->SetWaitingForEcho(true);

            // 2. 메시지 전송
            // 널 종료 문자 포함해서 전송 (엔진 규격에 따라 다를 수 있으므로 길이는 기존 방식 유지)
            uint16_t len = static_cast<uint16_t>(input.length() + 1);
            session->Send(input.c_str(), len);

            // 3. 서버로부터 응답(OnRecv)이 와서 g_waitingForEcho가 false가 될 때까지 대기
            // (무한 대기 방지를 위해 타임아웃 1~2초 정도 추가)
            int timeoutCount = 0;
            while( session->IsWaitingForEcho() && timeoutCount < 40 )
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                timeoutCount++;
            }

            // 응답이 너무 늦거나 끊긴 경우 대비용 딜레이 (로그 출력 순서가 꼬이는 것 방지)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        else
        {
            std::cout << "[Error] Connection lost!\n";
            break;
        }
    }

    // 8. 안전한 종료 처리
    if( clientService ) clientService->Close();

    // BaseGlobal::Destroy 내부에서 gpThreadManager가 소멸되며 워커 스레드를 안전하게 Join함
    BaseGlobal::Destroy();
    CSocketUtils::Clear();

    std::cout << "[System] Client terminated safely.\n";
    return 0;
}

