// EchoServerClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "RioEchoSession.h"
#include "IocpEchoSession.h"

constexpr ENetworkEngineType kTestEngineType = ENetworkEngineType::IOCP; // 또는 ENetworkEngineType::RIO

#ifndef RIO_INVALID_BUFFER_ID
#define RIO_INVALID_BUFFER_ID ((RIO_BUFFERID)(ULONG_PTR)-1)
#endif

#ifndef RIO_INVALID_CQ
#define RIO_INVALID_CQ ((RIO_CQ)(ULONG_PTR)-1)
#endif

#ifndef RIO_INVALID_RQ
#define RIO_INVALID_RQ ((RIO_RQ)(ULONG_PTR)-1)
#endif

// [서버 실행] 
// - 시작 메뉴에서 PowerShell을 검색하여 관리자 권한 또는 일반 권한으로 실행합니다.
/*
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 12345)
$listener.Start()
Write-Host "Listening on 12345... (Ctrl+C to stop)"

try {
	while ($true) {
		# 클라이언트 접속 대기 (블로킹)
		$client = $listener.AcceptTcpClient()
		Write-Host "Client connected!"

		# 테스트 목적이므로 접속 직후 연결 해제 (필요에 따라 주석 처리 가능)
		$client.Close()
	}
}
finally {
	$listener.Stop()
}
*/
// - 위의 올바른 코드를 복사해서 PowerShell 창에 그대로 붙여넣고 Enter를 누릅니다.
int RioSimpleTest()
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << "\n";
		return 1;
	}

	// 2. 소켓 생성 (WSA_FLAG_REGISTERED_IO 플래그 필수)
	SOCKET sock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_REGISTERED_IO);
	if( sock == INVALID_SOCKET ) {
		std::cerr << "WSASocket failed. Error: " << WSAGetLastError() << "\n";
		WSACleanup();
		return 1;
	}
	std::cout << "1. WSASocket with WSA_FLAG_REGISTERED_IO created successfully.\n";

	// 3. RIO 확장 함수 테이블 로드 (공식 GUID 변수 사용)
	GUID rIOId = WSAID_MULTIPLE_RIO;
	RIO_EXTENSION_FUNCTION_TABLE rio = {};
	DWORD bytes = 0;

	// 주의: 출력 인자로 구조체 전체 크기를 넘깁니다.
	if( WSAIoctl(sock, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
		&rIOId, sizeof(rIOId),
		&rio, sizeof(rio),
		&bytes, NULL, NULL) == SOCKET_ERROR ) {
		std::cerr << "WSAIoctl (SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER) failed. Error: " << WSAGetLastError() << "\n";
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	std::cout << "2. RIO extension functions loaded successfully.\n";

	// 5. 버퍼 malloc 및 RIORegisterBuffer() 등록
	const DWORD bufferSize = 1024;
	char* sendBuffer = (char*)malloc(bufferSize);
	if( !sendBuffer ) {
		std::cerr << "malloc failed.\n";
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	memset(sendBuffer, 'A', bufferSize);

	RIO_BUF rioBuf;
	rioBuf.BufferId = rio.RIORegisterBuffer(sendBuffer, bufferSize);
	if( rioBuf.BufferId == RIO_INVALID_BUFFER_ID ) {
		std::cerr << "RIORegisterBuffer failed. Error: " << WSAGetLastError() << "\n";
		free(sendBuffer);
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	rioBuf.Offset = 0;
	rioBuf.Length = bufferSize;
	std::cout << "4. RIORegisterBuffer succeeded. BufferId: " << (void*)rioBuf.BufferId << "\n";

	// 6. CQ(Completion Queue) 및 RQ(Request Queue) 생성
	RIO_CQ cq = rio.RIOCreateCompletionQueue(32, NULL);
	if( cq == RIO_INVALID_CQ ) {
		std::cerr << "RIOCreateCompletionQueue failed. Error: " << WSAGetLastError() << "\n";
		free(sendBuffer);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	RIO_RQ rq = rio.RIOCreateRequestQueue(sock, 16, 1, 16, 1, cq, cq, NULL);
	if( rq == RIO_INVALID_RQ ) {
		std::cerr << "RIOCreateRequestQueue failed. Error: " << WSAGetLastError() << "\n";
		rio.RIOCloseCompletionQueue(cq);
		free(sendBuffer);
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	std::cout << "5. CQ and RQ created successfully.\n";

	// 4. connect() 수행
	SOCKADDR_IN sa = {};
	sa.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
	sa.sin_port = htons(12345);

	if( connect(sock, (SOCKADDR*)&sa, sizeof(sa)) == SOCKET_ERROR ) {
		std::cerr << "connect failed with error (can be ignored if no server is up): " << WSAGetLastError() << "\n";
	}
	else {
		std::cout << "3. connect() succeeded.\n";
	}

	// 7. RIOSendEx 단발성 호출
	OVERLAPPED ov = {};
	BOOL sendSuccess = rio.RIOSendEx(
		rq,
		&rioBuf,
		1,
		NULL,
		NULL,
		NULL,
		NULL,
		0,
		&ov
	);

	if( !sendSuccess ) {
		int err = WSAGetLastError();
		std::cerr << ">>> [RESULT] RIOSendEx failed with error code: " << err << " <<<\n";
	}
	else {
		std::cout << ">>> [RESULT] RIOSendEx succeeded! <<<\n";
	}

	// 정리
	free(sendBuffer);
	closesocket(sock);
	WSACleanup();
	return 0;
}

int IocpRioTest()
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
			[]() { return std::make_shared<CIocpEchoServerSession>(); },
			10, &iocpCore
		);

		// 클라이언트 서비스 생성
		clientService = CNetworkFactory::CreateClientService(
			ENetworkEngineType::IOCP, serverAddress,
			[]() { return std::make_shared<CIocpEchoClientSession>(); },
			1, &iocpCore
		);
	}
	else if( kTestEngineType == ENetworkEngineType::RIO )
	{
		rioCore = std::make_shared<CRioCore>();

		// 서버 서비스 생성 (RIO)
		serverService = CNetworkFactory::CreateServerService(
			ENetworkEngineType::RIO, serverAddress,
			[]() { return std::make_shared<CRioEchoServerSession>(); },
			10, &rioCore
		);

		// 클라이언트 서비스 생성 (RIO)
		clientService = CNetworkFactory::CreateClientService(
			ENetworkEngineType::RIO, serverAddress,
			[]() { return std::make_shared<CRioEchoClientSession>(); },
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

int main()
{
#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
#ifdef _WIN32
	// 1. C 런타임 로케일 설정
	setlocale(LC_ALL, ".UTF8");		// printf, scanf 등 C 스타일의 입출력 함수나 일부 문자열 처리 함수들이 UTF-8 문자열을 올바르게 인식하고 처리할 수 있게 함.

	// 2. 콘솔 입출력 코드페이지를 UTF-8(65001)로 변경
	SetConsoleOutputCP(CP_UTF8);	// 프로그램이 콘솔창에 텍스트를 출력할 때(std::cout, printf 등), 유니코드 문자가 깨지지 않고 올바른 모양(한글 등)으로 그려지도록 지정
	SetConsoleCP(CP_UTF8);			// 사용자가 콘솔창에 키보드로 입력하는 텍스트(std::cin, scanf 등)를 프로그램이 UTF-8 인코딩으로 정확하게 읽어들이도록 보장
#endif

	int retCode = RioSimpleTest();
	//int retCode = IocpRioTest();
}

