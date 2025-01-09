
//***************************************************************************
// SocketUtil.cpp: implementation of the CSocketUtil class.
//
//***************************************************************************

#include "stdafx.h"
#include "SocketUtil.h"

WINSOCK_ERRORCODE_INFO g_ErrTableEn[] =
{
	{ WSAEINTR,					_T("Interrupted function call") },
	{ WSAEACCES,				_T("Permission denied") },
	{ WSAEFAULT,				_T("Bad address") },
	{ WSAEINVAL,				_T("Invalid argument") },
	{ WSAEMFILE,				_T("Too many open files") },
	{ WSAEWOULDBLOCK,			_T("Socket would block") },
	{ WSAEINPROGRESS,			_T("Operation now in progress") },
	{ WSAEALREADY,				_T("Operation already in progress") },
	{ WSAENOTSOCK,				_T("Socket operation on nonsocket") },
	{ WSAEDESTADDRREQ,			_T("Destination address required") },
	{ WSAEMSGSIZE,				_T("Message too long") },
	{ WSAEPROTOTYPE,			_T("Protocol wrong type for socket") },
	{ WSAENOPROTOOPT,			_T("Bad protocol option") },
	{ WSAEPROTONOSUPPORT,		_T("Protocol not supported") },
	{ WSAESOCKTNOSUPPORT,		_T("Socket type not supported") },
	{ WSAEOPNOTSUPP,			_T("Operation not supported") },
	{ WSAEPFNOSUPPORT,			_T("Protocol family not supported") },
	{ WSAEAFNOSUPPORT,			_T("Address family not supported by protocol family") },
	{ WSAEADDRINUSE,			_T("Address already in use") },
	{ WSAEADDRNOTAVAIL,			_T("Cannot assign requested address") },
	{ WSAENETDOWN,				_T("Network is down") },
	{ WSAENETUNREACH,			_T("Network is unreachable") },
	{ WSAENETRESET,				_T("Network dropped connection on reset") },
	{ WSAECONNABORTED,			_T("Software caused connection abort") },
	{ WSAECONNRESET,			_T("Connection reset by peer") },
	{ WSAENOBUFS,				_T("No buffer space available") },
	{ WSAEISCONN,				_T("Socket is already connected") },
	{ WSAENOTCONN,				_T("Socket is not connected") },
	{ WSAESHUTDOWN,				_T("Cannot send after socket shutdown") },
	{ WSAETIMEDOUT,				_T("Connection timed out") },
	{ WSAECONNREFUSED,			_T("Connection refused") },
	{ WSAEHOSTDOWN,				_T("Host is down") },
	{ WSAEHOSTUNREACH,			_T("No route to host") },
	{ WSAEPROCLIM,				_T("Too many processes") },
	{ WSASYSNOTREADY,			_T("Network subsystem is unavailable") },
	{ WSAVERNOTSUPPORTED,		_T("Winsock.dll version out of range") },
	{ WSANOTINITIALISED,		_T("Successful WSAStartup not yet performed") },
	{ WSAEDISCON,				_T("Graceful shutdown in progress") },
	{ WSATYPE_NOT_FOUND,		_T("Class type not found") },
	{ WSAHOST_NOT_FOUND,		_T("Host not found") },
	{ WSATRY_AGAIN,				_T("Nonauthoritative host not found") },
	{ WSANO_RECOVERY,			_T("This is a nonrecoverable error") },
	{ WSANO_DATA,				_T("Valid name, no data record of requested type") },
	{ WSA_INVALID_HANDLE,		_T("Specified event object handle is invalid") },
	{ WSA_INVALID_PARAMETER,	_T("One or more parameters are invalid") },
	{ WSA_IO_INCOMPLETE,		_T("Overlapped I/O event object not in signaled state") },
	{ WSA_IO_PENDING,			_T("Overlapped operations will complete later") },
	{ WSA_NOT_ENOUGH_MEMORY,	_T("Insufficient memory available") },
	{ WSA_OPERATION_ABORTED,	_T("Overlapped operation aborted") },
	//	{ WSAINVALIDPROCTABLE,		_T("Invalid procedure table from service provider") },
	//	{ WSAINVALIDPROVIDER,		_T("Invalid service provider version number") },
	//	{ WSAPROVIDERFAILEDINIT,	_T("Unable to initialize a service provider") },
	{ WSASYSCALLFAILURE,		_T("System call failure") }
};

