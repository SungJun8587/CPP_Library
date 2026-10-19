//***************************************************************************
// NetAddress.h : interface for the CNetAddress class.
//
//***************************************************************************

#ifndef UC_NETADDRESS_H
#define UC_NETADDRESS_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseRedefineDataType.h>

//***************************************************************************
// @class CNetAddress
// @brief IPv4/IPv6 소켓 주소(SOCKADDR_INET) 패킹 및 IP/Port 문자열 변환 래퍼 클래스
//
// @details
//      내부 저장소는 SOCKADDR_INET(28바이트 union)이며, 주소 체계(AF_INET/AF_INET6)는
//      GetFamily()로 구분합니다.
//
//      [유효성] 기본 생성된 객체와 파싱·검증에 실패한 객체는 IsValid()가 false이며
//      family가 0(AF_UNSPEC)입니다. 무효 주소는 GetSockAddr()가 nullptr,
//      GetSockAddrLen()이 0이고 Bind()/Connect()에서 거부됩니다.
//
//      [듀얼스택] 서버를 AF_INET6 와일드카드(AnyV6(), "::")로 열면 IPv4/IPv6 클라이언트를
//      모두 받습니다. 커널은 IPv4 클라이언트를 v4-mapped IPv6(::ffff:a.b.c.d)로 보고하며
//      (CSocketUtils::GetPeerAddress/Accept의 결과), 서비스 계층의 리스너는 이를 Unmap()해서
//      세션의 GetNetAddress()에는 순수 IPv4 주소로 담습니다.
//***************************************************************************
class CNetAddress
{
public:
	CNetAddress();
	CNetAddress(const SOCKADDR_IN& sockAddr);	// 암시적 변환을 쓰는 호출부가 있을 수 있어 explicit은 붙이지 않음
	CNetAddress(const SOCKADDR_IN6& sockAddr);
	CNetAddress(const _tstring& ip, uint16 port);

	//***************************************************************************
	// @brief accept/getpeername/AcceptEx 등이 돌려준 임의의 sockaddr로부터 주소를 만듭니다.
	// @param sockAddr 소켓 주소 포인터 (AF_INET 또는 AF_INET6)
	// @param sockAddrLen sockAddr가 가리키는 버퍼의 유효 길이
	// @return 포인터가 null이거나 지원하지 않는 family이거나 길이가 부족하면 IsValid()가 false인 주소
	//***************************************************************************
	static CNetAddress FromSockAddr(const SOCKADDR* sockAddr, int sockAddrLen);

	//***************************************************************************
	// @brief 사용 가능한 주소(지원하는 family와 완전한 주소 값)인지 반환합니다.
	// @details 기본 생성, IP 문자열 파싱 실패, family가 맞지 않는 SOCKADDR_IN/SOCKADDR_IN6,
	//          잘못된 FromSockAddr() 입력은 모두 false가 됩니다. 무효 주소는
	//          CSocketUtils::Bind()/Connect()가 거부해 "서버가 의도치 않게 모든 인터페이스에
	//          열리는" 상황을 막습니다.
	//***************************************************************************
	bool IsValid() const { return _valid; }

	// @brief 주소 체계를 반환합니다 (AF_INET, AF_INET6, 무효 주소는 0).
	int  GetFamily() const { return _addr.si_family; }

	// @brief 유효한 IPv4 / IPv6 주소인지 반환합니다 (무효 주소는 둘 다 false).
	bool IsIPv4() const { return _valid && _addr.si_family == AF_INET; }
	bool IsIPv6() const { return _valid && _addr.si_family == AF_INET6; }

	// @brief v4-mapped IPv6 주소(::ffff:a.b.c.d)인지 반환합니다.
	bool IsIPv4Mapped() const
	{
		return IsIPv6() && IN6_IS_ADDR_V4MAPPED(&_addr.Ipv6.sin6_addr);
	}

	//***************************************************************************
	// @brief v4-mapped IPv6 주소를 순수 IPv4 주소로 풀어서 반환합니다.
	// @details 듀얼스택 서버가 받은 IPv4 클라이언트 주소(::ffff:a.b.c.d)를 a.b.c.d로
	//          되돌립니다. 그 외 주소는 그대로 복사본을 반환합니다.
	//***************************************************************************
	CNetAddress Unmap() const;

	//***************************************************************************
	// @brief 내부 소켓 주소 구조체를 SOCKADDR 포인터로 반환합니다.
	// @return 유효한 주소면 구조체 포인터, 무효 주소면 nullptr
	// @details bind/connect/ConnectEx에는 GetSockAddrLen()과 함께 넘기십시오.
	//          const CNetAddress&로 받은 파라미터에서도 조회할 수 있도록 const 오버로드를 둡니다.
	//***************************************************************************
	SOCKADDR* GetSockAddr()
	{
		return _valid ? reinterpret_cast<SOCKADDR*>(&_addr) : nullptr;
	}

