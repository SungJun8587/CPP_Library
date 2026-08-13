
//***************************************************************************
// NetAddress.h : interface for the CNetAddress class.
//
//***************************************************************************

#ifndef __NETADDRESS_H__
#define __NETADDRESS_H__

//***************************************************************************
// @class CNetAddress
// @brief IPv4 소켓 주소(SOCKADDR_IN) 패킹 및 IP/Port 문자열 변환 래퍼 클래스
//***************************************************************************
class CNetAddress
{
public:
	CNetAddress() = default;
	CNetAddress(SOCKADDR_IN sockAddr);
	CNetAddress(_tstring ip, uint16 port);

	//***************************************************************************
	// @brief 내부 SOCKADDR_IN 구조체의 참조를 반환합니다.
	//***************************************************************************
	SOCKADDR_IN& GetSockAddr() { return _sockAddr; }

	_tstring		GetIpAddress();

	//***************************************************************************
	// @brief 포트 번호를 반환합니다 (Host Byte Order).
	//***************************************************************************
	uint16			GetPort() { return ::ntohs(_sockAddr.sin_port); }

public:
	static IN_ADDR	Ip2Address(const TCHAR* ip);

private:
	SOCKADDR_IN		_sockAddr = {}; // 소켓 주소(IP, Port, Family) 정보 구조체
};

#endif // ndef __NETADDRESS_H__