WINSOCK_ERRORCODE_INFO g_ErrTableKr[] =
{
	{ WSAEINTR,					_T("중단 된 함수 호출.") },
	{ WSAEACCES,				_T("사용 권한이 거부되었습니다.") },
	{ WSAEFAULT,				_T("잘못된 주소.") },
	{ WSAEINVAL,				_T("잘못된 인수.") },
	{ WSAEMFILE,				_T("열려있는 파일이 너무 많습니다.") },
	{ WSAEWOULDBLOCK,			_T("소켓이 차단됩니다.") },
	{ WSAEINPROGRESS,			_T("작업이 현재 진행중.") },
	{ WSAEALREADY,				_T("작업이 이미 진행중.") },
	{ WSAENOTSOCK,				_T("소켓이 아닌 소켓에서의 조작.") },
	{ WSAEDESTADDRREQ,			_T("목적지 주소 필요.") },
	{ WSAEMSGSIZE,				_T("메시지가 너무 깁니다.") },
	{ WSAEPROTOTYPE,			_T("소켓에 대한 프로토콜 유형이 잘못되었습니다.") },
	{ WSAENOPROTOOPT,			_T("잘못된 프로토콜 옵션.") },
	{ WSAEPROTONOSUPPORT,		_T("지원되지 않는 프로토콜") },
	{ WSAESOCKTNOSUPPORT,		_T("소켓 형식이 지원 되지 않습니다.") },
	{ WSAEOPNOTSUPP,			_T("작업이 지원 되지 않습니다.") },
	{ WSAEPFNOSUPPORT,			_T("프로토콜 제품군 지원 되지 않습니다.") },
	{ WSAEAFNOSUPPORT,			_T("프로토콜 패밀리가 지원 하지 않는 주소.") },
	{ WSAEADDRINUSE,			_T("주소 이미 사용 합니다.") },
	{ WSAEADDRNOTAVAIL,			_T("요청한 주소를 할당할 수 없습니다.") },
	{ WSAENETDOWN,				_T("네트워크 다운 되었습니다.") },
	{ WSAENETUNREACH,			_T("네트워크에 연결할 수 없습니다.") },
	{ WSAENETRESET,				_T("네트워크 재설정으로 연결이 끊어졌습니다.") },
	{ WSAECONNABORTED,			_T("소프트웨어 때문에 연결이 중단 되었습니다.") },
	{ WSAECONNRESET,			_T("기존 연결 원격 호스트에 의해 강제로 끊겼습니다.") },
	{ WSAENOBUFS,				_T("사용 가능한 버퍼 공간이 없습니다.") },
	{ WSAEISCONN,				_T("소켓이 이미 연결 되어 있습니다.") },
	{ WSAENOTCONN,				_T("소켓 연결 되어 있지 않습니다.") },
	{ WSAESHUTDOWN,				_T("소켓이 종료 된 후에 보낼 수 없습니다.") },
	{ WSAETIMEDOUT,				_T("연결 시간이 초과 되었습니다.") },
	{ WSAECONNREFUSED,			_T("연결 거부 됨.") },
	{ WSAEHOSTDOWN,				_T("호스트 다운 되었습니다.") },
	{ WSAEHOSTUNREACH,			_T("호스트에 경로가 없습니다.") },
	{ WSAEPROCLIM,				_T("프로세스가 너무 많습니다.") },
	{ WSASYSNOTREADY,			_T("네트워크 하위 시스템을 사용할 수 없습니다.") },
	{ WSAVERNOTSUPPORTED,		_T("Winsock.dll 버전이 범위를 벗어났습니다.") },
	{ WSANOTINITIALISED,		_T("아직 수행 되지 않을 때 WSAStartup을 성공적으로 수행 됩니다.") },
	{ WSAEDISCON,				_T("정상 종료 진행 중.") },
	{ WSATYPE_NOT_FOUND,		_T("클래스 유형을 찾을 수 없습니다.") },
	{ WSAHOST_NOT_FOUND,		_T("호스트를 찾을 수 없습니다.") },
	{ WSATRY_AGAIN,				_T("신뢰할 수 없는 호스트를 찾을 수 없습니다.") },
	{ WSANO_RECOVERY,			_T("복구할 수 없는 오류입니다.") },
	{ WSANO_DATA,				_T("유효한 이름, 요청 된 형식의 데이터 레코드가 없습니다.") },
	{ WSA_INVALID_HANDLE,		_T("지정한 이벤트 개체 핸들이 잘못 되었습니다.") },
	{ WSA_INVALID_PARAMETER,	_T("하나 이상의 매개 변수가 올바르지 않습니다.") },
	{ WSA_IO_INCOMPLETE,		_T("Overlapped I/O 이벤트 개체에 통보 상태") },
	{ WSA_IO_PENDING,			_T("Overlapped 작업 나중에 완성") },
	{ WSA_NOT_ENOUGH_MEMORY,	_T("사용 가능한 메모리가 부족 합니다.") },
	{ WSA_OPERATION_ABORTED,	_T("Overlapped 작업 중단") },
	//	{ WSAINVALIDPROCTABLE,		_T("잘못 된 프로시저 서비스 공급자 테이블입니다.") },
	//	{ WSAINVALIDPROVIDER,		_T("잘못 된 서비스 공급자의 버전 번호입니다.") },
	//	{ WSAPROVIDERFAILEDINIT,	_T("서비스 공급자를 초기화할 수 없습니다.") },
	{ WSASYSCALLFAILURE,		_T("시스템 호출 오류") }
};

