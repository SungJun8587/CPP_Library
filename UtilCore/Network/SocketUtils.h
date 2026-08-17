
//***************************************************************************
// SocketUtils.h : interface for the CSocketUtils class.
//
//***************************************************************************

#ifndef __SOCKETUTILS_H__
#define __SOCKETUTILS_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __NETADDRESS_H__
#include <Network/NetAddress.h>
#endif

class CNetAddress;

//***************************************************************************
// @struct WINSOCK_ERRORCODE_INFO
// @brief Winsock 에러 코드 ↔ 설명 문자열 매핑 테이블 원소 구조체.
//***************************************************************************
typedef struct _WINSOCK_ERRORCODE_INFO
{
	int           nErrorCode;   // Winsock 에러 코드 (예: WSAEADDRINUSE)
	const TCHAR* ptszErrMsg;   // 에러 코드에 대응하는 설명 문자열
} WINSOCK_ERRORCODE_INFO, * PWINSOCK_ERRORCODE_INFO;

//***************************************************************************
// @class CSocketUtils
// @brief TCP/UDP 소켓 생성·옵션 설정, IOCP 확장 함수(AcceptEx/ConnectEx/DisconnectEx),
//        RIO(Registered I/O) 전용 소켓 생성까지 아우르는 static 유틸리티 모음.
//
// @details
// 역할 구분:
//     - 소켓 생성          : CreateSocket / CreateUdpSocket (일반 IOCP용, overlapped)
//                            CreateRioSocket / CreateRioUdpSocket (RIO 전용, WSA_FLAG_REGISTERED_IO)
//     - 옵션               : SetReuseAddress, SetLinger, SetNoDelay, 버퍼 크기 등
//     - Bind/Listen/Close  : 공통
//     - IOCP 확장 함수     : AcceptEx/ConnectEx/DisconnectEx/GetAcceptExSockaddrs
//                            프로세스 최초 1회 WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)로 로드
//
// RIO와의 역할 분리:
//     이 클래스는 "RIO 소켓을 만드는 것"까지만 담당한다.
//     RIORegisterBuffer/RIOSend/RIOReceive 등 RIO 확장 함수 테이블 자체는
//     CRioCore가 별도로 WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER)로 로드/보관한다.
//     (CRioCore 생성자에 CSocketUtils::CreateRioSocket()으로 만든 소켓을 넘겨 사용)
//
// 스레드 안전성:
//     확장 함수 포인터는 Init()에서 단일 스레드로 1회만 로드한다고 가정한다.
//     서버 시작 시 WSAStartup 직후, 다른 스레드가 뜨기 전에 CSocketUtils::Init()을 호출할 것.
//***************************************************************************
class CSocketUtils
{
public:
	static bool     Init();
	static void     Clear();
	static void     CloseGraceful(SOCKET socket, int how = SD_BOTH);

public:
	// ---------- 소켓 생성 ----------
	static SOCKET   CreateSocket();
	static SOCKET   CreateUdpSocket();
	static SOCKET   CreateRioSocket();
	static SOCKET   CreateRioUdpSocket();

	// ---------- 옵션 ----------
	static bool     SetReuseAddress(SOCKET socket, bool flag);
	static bool     SetLinger(SOCKET socket, uint16 onOff, uint16 seconds);
	static bool     SetNoDelay(SOCKET socket, bool flag);
	static bool     SetRecvBufferSize(SOCKET socket, int32 size);
	static bool     SetSendBufferSize(SOCKET socket, int32 size);
	static bool     SetUpdateAcceptContext(SOCKET clientSocket, SOCKET listenSocket);
	static bool     SetUpdateConnectContext(SOCKET socket);

	// ---------- Bind / Listen / Close ----------
	static bool     Connect(SOCKET socket, CNetAddress netAddr);
	static bool     Bind(SOCKET socket, CNetAddress netAddr);
	static bool     Listen(SOCKET socket, int32 backlog = SOMAXCONN);
	static void     Close(SOCKET socket);

	// ---------- IOCP 확장 함수 ----------
	static BOOL     AcceptEx(SOCKET listenSocket, SOCKET acceptSocket, PVOID outputBuffer,
		DWORD receiveDataLength, DWORD localAddrLength, DWORD remoteAddrLength,
		LPDWORD bytesReceived, LPOVERLAPPED overlapped);

	static void     GetAcceptExSockaddrs(PVOID outputBuffer, DWORD receiveDataLength,
		DWORD localAddrLength, DWORD remoteAddrLength,
		LPSOCKADDR* localSockAddr, LPINT localSockAddrLen,
		LPSOCKADDR* remoteSockAddr, LPINT remoteSockAddrLen);

	static BOOL     ConnectEx(SOCKET socket, const SOCKADDR* name, int32 nameLen,
		PVOID sendBuffer, DWORD sendDataLength,
		LPDWORD bytesSent, LPOVERLAPPED overlapped);

	static BOOL     DisconnectEx(SOCKET socket, LPOVERLAPPED overlapped, DWORD flags, DWORD reserved);

	// ---------- accept()/non-blocking (RIO blocking-accept 모델용) ----------
	static bool     SetNonBlocking(SOCKET socket, bool nonBlocking);
	static SOCKET   Accept(SOCKET listenSocket, sockaddr_in& outClientAddress);

	// ---------- IP 관련 함수 ----------
	static bool     IPToAddr(const int af, const TCHAR* hostAddress, void* dest);
	static bool     AddrToIP(const int af, const void* src, TCHAR* hostAddress, socklen_t size);
	static void     IPv4ToIPv6(const struct in_addr ipv4, struct in6_addr& ipv6);
	static bool     GetSockAddrIn(const TCHAR* hostName, const int port, std::list<addrinfo>& sockAddrList);
	static bool     GetPeerAddress(SOCKET socket, sockaddr_in& outAddress);

	// ---------- 에러 메시지 ----------
	static const TCHAR* GetErrMsgToWinsockErrCodeEn(const int errorCode);
	static const TCHAR* GetErrMsgToWinsockErrCodeKr(const int errorCode);
	static void         ReportError(const TCHAR* operationDesc, const int errorCode);

private:
	//***************************************************************************
	// @brief WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)을 사용하여 확장 함수 포인터 1개를 바인딩합니다.
	// @param socket 바인딩 조회에 사용할 임시 소켓 핸들
	// @param guid 바인딩할 확장 함수의 GUID
	// @param fn 바인딩된 함수 포인터가 저장될 출력 포인터
	// @return 바인딩 성공 여부
	//***************************************************************************
	static bool     BindExtensionFunction(SOCKET socket, GUID guid, void** fn);

private:
	static LPFN_ACCEPTEX                _acceptEx;               // AcceptEx 확장 함수 포인터
	static LPFN_GETACCEPTEXSOCKADDRS    _getAcceptExSockAddrs;   // GetAcceptExSockaddrs 확장 함수 포인터
	static LPFN_CONNECTEX               _connectEx;              // ConnectEx 확장 함수 포인터
	static LPFN_DISCONNECTEX            _disconnectEx;           // DisconnectEx 확장 함수 포인터

	static const WINSOCK_ERRORCODE_INFO _errTableEn[];           // 영문 에러 코드 매핑 테이블
	static const WINSOCK_ERRORCODE_INFO _errTableKr[];           // 한글 에러 코드 매핑 테이블
};

#endif // ndef __SOCKETUTILS_H__