//***************************************************************************
// SocketUtils.cpp: implementation of the CSocketUtils class.
//
//***************************************************************************

#include "pch.h"
#include "SocketUtils.h"

// 영문 에러 메시지 테이블 정의
const WINSOCK_ERRORCODE_INFO CSocketUtils::_errTableEn[] =
{
	{ WSAEINTR,                 _T("Interrupted function call") },
	{ WSAEACCES,                _T("Permission denied") },
	{ WSAEFAULT,                _T("Bad address") },
	{ WSAEINVAL,                _T("Invalid argument") },
	{ WSAEMFILE,                _T("Too many open files") },
	{ WSAEWOULDBLOCK,           _T("Socket would block") },
	{ WSAEINPROGRESS,           _T("Operation now in progress") },
	{ WSAEALREADY,              _T("Operation already in progress") },
	{ WSAENOTSOCK,              _T("Socket operation on nonsocket") },
	{ WSAEDESTADDRREQ,          _T("Destination address required") },
	{ WSAEMSGSIZE,              _T("Message too long") },
	{ WSAEPROTOTYPE,            _T("Protocol wrong type for socket") },
	{ WSAENOPROTOOPT,           _T("Bad protocol option") },
	{ WSAEPROTONOSUPPORT,       _T("Protocol not supported") },
	{ WSAESOCKTNOSUPPORT,       _T("Socket type not supported") },
	{ WSAEOPNOTSUPP,            _T("Operation not supported") },
	{ WSAEPFNOSUPPORT,          _T("Protocol family not supported") },
	{ WSAEAFNOSUPPORT,          _T("Address family not supported by protocol family") },
	{ WSAEADDRINUSE,            _T("Address already in use") },
	{ WSAEADDRNOTAVAIL,         _T("Cannot assign requested address") },
	{ WSAENETDOWN,              _T("Network is down") },
	{ WSAENETUNREACH,           _T("Network is unreachable") },
	{ WSAENETRESET,             _T("Network dropped connection on reset") },
	{ WSAECONNABORTED,          _T("Software caused connection abort") },
	{ WSAECONNRESET,            _T("Connection reset by peer") },
	{ WSAENOBUFS,               _T("No buffer space available") },
	{ WSAEISCONN,               _T("Socket is already connected") },
	{ WSAENOTCONN,              _T("Socket is not connected") },
	{ WSAESHUTDOWN,             _T("Cannot send after socket shutdown") },
	{ WSAETIMEDOUT,             _T("Connection timed out") },
	{ WSAECONNREFUSED,          _T("Connection refused") },
	{ WSAEHOSTDOWN,             _T("Host is down") },
	{ WSAEHOSTUNREACH,          _T("No route to host") },
	{ WSAEPROCLIM,              _T("Too many processes") },
	{ WSASYSNOTREADY,           _T("Network subsystem is unavailable") },
	{ WSAVERNOTSUPPORTED,       _T("Winsock.dll version out of range") },
	{ WSANOTINITIALISED,        _T("Successful WSAStartup not yet performed") },
	{ WSAEDISCON,               _T("Graceful shutdown in progress") },
	{ WSATYPE_NOT_FOUND,        _T("Class type not found") },
	{ WSAHOST_NOT_FOUND,        _T("Host not found") },
	{ WSATRY_AGAIN,             _T("Nonauthoritative host not found") },
	{ WSANO_RECOVERY,           _T("This is a nonrecoverable error") },
	{ WSANO_DATA,               _T("Valid name, no data record of requested type") },
	{ WSA_INVALID_HANDLE,       _T("Specified event object handle is invalid") },
	{ WSA_INVALID_PARAMETER,    _T("One or more parameters are invalid") },
	{ WSA_IO_INCOMPLETE,        _T("Overlapped I/O event object not in signaled state") },
	{ WSA_IO_PENDING,           _T("Overlapped operations will complete later") },
	{ WSA_NOT_ENOUGH_MEMORY,    _T("Insufficient memory available") },
	{ WSA_OPERATION_ABORTED,    _T("Overlapped operation aborted") },
	{ WSASYSCALLFAILURE,        _T("System call failure") }
};