// ******************************************************************************************
//
// Description : 소켓 라이브러리 시작 : 소켓 라이브러리를 초기화 한다.
// Parameters : 없음
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
BOOL OpenSocketLib()
{
	WORD	wVersion;
	WSADATA	wsa;
	int		nRet;

	wVersion = MAKEWORD(2, 2);
	nRet = WSAStartup(wVersion, &wsa);

	if( nRet || LOBYTE(wsa.wVersion) != 2 && HIBYTE(wsa.wVersion) != 2 ) return FALSE;

	return TRUE;
}

// ******************************************************************************************
//
// Description : 소켓 라이브러리 비우기
// Parameters : 없음
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
BOOL CloseSocketLib()
{
	int nRet = WSACleanup();
	if( nRet == 0 ) return FALSE;

	return TRUE;
}

// ******************************************************************************************
//
// Description : Dotted-Decimal Notation의 주소값으로 네크워크 바이트 순서의 32비트 값 또는 128비트 값 구조체를 얻는다.
// Parameters
//		- const int nAf : The address family(AF_INET/AF_INET6)
//		- LPCTSTR lptszHostAddress : Dotted-Decimal Notation의 주소값
//		- void* Dest : 네크워크 바이트 순서의 32비트 값(in_addr) 또는 128비트 값(in_addr6) 구조체
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
BOOL IPToAddr(const int nAf, LPCTSTR lptszHostAddress, void* Dest)
{
	int		nRet = -1;
	int		nLen = 0;
	struct sockaddr_storage ss;

	nLen = sizeof(ss);

	TCHAR tszHostAddress[IP6_STRLEN + 1];

	ZeroMemory(&ss, sizeof(ss));

	_tcscpy_s(tszHostAddress, _countof(tszHostAddress), lptszHostAddress);

	if( nRet = WSAStringToAddress(tszHostAddress, nAf, NULL, (struct sockaddr *)&ss, &nLen) == 0 )
	{
		switch( nAf )
		{
			case AF_INET:
				struct in_addr InAddr;

				InAddr = ((struct sockaddr_in *)&ss)->sin_addr;
				memcpy(Dest, &InAddr, sizeof(struct in_addr));

				break;
			case AF_INET6:
				struct in6_addr InAddr6;

				InAddr6 = ((struct sockaddr_in6 *)&ss)->sin6_addr;
				memcpy(Dest, &InAddr6, sizeof(struct in_addr6));

				break;
		}
	}

	return nRet == 0 ? TRUE : FALSE;
}

// ******************************************************************************************
//
// Description : 네크워크 바이트 순서의 32비트 또는 128비트 값 구조체로 Dotted-Decimal Notation의 주소값을 얻는다.
// Parameters
//		- const int nAf : The address family(AF_INET/AF_INET6)
//		- const void *Src : 네크워크 바이트 순서의 32비트 값(in_addr) 또는 128비트 값(in_addr6) 구조체
//		- LPTSTR lptszHostAddress : Dotted-Decimal Notation의 주소 문자열
//		- socklen_t nSize : Dotted-Decimal Notation의 주소 문자열 길이
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
BOOL AddrToIP(const int nAf, const void* Src, LPTSTR lptszHostAddress, socklen_t nSize)
{
	int		nRet = -1;
	struct sockaddr_storage ss;

	unsigned long ulSize = nSize;

	ZeroMemory(&ss, sizeof(ss));

	ss.ss_family = nAf;
	switch( nAf )
	{
		case AF_INET:
			((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)Src;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)Src;
			break;
		default:
			return FALSE;
	}

	if( (nRet = WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, lptszHostAddress, &ulSize)) != 0 )
		return FALSE;

	return TRUE;
}

// ******************************************************************************************
//
// Description : IPv4 주소를 IPv6 주소로 변환
// Parameters
//		- const struct in_addr IPv4 : 32비트의 IP 주소를 저장하기 위한 구조체
//		- struct in_addr6& IPv6 : 128비트의 IP 주소를 저장하기 위한 구조체
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
void IPv4ToIPv6(const struct in_addr IPv4, struct in_addr6& IPv6)
{
	memset(&IPv6, 0x00, sizeof(IPv6));

	IPv6.s6_addr[10] = IPv6.s6_addr[11] = 0xFF;

	memcpy(&IPv6.s6_addr[12], &IPv4.s_addr, sizeof(IPv4));
}

