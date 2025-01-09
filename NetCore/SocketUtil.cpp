
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
	{ WSAEINTR,					_T("�ߴ� �� �Լ� ȣ��.") },
	{ WSAEACCES,				_T("��� ������ �źεǾ����ϴ�.") },
	{ WSAEFAULT,				_T("�߸��� �ּ�.") },
	{ WSAEINVAL,				_T("�߸��� �μ�.") },
	{ WSAEMFILE,				_T("�����ִ� ������ �ʹ� �����ϴ�.") },
	{ WSAEWOULDBLOCK,			_T("������ ���ܵ˴ϴ�.") },
	{ WSAEINPROGRESS,			_T("�۾��� ���� ������.") },
	{ WSAEALREADY,				_T("�۾��� �̹� ������.") },
	{ WSAENOTSOCK,				_T("������ �ƴ� ���Ͽ����� ����.") },
	{ WSAEDESTADDRREQ,			_T("������ �ּ� �ʿ�.") },
	{ WSAEMSGSIZE,				_T("�޽����� �ʹ� ��ϴ�.") },
	{ WSAEPROTOTYPE,			_T("���Ͽ� ���� �������� ������ �߸��Ǿ����ϴ�.") },
	{ WSAENOPROTOOPT,			_T("�߸��� �������� �ɼ�.") },
	{ WSAEPROTONOSUPPORT,		_T("�������� �ʴ� ��������") },
	{ WSAESOCKTNOSUPPORT,		_T("���� ������ ���� ���� �ʽ��ϴ�.") },
	{ WSAEOPNOTSUPP,			_T("�۾��� ���� ���� �ʽ��ϴ�.") },
	{ WSAEPFNOSUPPORT,			_T("�������� ��ǰ�� ���� ���� �ʽ��ϴ�.") },
	{ WSAEAFNOSUPPORT,			_T("�������� �йи��� ���� ���� �ʴ� �ּ�.") },
	{ WSAEADDRINUSE,			_T("�ּ� �̹� ��� �մϴ�.") },
	{ WSAEADDRNOTAVAIL,			_T("��û�� �ּҸ� �Ҵ��� �� �����ϴ�.") },
	{ WSAENETDOWN,				_T("��Ʈ��ũ �ٿ� �Ǿ����ϴ�.") },
	{ WSAENETUNREACH,			_T("��Ʈ��ũ�� ������ �� �����ϴ�.") },
	{ WSAENETRESET,				_T("��Ʈ��ũ �缳������ ������ ���������ϴ�.") },
	{ WSAECONNABORTED,			_T("����Ʈ���� ������ ������ �ߴ� �Ǿ����ϴ�.") },
	{ WSAECONNRESET,			_T("���� ���� ���� ȣ��Ʈ�� ���� ������ ������ϴ�.") },
	{ WSAENOBUFS,				_T("��� ������ ���� ������ �����ϴ�.") },
	{ WSAEISCONN,				_T("������ �̹� ���� �Ǿ� �ֽ��ϴ�.") },
	{ WSAENOTCONN,				_T("���� ���� �Ǿ� ���� �ʽ��ϴ�.") },
	{ WSAESHUTDOWN,				_T("������ ���� �� �Ŀ� ���� �� �����ϴ�.") },
	{ WSAETIMEDOUT,				_T("���� �ð��� �ʰ� �Ǿ����ϴ�.") },
	{ WSAECONNREFUSED,			_T("���� �ź� ��.") },
	{ WSAEHOSTDOWN,				_T("ȣ��Ʈ �ٿ� �Ǿ����ϴ�.") },
	{ WSAEHOSTUNREACH,			_T("ȣ��Ʈ�� ��ΰ� �����ϴ�.") },
	{ WSAEPROCLIM,				_T("���μ����� �ʹ� �����ϴ�.") },
	{ WSASYSNOTREADY,			_T("��Ʈ��ũ ���� �ý����� ����� �� �����ϴ�.") },
	{ WSAVERNOTSUPPORTED,		_T("Winsock.dll ������ ������ ������ϴ�.") },
	{ WSANOTINITIALISED,		_T("���� ���� ���� ���� �� WSAStartup�� ���������� ���� �˴ϴ�.") },
	{ WSAEDISCON,				_T("���� ���� ���� ��.") },
	{ WSATYPE_NOT_FOUND,		_T("Ŭ���� ������ ã�� �� �����ϴ�.") },
	{ WSAHOST_NOT_FOUND,		_T("ȣ��Ʈ�� ã�� �� �����ϴ�.") },
	{ WSATRY_AGAIN,				_T("�ŷ��� �� ���� ȣ��Ʈ�� ã�� �� �����ϴ�.") },
	{ WSANO_RECOVERY,			_T("������ �� ���� �����Դϴ�.") },
	{ WSANO_DATA,				_T("��ȿ�� �̸�, ��û �� ������ ������ ���ڵ尡 �����ϴ�.") },
	{ WSA_INVALID_HANDLE,		_T("������ �̺�Ʈ ��ü �ڵ��� �߸� �Ǿ����ϴ�.") },
	{ WSA_INVALID_PARAMETER,	_T("�ϳ� �̻��� �Ű� ������ �ùٸ��� �ʽ��ϴ�.") },
	{ WSA_IO_INCOMPLETE,		_T("Overlapped I/O �̺�Ʈ ��ü�� �뺸 ����") },
	{ WSA_IO_PENDING,			_T("Overlapped �۾� ���߿� �ϼ�") },
	{ WSA_NOT_ENOUGH_MEMORY,	_T("��� ������ �޸𸮰� ���� �մϴ�.") },
	{ WSA_OPERATION_ABORTED,	_T("Overlapped �۾� �ߴ�") },
	//	{ WSAINVALIDPROCTABLE,		_T("�߸� �� ���ν��� ���� ������ ���̺��Դϴ�.") },
	//	{ WSAINVALIDPROVIDER,		_T("�߸� �� ���� �������� ���� ��ȣ�Դϴ�.") },
	//	{ WSAPROVIDERFAILEDINIT,	_T("���� �����ڸ� �ʱ�ȭ�� �� �����ϴ�.") },
	{ WSASYSCALLFAILURE,		_T("�ý��� ȣ�� ����") }
};

