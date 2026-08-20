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

#ifdef _WIN32
    // 1. 환경 설정 및 초기화
    // 1-1. C 런타임 로케일 설정 (printf, scanf 등 C 스타일 입출력 및 문자열 처리 함수가 UTF-8을 인식하도록 지정)
    setlocale(LC_ALL, ".UTF8");

    // 1-2. 콘솔 입출력 코드페이지를 UTF-8(65001)로 변경
    SetConsoleOutputCP(CP_UTF8);	// 출력 텍스트(std::cout 등)의 유니코드/한글 깨짐 방지
    SetConsoleCP(CP_UTF8);			// 입력 텍스트(std::cin 등)의 UTF-8 인코딩 읽기 보장
#endif

    // 1-3. 전역 시스템 초기화
    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CIocpCoreRef iocpCore = nullptr;
    CRioCoreRef rioCore = nullptr;

    std::cout << "========================================\n";
    std::cout << " [ECHO CLIENT] Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    // 2. 네트워크 엔진 및 워커 스레드 구성
    // - 클라이언트 워커 스레드 개수 설정 가이드:
    //   > 1개(또는 소수) 세션 테스트: 1 또는 2 설정 (자원 낭비 및 불필요한 컨텍스트 스위칭 방지)
    //   > 수백~수천 개 세션 부하/스트레스 테스트: 0 설정 (하드웨어 코어 수 기반 자동 병렬 처리)
    uint32_t clientThreadWorkerCount = 1;

    // 3. 엔진별 코어 객체 및 클라이언트 서비스 생성
    std::cout << "[Client] Connecting to server (127.0.0.1:7777)...\n";

    CNetServiceRef clientService = nullptr;
    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();

        // 3-1. IOCP 클라이언트 서비스 생성 (인자 순서: 엔진타입, 주소, 팩토리, 최대세션수, 워커스레드수, 코어참조)
        clientService = CNetworkFactory::CreateClientService(
            ENetworkEngineType::IOCP, serverAddress,
            []() { return std::make_shared<CIocpEchoClientSession>(); },
            1, clientThreadWorkerCount, &iocpCore
        );
    }
    else if( kTestEngineType == ENetworkEngineType::RIO )
    {
        rioCore = std::make_shared<CRioCore>();

        // 3-2. RIO 클라이언트 서비스 생성
        clientService = CNetworkFactory::CreateClientService(
            ENetworkEngineType::RIO, serverAddress,
            []() { return std::make_shared<CRioEchoClientSession>(); },
            1, clientThreadWorkerCount, &rioCore
        );
    }

    // 4. 클라이언트 서비스 구동 시작
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

    // 5. 서버 세션 연결 대기
    // 5-1. 세션 연결 완료 대기 (최대 약 2초)
    auto session = std::static_pointer_cast<CIocpEchoClientSession>(clientService->GetSession(0));
    int retryCount = 0;
    while( session && session->IsConnected() == false )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if( ++retryCount > 40 )
            break;
    }

    // 5-2. 연결 실패 시 예외 처리 및 안전 종료
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

    // 6. 사용자 입력 및 에코 메시지 송수신 루프
    while( true )
    {
        std::cout << "\n[Client Send] > ";
        std::string input;
        std::getline(std::cin, input);

        // 6-1. 종료 조건 검사 (exit 명령어 또는 ESC 등)
        if( input == "exit" || input == "ESC" )
        {
            std::cout << "Disconnecting...\n";
            break;
        }

        if( input.empty() ) continue;

        if( session && session->IsConnected() )
        {
            // 6-2. 에코 응답 대기 상태 활성화
            session->SetWaitingForEcho(true);

            // 6-3. 메시지 서버로 전송
            uint16_t len = static_cast<uint16_t>(input.length() + 1);
            session->Send(input.c_str(), len);

            // 6-4. 서버로부터 응답(OnRecv)을 수신할 때까지 대기 (타임아웃 적용)
            int timeoutCount = 0;
            while( session->IsWaitingForEcho() && timeoutCount < 40 )
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                timeoutCount++;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        else
        {
            std::cout << "[Error] Connection lost!\n";
            break;
        }
    }

    // 7. 안전한 프로그램 종료 및 자원 해제
    // 7-1. 클라이언트 서비스 종료 처리
    if( clientService ) clientService->Close();

    // 7-2. 전역 시스템 및 소켓 유틸리티 정리
    BaseGlobal::Destroy();
    CSocketUtils::Clear();

    std::cout << "[System] Client terminated safely.\n";
    return 0;
}