	const SOCKADDR* GetSockAddr() const
	{
		return _valid ? reinterpret_cast<const SOCKADDR*>(&_addr) : nullptr;
	}

	//***************************************************************************
	// @brief GetSockAddr()가 가리키는 주소 구조체의 길이를 반환합니다 (무효 주소는 0).
	//***************************************************************************
	int GetSockAddrLen() const;

	//***************************************************************************
	// @brief IP 주소 문자열을 반환합니다 (무효 주소이거나 변환에 실패하면 빈 문자열).
	//***************************************************************************
	_tstring GetIpAddress() const;

	//***************************************************************************
	// @brief 포트 번호를 반환합니다 (Host Byte Order, 무효 주소는 0).
	//***************************************************************************
	uint16 GetPort() const;

	//***************************************************************************
	// @brief IPv6 scope id를 반환합니다 (IPv6가 아니거나 무효 주소는 0).
	//***************************************************************************
	uint32 GetScopeId() const;

	//***************************************************************************
	// @brief 루프백 주소인지 반환합니다 (IPv4 127.0.0.0/8, IPv6 ::1, v4-mapped 127.x).
	// @details 127.0.0.1 하나만 비교하지 않고 /8 전체를 검사합니다.
	//***************************************************************************
	bool IsLoopback() const;

	//***************************************************************************
	// @brief 와일드카드 주소(0.0.0.0 또는 ::)인지 반환합니다.
	// @details 무효 주소는 false입니다. IPv6는 순수 "::"만 와일드카드로 봅니다
	//          (v4-mapped ::ffff:0.0.0.0은 해당하지 않음).
	//          서버가 의도치 않게 모든 인터페이스에 열리는지 점검하는 용도.
	//***************************************************************************
	bool IsAny() const;

	//***************************************************************************
	// @brief 주소 체계, IP, 포트가 모두 같은지 비교합니다.
	// @details v4-mapped IPv6 주소는 IPv4 주소로 풀어서 비교하므로 ::ffff:1.2.3.4:80과
	//          1.2.3.4:80은 같은 주소로 봅니다. IPv6는 scope id까지 비교합니다.
	//          sockaddr 구조체 전체를 memcmp하지 않습니다(패딩·flowinfo 값은 무시).
	//          무효 주소끼리는 같고(반사성 유지), 무효 주소와 유효 주소는 다릅니다.
	//***************************************************************************
	bool operator==(const CNetAddress& rhs) const;
	bool operator!=(const CNetAddress& rhs) const { return !(*this == rhs); }

	//***************************************************************************
	// @brief 와일드카드 주소 팩토리.
	// @details 기본 생성된 주소는 무효라 bind()에 쓸 수 없으므로, 바인드용 주소는
	//          family가 채워지는 이 팩토리를 사용합니다.
	//          Any()는 IPv4(0.0.0.0), AnyV6()는 IPv6(::)이며 듀얼스택 서버의 바인드 주소입니다.
	// @param port 포트 번호 (Host Byte Order, 0이면 임시 포트)
	//***************************************************************************
	static CNetAddress Any(uint16 port = 0);
	static CNetAddress AnyV6(uint16 port = 0);

	//***************************************************************************
	// @brief family(AF_INET/AF_INET6)에 맞는 와일드카드 주소를 만듭니다. 그 외 family는 무효 주소.
	//***************************************************************************
	static CNetAddress Wildcard(int family, uint16 port = 0);

	//***************************************************************************
	// @brief 루프백 주소(127.0.0.1 / ::1) 팩토리.
	//***************************************************************************
	static CNetAddress Loopback(uint16 port);
	static CNetAddress LoopbackV6(uint16 port);

	//***************************************************************************
	// @brief 숫자 형식의 IP 문자열을 IN_ADDR/IN6_ADDR로 변환합니다. 실패 시 false(outAddress는 0으로 초기화).
	// @details IPv6 문자열의 scope id("fe80::1%12")는 지원하지 않습니다.
	//***************************************************************************
	static bool TryIp2Address(const TCHAR* ip, IN_ADDR& outAddress);
	static bool TryIp2Address(const TCHAR* ip, IN6_ADDR& outAddress);

	//***************************************************************************
	// 실패 시 0.0.0.0을 반환하고 에러 로그를 남깁니다(기존 호환용). 성공 여부가
	// 필요하면 TryIp2Address()를 사용할 것.
	//***************************************************************************
	static IN_ADDR Ip2Address(const TCHAR* ip);

private:
	//***************************************************************************
	// @brief 주소를 전부 0(family 0)으로 지우고 무효 상태로 만듭니다.
	//***************************************************************************
	void SetInvalid();

	SOCKADDR_INET _addr;	// 소켓 주소(IP, Port, Family) 정보 (si_family로 IPv4/IPv6 구분)
	bool _valid;			// 지원하는 family의 완전한 주소인지 여부 (IsValid() 참고)
};

#endif // UC_NETADDRESS_H