// ******************************************************************************************
//
// Description : ���� ���̺귯�� ���� : ���� ���̺귯���� �ʱ�ȭ �Ѵ�.
// Parameters : ����
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
// Description : ���� ���̺귯�� ����
// Parameters : ����
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
// Description : Dotted-Decimal Notation�� �ּҰ����� ��ũ��ũ ����Ʈ ������ 32��Ʈ �� �Ǵ� 128��Ʈ �� ����ü�� ��´�.
// Parameters
//		- const int nAf : The address family(AF_INET/AF_INET6)
//		- LPCTSTR lptszHostAddress : Dotted-Decimal Notation�� �ּҰ�
//		- void* Dest : ��ũ��ũ ����Ʈ ������ 32��Ʈ ��(in_addr) �Ǵ� 128��Ʈ ��(in_addr6) ����ü
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
// Description : ��ũ��ũ ����Ʈ ������ 32��Ʈ �Ǵ� 128��Ʈ �� ����ü�� Dotted-Decimal Notation�� �ּҰ��� ��´�.
// Parameters
//		- const int nAf : The address family(AF_INET/AF_INET6)
//		- const void *Src : ��ũ��ũ ����Ʈ ������ 32��Ʈ ��(in_addr) �Ǵ� 128��Ʈ ��(in_addr6) ����ü
//		- LPTSTR lptszHostAddress : Dotted-Decimal Notation�� �ּ� ���ڿ�
//		- socklen_t nSize : Dotted-Decimal Notation�� �ּ� ���ڿ� ����
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
// Description : IPv4 �ּҸ� IPv6 �ּҷ� ��ȯ
// Parameters
//		- const struct in_addr IPv4 : 32��Ʈ�� IP �ּҸ� �����ϱ� ���� ����ü
//		- struct in_addr6& IPv6 : 128��Ʈ�� IP �ּҸ� �����ϱ� ���� ����ü
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
// Description : ȣ��Ʈ ��� ��Ʈ ��ȣ�� �̿��Ͽ� sockaddr_in ��ü �Ǵ� sockaddr_in6 ��ü �� ���ϱ�
// Parameters
//		- LPCTSTR lptszHostName : ȣ��Ʈ ��
//		- const int nPort : ��Ʈ ��ȣ
//		- sockaddr_in& SockAddrIn : sockaddr_in ��ü
//		- sockaddr_in6& SockAddrIn6 : sockaddr_in6 ��ü
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
// Description : Winsock ���� �ڵ忡 �ش��ϴ� ���� ���� �޼��� ���
// Parameters
//		- const int nErrorCode : ���� �ڵ�
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
// Description : Winsock ���� �ڵ忡 �ش��ϴ� �ѱ� ���� �޼��� ���
// Parameters
//		- const int nErrorCode : ���� �ڵ�
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
// Description : ���� ������ �����ش�
// Parameters
//		- LPCTSTR lptszInOperationDesc : ó�� �ܰ� ���
//		- const int nErrorCode : ���� �ڵ�
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
