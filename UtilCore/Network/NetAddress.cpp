
//***************************************************************************
// NetAddress.cpp: implementation of the CNetAddress class.
//
//***************************************************************************

#include "pch.h"
#include "NetAddress.h"

//***************************************************************************
// @brief SOCKADDR_IN 구조체 기반 생성자
// @param sockAddr 초기화할 소켓 주소 구조체
//***************************************************************************
CNetAddress::CNetAddress(SOCKADDR_IN sockAddr) : _sockAddr(sockAddr)
{
}

//***************************************************************************
// @brief IP 문자열 및 Port 기반 생성자
// @param ip IP 주소 문자열
// @param port 포트 번호 (Host Byte Order)
//***************************************************************************
CNetAddress::CNetAddress(_tstring ip, uint16 port)
{
	::memset(&_sockAddr, 0, sizeof(_sockAddr));
	_sockAddr.sin_family = AF_INET;
	_sockAddr.sin_addr = Ip2Address(ip.c_str());
	_sockAddr.sin_port = ::htons(port);
}

//***************************************************************************
// @brief 저장된 소켓 주소로부터 IP 주소 문자열을 추출합니다.
// @return 변환된 IP 주소 문자열
//***************************************************************************
_tstring CNetAddress::GetIpAddress()
{
	TCHAR buffer[46];  // IPv6 최대 길이 대비 (IP6_STRLEN 등 프로젝트 상수 활용 권장)
	CSocketUtils::AddrToIP(AF_INET, &_sockAddr.sin_addr, buffer, _countof(buffer));
	return _tstring(buffer);
}

//***************************************************************************
// @brief 문자열 IP 주소를 네트워크 바이트 오더 형태의 IN_ADDR 구조체로 변환합니다.
// @param ip 변환할 IP 주소 문자열 (문자열 포인터)
// @return 변환된 IN_ADDR 구조체
//***************************************************************************
IN_ADDR CNetAddress::Ip2Address(const TCHAR* ip)
{
	IN_ADDR address{};
	CSocketUtils::IPToAddr(AF_INET, ip, &address);
	return address;
}