// 한글 에러 메시지 테이블 정의
const WINSOCK_ERRORCODE_INFO CSocketUtils::_errTableKr[] =
{
	{ WSAEINTR,                 _T("중단 된 함수 호출.") },
	{ WSAEACCES,                _T("사용 권한이 거부되었습니다.") },
	{ WSAEFAULT,                _T("잘못된 주소.") },
	{ WSAEINVAL,                _T("잘못된 인수.") },
	{ WSAEMFILE,                _T("열려있는 파일이 너무 많습니다.") },
	{ WSAEWOULDBLOCK,           _T("소켓이 차단됩니다.") },
	{ WSAEINPROGRESS,           _T("작업이 현재 진행중.") },
	{ WSAEALREADY,              _T("작업이 이미 진행중.") },
	{ WSAENOTSOCK,              _T("소켓이 아닌 소켓에서의 조작.") },
	{ WSAEDESTADDRREQ,          _T("목적지 주소 필요.") },
	{ WSAEMSGSIZE,              _T("메시지가 너무 깁니다.") },
	{ WSAEPROTOTYPE,            _T("소켓에 대한 프로토콜 유형이 잘못되었습니다.") },
	{ WSAENOPROTOOPT,           _T("잘못된 프로토콜 옵션.") },
	{ WSAEPROTONOSUPPORT,       _T("지원되지 않는 프로토콜") },
	{ WSAESOCKTNOSUPPORT,       _T("소켓 형식이 지원 되지 않습니다.") },
	{ WSAEOPNOTSUPP,            _T("작업이 지원 되지 않습니다.") },
	{ WSAEPFNOSUPPORT,          _T("프로토콜 제품군 지원 되지 않습니다.") },
	{ WSAEAFNOSUPPORT,          _T("프로토콜 패밀리가 지원 하지 않는 주소.") },
	{ WSAEADDRINUSE,            _T("주소 이미 사용 합니다.") },
	{ WSAEADDRNOTAVAIL,         _T("요청한 주소를 할당할 수 없습니다.") },
	{ WSAENETDOWN,              _T("네트워크 다운 되었습니다.") },
	{ WSAENETUNREACH,           _T("네트워크에 연결할 수 없습니다.") },
	{ WSAENETRESET,             _T("네트워크 재설정으로 연결이 끊어졌습니다.") },
	{ WSAECONNABORTED,          _T("소프트웨어 때문에 연결이 중단 되었습니다.") },
	{ WSAECONNRESET,            _T("기존 연결 원격 호스트에 의해 강제로 끊겼습니다.") },
	{ WSAENOBUFS,               _T("사용 가능한 버퍼 공간이 없습니다.") },
	{ WSAEISCONN,               _T("소켓이 이미 연결 되어 있습니다.") },
	{ WSAENOTCONN,              _T("소켓 연결 되어 있지 않습니다.") },
	{ WSAESHUTDOWN,             _T("소켓이 종료 된 후에 보낼 수 없습니다.") },
	{ WSAETIMEDOUT,             _T("연결 시간이 초과 되었습니다.") },
	{ WSAECONNREFUSED,          _T("연결 거부 됨.") },
	{ WSAEHOSTDOWN,             _T("호스트 다운 되었습니다.") },
	{ WSAEHOSTUNREACH,          _T("호스트에 경로가 없습니다.") },
	{ WSAEPROCLIM,              _T("프로세스가 너무 많습니다.") },
	{ WSASYSNOTREADY,           _T("네트워크 하위 시스템을 사용할 수 없습니다.") },
	{ WSAVERNOTSUPPORTED,       _T("Winsock.dll 버전이 범위를 벗어났습니다.") },
	{ WSANOTINITIALISED,        _T("아직 수행 되지 않을 때 WSAStartup을 성공적으로 수행 됩니다.") },
	{ WSAEDISCON,               _T("정상 종료 진행 중.") },
	{ WSATYPE_NOT_FOUND,        _T("클래스 유형을 찾을 수 없습니다.") },
	{ WSAHOST_NOT_FOUND,        _T("호스트를 찾을 수 없습니다.") },
	{ WSATRY_AGAIN,             _T("신뢰할 수 없는 호스트를 찾을 수 없습니다.") },
	{ WSANO_RECOVERY,           _T("복구할 수 없는 오류입니다.") },
	{ WSANO_DATA,               _T("유효한 이름, 요청 된 형식의 데이터 레코드가 없습니다.") },
	{ WSA_INVALID_HANDLE,       _T("지정한 이벤트 개체 핸들이 잘못 되었습니다.") },
	{ WSA_INVALID_PARAMETER,    _T("하나 이상의 매개 변수가 올바르지 않습니다.") },
	{ WSA_IO_INCOMPLETE,        _T("Overlapped I/O 이벤트 개체에 통보 상태") },
	{ WSA_IO_PENDING,           _T("Overlapped 작업 나중에 완성") },
	{ WSA_NOT_ENOUGH_MEMORY,    _T("사용 가능한 메모리가 부족 합니다.") },
	{ WSA_OPERATION_ABORTED,    _T("Overlapped 작업 중단") },
	{ WSASYSCALLFAILURE,        _T("시스템 호출 오류") }
};