// ******************************************************************************************
//
// Description : 호스트 명과 포트 번호를 이용하여 sockaddr_in 객체 또는 sockaddr_in6 객체 값 구하기
// Parameters
//		- LPCTSTR lptszHostName : 호스트 명
//		- const int nPort : 포트 번호
//		- sockaddr_in& SockAddrIn : sockaddr_in 객체
//		- sockaddr_in6& SockAddrIn6 : sockaddr_in6 객체
// Return Type : BOOL
// Reference : 
//
// ******************************************************************************************
BOOL GetSockAddrIn(LPCTSTR lptszHostName, const int nPort, std::list<addrinfo>& SockAddrList)
{
	int		nError = -1;
	int		nAddrLen = -1;
	char	szHostName[PC_NAME_STRLEN];
	char	szPort[PORT_STRLEN];
	char*	pszHostName = NULL;

	struct addrinfo Hints;
	struct addrinfo *pResult = NULL;
	struct addrinfo *pAddrInfo = NULL;

	memset(&Hints, 0, sizeof(addrinfo));
	Hints.ai_flags = AI_PASSIVE;
	Hints.ai_family = AF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;

	if( lptszHostName )
	{
#ifdef _UNICODE
		WideCharToMultiByte(CP_ACP, 0, lptszHostName, -1, szHostName, _countof(szHostName), NULL, NULL);
		pszHostName = szHostName;
#else
		pszHostName = lptszHostName;
#endif
	}

	_itoa_s(nPort, szPort, _countof(szPort), 10);
	
	if( (nError = getaddrinfo(pszHostName, szPort, &Hints, &pResult)) != 0 )
	{
		_ASSERT(0);
		return FALSE;
	}

	for( pAddrInfo = pResult; pAddrInfo != NULL; pAddrInfo = pAddrInfo->ai_next )
	{
		addrinfo AddrInfo;

		memcpy(&AddrInfo, pAddrInfo, sizeof(addrinfo));

		SockAddrList.push_back(AddrInfo);
	}

	freeaddrinfo(pResult);

	return TRUE;
}

// ******************************************************************************************
//
// Description : Winsock 에러 코드에 해당하는 영문 오류 메세지 얻기
// Parameters
//		- const int nErrorCode : 에러 코드
// Return Type : TCHAR*
// Reference : 
//
// ******************************************************************************************
TCHAR* GetErrMsgToWinsockErrCodeEn(const int nErrorCode)
{
	for( int i = 0; i < 50; i++ )
	{
		if( g_ErrTableEn[i].nErrorCode == nErrorCode )
			return g_ErrTableEn[i].ptszErrMsg;
	}

	return NULL;
}

// ******************************************************************************************
//
// Description : Winsock 에러 코드에 해당하는 한글 오류 메세지 얻기
// Parameters
//		- const int nErrorCode : 에러 코드
// Return Type : TCHAR*
// Reference : 
//
// ******************************************************************************************
TCHAR* GetErrMsgToWinsockErrCodeKr(const int nErrorCode)
{
	for( int i = 0; i < 50; i++ )
	{
		if( g_ErrTableKr[i].nErrorCode == nErrorCode )
			return g_ErrTableKr[i].ptszErrMsg;
	}

	return NULL;
}

// ******************************************************************************************
//
// Description : 에러 내용을 보여준다
// Parameters
//		- LPCTSTR lptszInOperationDesc : 처리 단계 명시
//		- const int nErrorCode : 에러 코드
// Return Type : void
// Reference : 
//
// ******************************************************************************************
void ReportError(LPCTSTR lptszInOperationDesc, const int nErrorCode)
{
	TCHAR  tszBuffer[MAX_BUFFER_SIZE];
	TCHAR* ptszMsgBuffer = NULL;

#ifdef _DEBUG_KR
	ptszMsgBuffer = GetErrMsgToWinsockErrCodeEn(nErrorCode);
#elif _DEBUG_EN
	ptszMsgBuffer = GetErrMsgToWinsockErrCodeKr(nErrorCode);
#else
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		nErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		ptszMsgBuffer,
		0, NULL);
#endif

	_stprintf_s(tszBuffer, _countof(tszBuffer), _T("Error %s: %d- %s"), lptszInOperationDesc, nErrorCode, ptszMsgBuffer);

#ifdef _DEBUGLOG
	g_SysLog.EventLog(tszBuffer);
#endif 
}
