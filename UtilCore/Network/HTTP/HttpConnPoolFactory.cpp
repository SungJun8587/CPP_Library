
//***************************************************************************
// HttpConnPoolFactory.cpp : implementation of the HTTP connection pool factory helpers.
//
//***************************************************************************

#include "pch.h"
#include "HttpConnPoolFactory.h"

//***************************************************************************
// @brief hostname을 CNetAddress로 변환하는 기본 DNS resolve 함수 (CSocketUtils::
//        GetSockAddrIn 기반). CHttpConnPoolManager 생성 시 HttpDnsResolveFn으로
//        그대로 넘기면 된다.
// @param hostname 조회할 hostname (DNS 이름은 RFC 1035상 항상 ASCII이므로,
//        UNICODE 빌드에서도 바이트를 그대로 TCHAR로 widen하는 게 안전하다 —
//        임의의 유니코드 텍스트일 수 있는 폼 필드 값과는 다른 경우.)
// @param port 조회할 포트
// @param outAddr [OUT] 조회 성공 시 채워지는 주소 (IPv4 첫 결과만 사용)
// @return bool 성공 여부
//***************************************************************************
bool ResolveHostnameToNetAddress(const std::string& hostname, uint16 port, CNetAddress& outAddr)
{
	_tstring tHostname;
	tHostname.reserve(hostname.size());
	for( char c : hostname )
		tHostname.push_back(static_cast<TCHAR>(static_cast<unsigned char>(c)));

	std::list<addrinfo> results;
	if( !CSocketUtils::GetSockAddrIn(tHostname.c_str(), static_cast<int>(port), results) )
		return false;

	// GetSockAddrIn()이 만든 ai_addr 사본은 호출자 소유다. 주소 재해석 스레드가 주기적으로 이 함수를
	// 부르므로 반드시 해제해야 누수가 쌓이지 않는다.
	bool found = false;
	for( const addrinfo& info : results )
	{
		if( info.ai_family == AF_INET && info.ai_addr != nullptr )
		{
			SOCKADDR_IN sockAddr = *reinterpret_cast<SOCKADDR_IN*>(info.ai_addr);
			outAddr = CNetAddress(sockAddr);
			found = true;
			break;
		}
	}

	CSocketUtils::FreeSockAddrIn(results);
	return found;
}