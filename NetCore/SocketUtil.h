
//***************************************************************************
// SocketUtil.h : interface for the CSocketUtil class.
//
//***************************************************************************

#ifndef __SOCKETUTIL_H__
#define __SOCKETUTIL_H__

#define _DEBUG_KR

#include <list>
using namespace std;

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

//***************************************************************************
//
typedef struct _WINSOCK_ERRORCODE_INFO
{
	int				nErrorCode;
	const TCHAR*	ptszErrMsg;
} WINSOCK_ERRORCODE_INFO, *PWINSOCK_ERRORCODE_INFO;

BOOL	OpenSocketLib();
BOOL	CloseSocketLib();

BOOL	IPToAddr(const int nAf, const TCHAR* ptszHostAddress, void* Dest);
BOOL	AddrToIP(const int nAf, const void* Src, TCHAR* ptszHostAddress, socklen_t nSize);

void	IPv4ToIPv6(const struct in_addr IPv4, struct in_addr6& IPv6);

BOOL	GetSockAddrIn(const TCHAR* ptszHostName, const int nPort, std::list<addrinfo> &SockAddrList);

const TCHAR*	GetErrMsgToWinsockErrCodeEn(const int nErrorCode);
const TCHAR*	GetErrMsgToWinsockErrCodeKr(const int nErrorCode);
void	ReportError(const TCHAR* ptszInOperationDesc, const int nErrorCode);

#endif // ndef __SOCKETUTIL_H__