// 정적 확장 함수 포인터 초기화
LPFN_ACCEPTEX               CSocketUtils::_acceptEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS    CSocketUtils::_getAcceptExSockAddrs = nullptr;
LPFN_CONNECTEX               CSocketUtils::_connectEx = nullptr;
LPFN_DISCONNECTEX            CSocketUtils::_disconnectEx = nullptr;

//***************************************************************************
// @brief Winsock 라이브러리(WSAStartup) 및 IOCP 확장 함수 포인터를 초기화합니다.
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::Init()
{
	WORD wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;

	int32 result = ::WSAStartup(wVersion, &wsaData);
	if( result != 0 || LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2 )
		return false;

	SOCKET dummySocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if( dummySocket == INVALID_SOCKET )
		return false;

	bool success = true;
	success &= BindExtensionFunction(dummySocket, WSAID_ACCEPTEX, reinterpret_cast<void**>(&_acceptEx));
	success &= BindExtensionFunction(dummySocket, WSAID_GETACCEPTEXSOCKADDRS, reinterpret_cast<void**>(&_getAcceptExSockAddrs));
	success &= BindExtensionFunction(dummySocket, WSAID_CONNECTEX, reinterpret_cast<void**>(&_connectEx));
	success &= BindExtensionFunction(dummySocket, WSAID_DISCONNECTEX, reinterpret_cast<void**>(&_disconnectEx));

	::closesocket(dummySocket);
	return success;
}

//***************************************************************************
// @brief Winsock 라이브러리 자원(WSACleanup)을 해제합니다.
//***************************************************************************
void CSocketUtils::Clear()
{
	::WSACleanup();
}

//***************************************************************************
// @brief shutdown 후 closesocket을 수행하여 소켓을 정상 종료합니다.
// @param socket 대상 소켓 핸들
// @param how shutdown 방식 (SD_RECEIVE / SD_SEND / SD_BOTH)
//***************************************************************************
void CSocketUtils::CloseGraceful(SOCKET socket, int how)
{
	if( socket == INVALID_SOCKET ) return;

	(void)::shutdown(socket, how);
	(void)::closesocket(socket);
}

//***************************************************************************
// @brief WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)을 사용하여 확장 함수 포인터 1개를 바인딩합니다.
// @param socket 바인딩 조회에 사용할 임시 소켓 핸들
// @param guid 바인딩할 확장 함수의 GUID
// @param fn 바인딩된 함수 포인터가 저장될 출력 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::BindExtensionFunction(SOCKET socket, GUID guid, void** fn)
{
	DWORD bytesReturned = 0;
	int32 result = ::WSAIoctl(
		socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid, sizeof(guid),
		fn, sizeof(*fn),
		OUT & bytesReturned,
		nullptr, nullptr
	);

	return (result != SOCKET_ERROR);
}

// ---------- 소켓 생성 ----------

//***************************************************************************
// @brief 비동기 Overlapped I/O를 지원하는 TCP 소켓(IPv4)을 생성합니다.
// @return 생성된 소켓 핸들 (실패 시 INVALID_SOCKET)
//***************************************************************************
SOCKET CSocketUtils::CreateSocket()
{
	return ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
}

//***************************************************************************
// @brief 비동기 Overlapped I/O를 지원하는 UDP 소켓(IPv4)을 생성합니다.
// @return 생성된 소켓 핸들 (실패 시 INVALID_SOCKET)
//***************************************************************************
SOCKET CSocketUtils::CreateUdpSocket()
{
	return ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
}

