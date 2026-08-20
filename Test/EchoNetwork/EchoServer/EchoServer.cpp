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

#ifdef _WIN32
    // 1. 환경 설정 및 초기화
    // 1-1. C 런타임 로케일 설정 (printf, scanf 등 C 스타일 입출력 및 문자열 처리 함수가 UTF-8을 인식하도록 지정)
    setlocale(LC_ALL, ".UTF8");

    // 1-2. 콘솔 입출력 코드페이지를 UTF-8(65001)로 변경
    SetConsoleOutputCP(CP_UTF8);	// 출력 텍스트(std::cout 등)의 유니코드/한글 깨짐 방지
    SetConsoleCP(CP_UTF8);			// 입력 텍스트(std::cin 등)의 UTF-8 인코딩 읽기 보장
#endif

    // 1-3. 전역 시스템 초기화 (gpThreadManager 및 gpMemory 생성)
    BaseGlobal::Init();
    CSocketUtils::Init();

    CNetAddress serverAddress(L"127.0.0.1", 7777);
    CNetServiceRef serverService = nullptr;

    CIocpCoreRef iocpCore = nullptr;
    CRioCoreRef rioCore = nullptr;

    std::cout << "========================================\n";
    std::cout << " Network Engine: " << (kTestEngineType == ENetworkEngineType::RIO ? "RIO (Registered I/O)" : "IOCP") << "\n";
    std::cout << "========================================\n";

    // 2. 네트워크 엔진 및 서버 서비스 구성
    // - 서버 워커 스레드 개수 설정 가이드 (0 입력 시 하드웨어 코어 수 기반 자동 산정)
    uint32_t serverThreadWorkerCount = 2;

    // 3. 엔진별 코어 객체 및 서버 서비스 생성
    if( kTestEngineType == ENetworkEngineType::IOCP )
    {
        iocpCore = std::make_shared<CIocpCore>();

        // 3-1. IOCP 서버 서비스 생성 (인자 순서: 엔진타입, 주소, 팩토리, 최대세션수, 워커스레드수, 코어참조)
        serverService = CNetworkFactory::CreateServerService(
            ENetworkEngineType::IOCP, serverAddress,
            []() { return std::make_shared<CIocpEchoServerSession>(); },
            10, serverThreadWorkerCount, &iocpCore
        );
    }
    else if( kTestEngineType == ENetworkEngineType::RIO )
    {
        rioCore = std::make_shared<CRioCore>();

        // 3-2. RIO 서버 서비스 생성
        serverService = CNetworkFactory::CreateServerService(
            ENetworkEngineType::RIO, serverAddress,
            []() { return std::make_shared<CRioEchoServerSession>(); },
            10, serverThreadWorkerCount, &rioCore
        );
    }

    // 4. 서버 서비스 구동 시작
    if( serverService && serverService->Start() )
    {
        std::cout << "[Server] Echo Server started successfully on port 7777 (CThreadManager active).\n";
    }
    else
    {
        std::cout << "[Error] Server Start failed!\n";
        BaseGlobal::Destroy();
        return 1;
    }

    // 5. 서버 메인 대기 루프 (ESC 키 입력 시 종료)
    std::cout << "[System] Press ESC to stop the server and exit...\n";
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
    // 6-1. 서버 서비스 종료 처리
    std::cout << "[System] Closing server service...\n";
    if( serverService ) serverService->Close();

    // 6-2. 전역 시스템 파괴
    //   - BaseGlobal::Destroy 내부에서 gpThreadManager가 먼저 파괴되며 모든 워커 스레드를 안전하게 Join합니다.
    //   - 메인 스레드의 TLS 캐시 정리 및 메모리 풀 해제가 올바른 순서로 진행됩니다.
    std::cout << "[System] Calling BaseGlobal::Destroy()...\n";
    BaseGlobal::Destroy();
    std::cout << "[System] BaseGlobal::Destroy() finished.\n";

    // 6-3. 소켓 유틸리티 정리
    CSocketUtils::Clear();

    std::cout << "[System] Server terminated safely.\n";

    system("pause");

    return 0;
}