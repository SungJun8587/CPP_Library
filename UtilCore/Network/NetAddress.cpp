
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
CNetAddress::CNetAddress(const SOCKADDR_IN& sockAddr) : _sockAddr(sockAddr)
{
}

//***************************************************************************
// @brief IP 문자열 및 Port 기반 생성자
// @param ip IP 주소 문자열
// @param port 포트 번호 (Host Byte Order)
// @note IP 문자열 파싱에 실패하면 주소는 0.0.0.0으로 남고 IsValid()가 false가 됩니다.
//***************************************************************************
CNetAddress::CNetAddress(const _tstring& ip, uint16 port)
{
	::memset(&_sockAddr, 0, sizeof(_sockAddr));
	_sockAddr.sin_family = AF_INET;
	_valid = TryIp2Address(ip.c_str(), _sockAddr.sin_addr);
	if( _valid == false )
		LOG_ERROR(_T("[CNetAddress] invalid IPv4 address string: '%s'"), ip.c_str());
	_sockAddr.sin_port = ::htons(port);
}

//***************************************************************************
// @brief 저장된 소켓 주소로부터 IP 주소 문자열을 추출합니다.
// @return 변환된 IP 주소 문자열
//***************************************************************************
_tstring CNetAddress::GetIpAddress() const
{
	TCHAR buffer[46] = {};  // IPv6 최대 길이 대비 (IP6_STRLEN 등 프로젝트 상수 활용 권장)
	if( CSocketUtils::AddrToIP(AF_INET, &_sockAddr.sin_addr, buffer, _countof(buffer)) == false )
		return _tstring();	// 변환 실패 시 초기화되지 않은 버퍼 대신 빈 문자열을 반환한다.

	return _tstring(buffer);
}

//***************************************************************************
// @brief 문자열 IP 주소를 네트워크 바이트 오더 형태의 IN_ADDR 구조체로 변환합니다.
// @param ip 변환할 IP 주소 문자열 (문자열 포인터)
// @param outAddress [out] 변환된 IN_ADDR (실패 시 0.0.0.0)
// @return 변환 성공 여부
//***************************************************************************
bool CNetAddress::TryIp2Address(const TCHAR* ip, IN_ADDR& outAddress)
{
	outAddress = {};

	if( ip == nullptr )
		return false;

	if( CSocketUtils::IPToAddr(AF_INET, ip, &outAddress) == false )
	{
		outAddress = {};	// 부분 기록 가능성에 대비해 0.0.0.0으로 정리
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 문자열 IP 주소를 IN_ADDR로 변환합니다(기존 호환 API).
// @param ip 변환할 IP 주소 문자열 (문자열 포인터)
// @return 변환된 IN_ADDR 구조체 (실패 시 0.0.0.0 + 에러 로그)
// @note 실패 여부를 구분해야 하는 호출부는 TryIp2Address()를 사용할 것.
//***************************************************************************
IN_ADDR CNetAddress::Ip2Address(const TCHAR* ip)
{
	IN_ADDR address{};

	if( TryIp2Address(ip, address) == false )
		LOG_ERROR(_T("[CNetAddress] invalid IPv4 address string: '%s' - falling back to 0.0.0.0"), (ip != nullptr) ? ip : _T("(null)"));

	return address;
}