//***************************************************************************
// @brief RIO(Registered I/O) 전용 TCP 소켓(WSA_FLAG_REGISTERED_IO)을 생성합니다.
// @return 생성된 소켓 핸들 (실패 시 INVALID_SOCKET)
//***************************************************************************
SOCKET CSocketUtils::CreateRioSocket()
{
	return ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
		WSA_FLAG_OVERLAPPED | WSA_FLAG_REGISTERED_IO);
}

//***************************************************************************
// @brief RIO(Registered I/O) 전용 UDP 소켓(WSA_FLAG_REGISTERED_IO)을 생성합니다.
// @return 생성된 소켓 핸들 (실패 시 INVALID_SOCKET)
//***************************************************************************
SOCKET CSocketUtils::CreateRioUdpSocket()
{
	return ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
		WSA_FLAG_OVERLAPPED | WSA_FLAG_REGISTERED_IO);
}

// ---------- 옵션 ----------

//***************************************************************************
// @brief SO_REUSEADDR 옵션을 설정하여 이미 사용 중인 주소의 재사용 여부를 지정합니다.
// @param socket 대상 소켓 핸들
// @param flag 재사용 활성화 여부 (true: 활성화, false: 비활성화)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetReuseAddress(SOCKET socket, bool flag)
{
	int32 value = flag ? 1 : 0;
	return ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
		reinterpret_cast<char*>(&value), sizeof(value)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief SO_LINGER 옵션을 설정하여 소켓 종료 시 잔여 데이터 처리 방식을 지정합니다.
// @param socket 대상 소켓 핸들
// @param onOff Linger 옵션 활성화 여부 (1: 활성화, 0: 비활성화)
// @param seconds Linger 대기 시간(초 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetLinger(SOCKET socket, uint16 onOff, uint16 seconds)
{
	LINGER lingerOption;
	lingerOption.l_onoff = onOff;
	lingerOption.l_linger = seconds;
	return ::setsockopt(socket, SOL_SOCKET, SO_LINGER,
		reinterpret_cast<char*>(&lingerOption), sizeof(lingerOption)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief TCP_NODELAY 옵션을 설정하여 Nagle 알고리즘의 활성화 여부를 지정합니다.
// @param socket 대상 소켓 핸들
// @param flag Nagle 알고리즘 비활성화 여부 (true: Nagle 끔/즉시 전송, false: Nagle 켬)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetNoDelay(SOCKET socket, bool flag)
{
	int32 value = flag ? 1 : 0;
	return ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
		reinterpret_cast<char*>(&value), sizeof(value)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief SO_RCVBUF 옵션을 설정하여 소켓의 수신 버퍼 크기를 변경합니다.
// @param socket 대상 소켓 핸들
// @param size 바이트 단위 수신 버퍼 크기
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetRecvBufferSize(SOCKET socket, int32 size)
{
	return ::setsockopt(socket, SOL_SOCKET, SO_RCVBUF,
		reinterpret_cast<char*>(&size), sizeof(size)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief SO_SNDBUF 옵션을 설정하여 소켓의 송신 버퍼 크기를 변경합니다.
// @param socket 대상 소켓 핸들
// @param size 바이트 단위 송신 버퍼 크기
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetSendBufferSize(SOCKET socket, int32 size)
{
	return ::setsockopt(socket, SOL_SOCKET, SO_SNDBUF,
		reinterpret_cast<char*>(&size), sizeof(size)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief SO_UPDATE_ACCEPT_CONTEXT 옵션을 설정하여 AcceptEx로 수락된 소켓에 Listen 소켓의 컨텍스트를 동기화합니다.
// @param clientSocket AcceptEx로 연결 수락된 소켓 Handle
// @param listenSocket 대기 중이던 Listen 소켓 Handle
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetUpdateAcceptContext(SOCKET clientSocket, SOCKET listenSocket)
{
	return ::setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		reinterpret_cast<char*>(&listenSocket), sizeof(listenSocket)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief SO_UPDATE_CONNECT_CONTEXT 옵션을 설정하여 ConnectEx 비동기 연결 완료 후 소켓 컨텍스트를 갱신합니다.
// @param socket ConnectEx가 완료된 소켓 Handle
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetUpdateConnectContext(SOCKET socket)
{
	return ::setsockopt(socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
		nullptr, 0) != SOCKET_ERROR;
}

// ---------- Connect / Bind / Listen / Close ----------

//***************************************************************************
// @brief 지정한 소켓을 원격 주소(CNetAddress)에 동기 방식으로 연결합니다.
// @param socket 연결에 사용할 대상 소켓 핸들
// @param netAddr 접속할 원격지의 네트워크 주소 정보 (CNetAddress)
// @return bool 연결 성공 시 true, 실패 시 false 반환
// @details
//      - Winsock의 기본 동기 `connect()` API를 호출하여 원격 서버와의 3-Way Handshake를 수행합니다.
//      - 연결 실패 시 내부적으로 `ReportError`를 통해 Winsock 에러 코드를 로그로 기록합니다.
//      - RIO(Registered I/O) 환경에서 소켓 연결 완료 후 RIO_RQ를 생성하기 전 단계에 활용됩니다.
//***************************************************************************
bool CSocketUtils::Connect(SOCKET socket, CNetAddress netAddr)
{
	if( socket == INVALID_SOCKET )
		return false;

	SOCKADDR_IN serverAddr = netAddr.GetSockAddr();
	int result = ::connect(socket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr));

	if( result == SOCKET_ERROR )
	{
		ReportError(_T("CSocketUtils::Connect"), ::WSAGetLastError());
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 소켓에 네트워크 주소(IP 및 Port)를 바인딩합니다.
// @param socket 대상 소켓 핸들
// @param netAddr 바인딩할 주소 정보를 담은 CNetAddress 객체
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::Bind(SOCKET socket, CNetAddress netAddr)
{
	SOCKADDR_IN sockAddr = netAddr.GetSockAddr();
	return ::bind(socket, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr)) != SOCKET_ERROR;
}

//***************************************************************************
// @brief 소켓을 연결 요청 수신 대기 상태(Listen)로 전환합니다.
// @param socket 대상 소켓 핸들
// @param backlog 연결 대기 큐의 최대 크기 (기본값: SOMAXCONN)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::Listen(SOCKET socket, int32 backlog)
{
	return ::listen(socket, backlog) != SOCKET_ERROR;
}

//***************************************************************************
// @brief 소켓 핸들을 닫아 네트워크 자원을 해제합니다.
// @param socket 닫을 소켓 핸들
//***************************************************************************
void CSocketUtils::Close(SOCKET socket)
{
	if( socket != INVALID_SOCKET )
		::closesocket(socket);
}

// ---------- IOCP 확장 함수 ----------

//***************************************************************************
// @brief AcceptEx 비동기 수락 확장 함수를 호출합니다.
// @param listenSocket 리슨 소켓 핸들
// @param acceptSocket 미리 생성해 둔 수락 대상 소켓 핸들
// @param outputBuffer 첫 수신 데이터 및 로컬/원격 주소를 수신할 버퍼 포인터
// @param receiveDataLength outputBuffer에서 데이터 수신용으로 사용할 바이트 크기
// @param localAddrLength 로컬 주소 저장용 버퍼 크기 (sizeof(sockaddr_in) + 16)
// @param remoteAddrLength 원격 주소 저장용 버퍼 크기 (sizeof(sockaddr_in) + 16)
// @param bytesReceived 실제 수신 완료된 바이트 수가 출력될 포인터
// @param overlapped 비동기 I/O 처리를 위한 OVERLAPPED 구조체 포인터
// @return 성공 시 TRUE, 실패 시 FALSE (WSA_IO_PENDING 등)
//***************************************************************************
BOOL CSocketUtils::AcceptEx(SOCKET listenSocket, SOCKET acceptSocket, PVOID outputBuffer,
	DWORD receiveDataLength, DWORD localAddrLength, DWORD remoteAddrLength,
	LPDWORD bytesReceived, LPOVERLAPPED overlapped)
{
	ASSERT_CRASH(_acceptEx != nullptr);
	return _acceptEx(listenSocket, acceptSocket, outputBuffer, receiveDataLength,
		localAddrLength, remoteAddrLength, bytesReceived, overlapped);
}

//***************************************************************************
// @brief AcceptEx 수신 버퍼로부터 로컬 및 원격 SOCKADDR 포인터를 파싱합니다.
// @param outputBuffer AcceptEx 호출 시 사용했던 버퍼 포인터
// @param receiveDataLength AcceptEx 호출 시 지정한 수신 데이터 크기
// @param localAddrLength AcceptEx 호출 시 지정한 로컬 주소 버퍼 크기
// @param remoteAddrLength AcceptEx 호출 시 지정한 원격 주소 버퍼 크기
// @param localSockAddr 파싱된 로컬 SOCKADDR 구조체 포인터 출력
// @param localSockAddrLen 로컬 SOCKADDR 구조체 길이 출력
// @param remoteSockAddr 파싱된 원격 SOCKADDR 구조체 포인터 출력
// @param remoteSockAddrLen 원격 SOCKADDR 구조체 길이 출력
//***************************************************************************
void CSocketUtils::GetAcceptExSockaddrs(PVOID outputBuffer, DWORD receiveDataLength,
	DWORD localAddrLength, DWORD remoteAddrLength,
	LPSOCKADDR* localSockAddr, LPINT localSockAddrLen,
	LPSOCKADDR* remoteSockAddr, LPINT remoteSockAddrLen)
{
	ASSERT_CRASH(_getAcceptExSockAddrs != nullptr);
	_getAcceptExSockAddrs(outputBuffer, receiveDataLength, localAddrLength, remoteAddrLength,
		localSockAddr, localSockAddrLen, remoteSockAddr, remoteSockAddrLen);
}

//***************************************************************************
// @brief ConnectEx 비동기 연결 확장 함수를 호출합니다.
// @param socket 연결에 사용할 소켓 핸들 (미리 Bind 되어 있어야 함)
// @param name 연결할 목적지 SOCKADDR 구조체 포인터
// @param nameLen SOCKADDR 구조체 크기
// @param sendBuffer 연결 직후 바로 전송할 데이터 버퍼 포인터 (없을 경우 nullptr)
// @param sendDataLength 전송할 데이터 길이
// @param bytesSent 실제 전송 완료된 바이트 수 출력 포인터
// @param overlapped 비동기 I/O 처리를 위한 OVERLAPPED 구조체 포인터
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CSocketUtils::ConnectEx(SOCKET socket, const SOCKADDR* name, int32 nameLen,
	PVOID sendBuffer, DWORD sendDataLength,
	LPDWORD bytesSent, LPOVERLAPPED overlapped)
{
	ASSERT_CRASH(_connectEx != nullptr);
	return _connectEx(socket, name, nameLen, sendBuffer, sendDataLength, bytesSent, overlapped);
}

//***************************************************************************
// @brief DisconnectEx 비동기 연결 해제 확장 함수를 호출합니다.
// @param socket 연결을 끊을 소켓 핸들
// @param overlapped 비동기 I/O 처리를 위한 OVERLAPPED 구조체 포인터
// @param flags 소켓 해제 옵션 플래그 (TF_REUSE_SOCKET 등)
// @param reserved 예약 필드 (0 지정)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CSocketUtils::DisconnectEx(SOCKET socket, LPOVERLAPPED overlapped, DWORD flags, DWORD reserved)
{
	ASSERT_CRASH(_disconnectEx != nullptr);
	return _disconnectEx(socket, overlapped, flags, reserved);
}

//***************************************************************************
// @brief 소켓의 블로킹/논블로킹(Non-blocking) 모드를 설정합니다.
// @param socket 모드를 변경할 대상 소켓 핸들
// @param nonBlocking true 설정 시 논블로킹 모드, false 설정 시 블로킹 모드
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::SetNonBlocking(SOCKET socket, bool nonBlocking)
{
	u_long mode = nonBlocking ? 1 : 0;
	return ::ioctlsocket(socket, FIONBIO, &mode) == 0;
}

//***************************************************************************
// @brief 클라이언트의 연결 요청을 수락(Accept)하고 주소 정보를 읽어옵니다.
// @param listenSocket 연결 요청을 수신 대기 중인 리슨(Listen) 소켓 핸들
// @param outClientAddress [OUT] 연결된 클라이언트의 주소 정보(sockaddr_in)를 저장할 참조 변수
// @return 생성된 클라이언트 소켓 핸들 (실패 시 INVALID_SOCKET)
//***************************************************************************
SOCKET CSocketUtils::Accept(SOCKET listenSocket, sockaddr_in& outClientAddress)
{
	int addrLen = sizeof(outClientAddress);
	return ::accept(listenSocket, reinterpret_cast<SOCKADDR*>(&outClientAddress), &addrLen);
}

//***************************************************************************
// @brief 문자열 형식의 IP 주소를 이진 주소 구조체(in_addr / in6_addr)로 변환합니다.
// @param af 주소 체계 (AF_INET 또는 AF_INET6)
// @param hostAddress IP 주소 문자열 (예: _T("127.0.0.1"))
// @param dest 변환 결과를 저장할 메모리 버퍼 포인터
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
bool CSocketUtils::IPToAddr(const int af, const TCHAR* hostAddress, void* dest)
{
	int nLen = sizeof(sockaddr_storage);
	struct sockaddr_storage ss;
	TCHAR tszHostAddress[IP6_STRLEN + 1];

	::ZeroMemory(&ss, sizeof(ss));
	_tcsncpy_s(tszHostAddress, _countof(tszHostAddress), hostAddress, _TRUNCATE);

	int nRet = ::WSAStringToAddress(tszHostAddress, af, NULL,
		reinterpret_cast<SOCKADDR*>(&ss), &nLen);
	if( nRet != 0 )
		return FALSE;

	switch( af )
	{
	case AF_INET:
	{
		struct in_addr inAddr = reinterpret_cast<sockaddr_in*>(&ss)->sin_addr;
		::memcpy(dest, &inAddr, sizeof(struct in_addr));
		break;
	}
	case AF_INET6:
	{
		struct in6_addr inAddr6 = reinterpret_cast<sockaddr_in6*>(&ss)->sin6_addr;
		::memcpy(dest, &inAddr6, sizeof(struct in6_addr));
		break;
	}
	}

	return TRUE;
}

//***************************************************************************
// @brief 이진 주소 구조체를 문자열 IP 주소로 변환합니다.
// @param af 주소 체계 (AF_INET 또는 AF_INET6)
// @param src 바이너리 주소 구조체 포인터
// @param hostAddress 변환된 문자열이 저장될 버퍼
// @param size 버퍼의 크기
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
bool CSocketUtils::AddrToIP(const int af, const void* src, TCHAR* hostAddress, socklen_t size)
{
	struct sockaddr_storage ss;
	unsigned long ulSize = size;

	::ZeroMemory(&ss, sizeof(ss));
	ss.ss_family = static_cast<ADDRESS_FAMILY>(af);

	DWORD dwSockAddrLen = sizeof(ss);

	switch( af )
	{
	case AF_INET:
		reinterpret_cast<sockaddr_in*>(&ss)->sin_addr = *reinterpret_cast<const struct in_addr*>(src);
		dwSockAddrLen = sizeof(sockaddr_in); // [수정] IPv4 정확한 구조체 크기 지정
		break;
	case AF_INET6:
		reinterpret_cast<sockaddr_in6*>(&ss)->sin6_addr = *reinterpret_cast<const struct in_addr6*>(src);
		dwSockAddrLen = sizeof(sockaddr_in6); // [수정] IPv6 정확한 구조체 크기 지정
		break;
	default:
		return false;
	}

	if( ::WSAAddressToString(reinterpret_cast<SOCKADDR*>(&ss), dwSockAddrLen, NULL, hostAddress, &ulSize) != 0 )
		return false;

	return true;
}

//***************************************************************************
// @brief IPv4 주소를 IPv6 Mapped IPv4 주소 형태(::ffff:x.x.x.x)로 변환합니다.
// @param ipv4 원본 IPv4 in_addr 구조체
// @param ipv6 변환 결과가 저장될 IPv6 in6_addr 참조
//***************************************************************************
void CSocketUtils::IPv4ToIPv6(const struct in_addr ipv4, struct in6_addr& ipv6)
{
	::memset(&ipv6, 0x00, sizeof(ipv6));
	ipv6.s6_addr[10] = ipv6.s6_addr[11] = 0xFF;
	::memcpy(&ipv6.s6_addr[12], &ipv4.s_addr, sizeof(ipv4));
}

//***************************************************************************
// @brief 호스트 이름 및 포트 번호를 바탕으로 도메인 조회(DNS) 후 addrinfo 리스트를 취득합니다.
// @param hostName 도메인 이름 또는 IP 문자열
// @param port 포트 번호
// @param sockAddrList 조회된 addrinfo 목록이 저장될 std::list 참조
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
bool CSocketUtils::GetSockAddrIn(const TCHAR* hostName, const int port, std::list<addrinfo>& sockAddrList)
{
	char    szHostName[PC_NAME_STRLEN];
	char    szPort[PORT_STRLEN];
	char* pszHostName = NULL;
	struct addrinfo hints;
	struct addrinfo* pResult = NULL;

	::memset(&hints, 0, sizeof(addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if( hostName )
	{
#ifdef _UNICODE
		::WideCharToMultiByte(CP_ACP, 0, hostName, -1, szHostName, _countof(szHostName), NULL, NULL);
		pszHostName = szHostName;
#else
		pszHostName = const_cast<char*>(hostName);
#endif
	}

	_itoa_s(port, szPort, _countof(szPort), 10);

	if( ::getaddrinfo(pszHostName, szPort, &hints, &pResult) != 0 )
	{
		_ASSERT(0);
		return FALSE;
	}

	for( addrinfo* pAddrInfo = pResult; pAddrInfo != NULL; pAddrInfo = pAddrInfo->ai_next )
	{
		addrinfo addrInfo;
		::memcpy(&addrInfo, pAddrInfo, sizeof(addrinfo));
		sockAddrList.push_back(addrInfo);
	}

	::freeaddrinfo(pResult);
	return TRUE;
}

//***************************************************************************
// @brief getpeername()을 래핑하여 연결된 소켓의 원격지 주소를 조회합니다.
// @param socket 대상 소켓 핸들
// @param outAddress 조회된 원격지 주소가 채워질 구조체
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CSocketUtils::GetPeerAddress(SOCKET socket, sockaddr_in& outAddress)
{
	int addrLen = sizeof(outAddress);
	return ::getpeername(socket, reinterpret_cast<SOCKADDR*>(&outAddress), &addrLen) == 0;
}

// ---------- 에러 메시지 ----------

//***************************************************************************
// @brief Winsock 에러 코드에 해당하는 영문 설명 메시지를 조회합니다.
// @param errorCode Winsock 에러 번호 (WSAGetLastError())
// @return 영문 에러 메시지 문자열 포인터 (없을 경우 nullptr)
//***************************************************************************
const TCHAR* CSocketUtils::GetErrMsgToWinsockErrCodeEn(const int errorCode)
{
	for( size_t i = 0; i < _countof(_errTableEn); i++ )
	{
		if( _errTableEn[i].nErrorCode == errorCode )
			return _errTableEn[i].ptszErrMsg;
	}
	return nullptr;
}

//***************************************************************************
// @brief Winsock 에러 코드에 해당하는 한글 설명 메시지를 조회합니다.
// @param errorCode Winsock 에러 번호 (WSAGetLastError())
// @return 한글 에러 메시지 문자열 포인터 (없을 경우 nullptr)
//***************************************************************************
const TCHAR* CSocketUtils::GetErrMsgToWinsockErrCodeKr(const int errorCode)
{
	for( size_t i = 0; i < _countof(_errTableKr); i++ )
	{
		if( _errTableKr[i].nErrorCode == errorCode )
			return _errTableKr[i].ptszErrMsg;
	}
	return nullptr;
}

//***************************************************************************
// @brief 작업 명칭과 에러 코드를 포맷팅하여 에러 보고(로그 기록)를 수행합니다.
// @param operationDesc 에러가 발생한 작업에 대한 설명
// @param errorCode Winsock 에러 번호
//***************************************************************************
void CSocketUtils::ReportError(const TCHAR* operationDesc, const int errorCode)
{
	TCHAR tszBuffer[MAX_BUFFER_SIZE];
	const TCHAR* ptszMsgBuffer = NULL;
	bool isAllocatedBySystem = false;

#ifdef _DEBUG_KR
	ptszMsgBuffer = GetErrMsgToWinsockErrCodeEn(errorCode);
#elif _DEBUG_EN
	ptszMsgBuffer = GetErrMsgToWinsockErrCodeKr(errorCode);
#else
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	if( ::FormatMessage(flags, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPTSTR>(&ptszMsgBuffer), 0, NULL) != 0 )
	{
		isAllocatedBySystem = true;
	}
#endif

	_stprintf_s(tszBuffer, _countof(tszBuffer), _T("%s: %d- %s"),
		operationDesc, errorCode, ptszMsgBuffer);
	LOG_INFO(_T("Error : %s"), tszBuffer);

	// FormatMessage 시스템 할당 메모리 해제
	if( isAllocatedBySystem && ptszMsgBuffer )
	{
		::LocalFree(const_cast<TCHAR*>(ptszMsgBuffer));
	}
}