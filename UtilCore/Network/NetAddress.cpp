
//***************************************************************************
// NetAddress.cpp: implementation of the CNetAddress class.
//
//***************************************************************************

#include "pch.h"
#include "NetAddress.h"

//***************************************************************************
// @brief 기본 생성자 — 전부 0(family 0)인 무효 주소로 초기화합니다.
// @note 무효 주소는 Bind()/Connect()에 쓸 수 없습니다.
//       바인드용 와일드카드 주소는 Any()/AnyV6()/Wildcard()를 사용하십시오.
//***************************************************************************
CNetAddress::CNetAddress()
    : _valid(false)
{
    ::memset(&_addr, 0, sizeof(_addr));
}

//***************************************************************************
// @brief 주소를 전부 0(family 0)으로 지우고 무효 상태로 만듭니다.
//***************************************************************************
void CNetAddress::SetInvalid()
{
    ::memset(&_addr, 0, sizeof(_addr));
    _valid = false;
}

//***************************************************************************
// @brief SOCKADDR_IN 구조체 기반 생성자
// @param sockAddr 초기화할 소켓 주소 구조체
// @note sin_family가 AF_INET이 아니면 에러 로그를 남기고 무효 주소가 됩니다.
//***************************************************************************
CNetAddress::CNetAddress(const SOCKADDR_IN& sockAddr)
    : CNetAddress()
{
    if( sockAddr.sin_family != AF_INET )
    {
        LOG_ERROR(_T("[CNetAddress] SOCKADDR_IN family is not AF_INET (family=%d)"),
            static_cast<int>(sockAddr.sin_family));
        return;
    }

    _addr.Ipv4 = sockAddr;
    _valid = true;
}

//***************************************************************************
// @brief SOCKADDR_IN6 구조체 기반 생성자
// @param sockAddr 초기화할 소켓 주소 구조체
// @note sin6_family가 AF_INET6가 아니면 에러 로그를 남기고 무효 주소가 됩니다.
//***************************************************************************
CNetAddress::CNetAddress(const SOCKADDR_IN6& sockAddr)
    : CNetAddress()
{
    if( sockAddr.sin6_family != AF_INET6 )
    {
        LOG_ERROR(_T("[CNetAddress] SOCKADDR_IN6 family is not AF_INET6 (family=%d)"),
            static_cast<int>(sockAddr.sin6_family));
        return;
    }

    _addr.Ipv6 = sockAddr;
    _valid = true;
}

//***************************************************************************
// @brief IP 문자열 및 Port 기반 생성자
// @param ip IP 주소 문자열 (IPv4 점-십진 표기 또는 IPv6 표기). IPv4로 먼저 해석하고 실패하면 IPv6로 해석합니다.
// @param port 포트 번호 (Host Byte Order)
// @note IP 문자열 파싱에 실패하면 에러 로그를 남기고 무효 주소가 됩니다.
//***************************************************************************
CNetAddress::CNetAddress(const _tstring& ip, uint16 port)
    : CNetAddress()
{
    IN_ADDR addr4{};
    if( TryIp2Address(ip.c_str(), addr4) )
    {
        _addr.Ipv4.sin_family = AF_INET;
        _addr.Ipv4.sin_addr = addr4;
        _addr.Ipv4.sin_port = ::htons(port);
        _valid = true;
        return;
    }

    IN6_ADDR addr6{};
    if( TryIp2Address(ip.c_str(), addr6) )
    {
        _addr.Ipv6.sin6_family = AF_INET6;
        _addr.Ipv6.sin6_addr = addr6;
        _addr.Ipv6.sin6_port = ::htons(port);
        _valid = true;
        return;
    }

    LOG_ERROR(_T("[CNetAddress] invalid IP address string: '%s'"), ip.c_str());
}

//***************************************************************************
// @brief 임의의 sockaddr로부터 주소를 만듭니다.
// @param sockAddr 소켓 주소 포인터 (AF_INET 또는 AF_INET6)
// @param sockAddrLen sockAddr가 가리키는 버퍼의 유효 길이
// @return 포인터가 null이거나 지원하지 않는 family이거나 길이가 부족하면 IsValid()가 false인 주소
//***************************************************************************
CNetAddress CNetAddress::FromSockAddr(const SOCKADDR* sockAddr, int sockAddrLen)
{
    CNetAddress result;

    // family 필드를 읽기 전에 그 필드만큼의 길이가 있는지부터 확인한다.
    if( sockAddr == nullptr || sockAddrLen < static_cast<int>(sizeof(ADDRESS_FAMILY)) )
        return result;

    // family와 구조체 전체 길이를 검증한 뒤에만 복사한다.
    if( sockAddr->sa_family == AF_INET &&
        sockAddrLen >= static_cast<int>(sizeof(SOCKADDR_IN)) )
    {
        SOCKADDR_IN addr4{};
        ::memcpy(&addr4, sockAddr, sizeof(addr4));
        result._addr.Ipv4 = addr4;
        result._valid = true;
        return result;
    }

    if( sockAddr->sa_family == AF_INET6 &&
        sockAddrLen >= static_cast<int>(sizeof(SOCKADDR_IN6)) )
    {
        SOCKADDR_IN6 addr6{};
        ::memcpy(&addr6, sockAddr, sizeof(addr6));
        result._addr.Ipv6 = addr6;
        result._valid = true;
        return result;
    }

    return result;
}

//***************************************************************************
// @brief v4-mapped IPv6 주소(::ffff:a.b.c.d)를 순수 IPv4 주소로 풀어서 반환합니다.
// @return v4-mapped가 아니면 *this의 복사본
//***************************************************************************
CNetAddress CNetAddress::Unmap() const
{
    if( !IsIPv4Mapped() )
        return *this;

    CNetAddress result;
    result._addr.Ipv4.sin_family = AF_INET;
    ::memcpy(&result._addr.Ipv4.sin_addr,
        &_addr.Ipv6.sin6_addr.s6_addr[12], sizeof(IN_ADDR));
    result._addr.Ipv4.sin_port = _addr.Ipv6.sin6_port;
    result._valid = true;
    return result;
}

//***************************************************************************
// @brief GetSockAddr()가 가리키는 주소 구조체의 길이를 반환합니다.
// @return AF_INET이면 sizeof(SOCKADDR_IN), AF_INET6이면 sizeof(SOCKADDR_IN6), 무효 주소는 0
//***************************************************************************
int CNetAddress::GetSockAddrLen() const
{
    if( !_valid )
        return 0;

    switch( _addr.si_family )
    {
    case AF_INET:  return static_cast<int>(sizeof(SOCKADDR_IN));
    case AF_INET6: return static_cast<int>(sizeof(SOCKADDR_IN6));
    default:       return 0;
    }
}

//***************************************************************************
// @brief 저장된 소켓 주소로부터 IP 주소 문자열을 추출합니다.
// @return 변환된 IP 주소 문자열 (무효 주소이거나 변환에 실패하면 빈 문자열)
//***************************************************************************
_tstring CNetAddress::GetIpAddress() const
{
    if( !_valid )
        return _tstring();

    // INET6_ADDRSTRLEN은 숫자 주소 텍스트의 문자 수다. IPv6 최대 길이면 IPv4도 담을 수 있다.
    TCHAR buffer[INET6_ADDRSTRLEN] = {};
    bool converted = false;

    if( IsIPv4() )
        converted = CSocketUtils::AddrToIP(
            AF_INET, &_addr.Ipv4.sin_addr, buffer, _countof(buffer));
    else if( IsIPv6() )
        converted = CSocketUtils::AddrToIP(
            AF_INET6, &_addr.Ipv6.sin6_addr, buffer, _countof(buffer));

    // 변환 실패 시 초기화되지 않은 버퍼 대신 빈 문자열을 반환한다.
    return converted ? _tstring(buffer) : _tstring();
}

//***************************************************************************
// @brief 포트 번호를 반환합니다 (Host Byte Order).
// @return 포트 번호 (무효 주소는 0)
//***************************************************************************
uint16 CNetAddress::GetPort() const
{
    if( !_valid )
        return 0;

    if( IsIPv4() )
        return ::ntohs(_addr.Ipv4.sin_port);
    if( IsIPv6() )
        return ::ntohs(_addr.Ipv6.sin6_port);
    return 0;
}

//***************************************************************************
// @brief IPv6 scope id를 반환합니다 (링크 로컬 주소의 인터페이스 구분용).
// @return scope id (IPv6가 아니거나 무효 주소는 0)
//***************************************************************************
uint32 CNetAddress::GetScopeId() const
{
    return IsIPv6() ? _addr.Ipv6.sin6_scope_id : 0;
}

//***************************************************************************
// @brief 루프백 주소인지 반환합니다.
// @details IPv4는 127.0.0.0/8 전체를, IPv6는 ::1과 v4-mapped 127.x를 루프백으로 봅니다.
//***************************************************************************
bool CNetAddress::IsLoopback() const
{
    if( !IsValid() )
        return false;

    if( IsIPv4() )
        return (::ntohl(_addr.Ipv4.sin_addr.s_addr) & 0xFF000000u) == 0x7F000000u;

    if( IsIPv6() )
    {
        if( IN6_IS_ADDR_LOOPBACK(&_addr.Ipv6.sin6_addr) )
            return true;
        if( IsIPv4Mapped() )
            return Unmap().IsLoopback();
    }

    return false;
}

//***************************************************************************
// @brief 와일드카드 주소(0.0.0.0 또는 ::)인지 반환합니다.
//***************************************************************************
bool CNetAddress::IsAny() const
{
    if( !IsValid() )
        return false;

    if( IsIPv4() )
        return _addr.Ipv4.sin_addr.s_addr == INADDR_ANY;

    // 순수 IPv6 미지정 주소("::")만 IPv6 와일드카드로 취급한다.
    if( IsIPv6() )
        return IN6_IS_ADDR_UNSPECIFIED(&_addr.Ipv6.sin6_addr) != 0;

    return false;
}

//***************************************************************************
// @brief 주소 체계, IP, 포트가 모두 같은지 비교합니다 (v4-mapped는 IPv4로 풀어서 비교).
// @return 같으면 true. 무효 주소끼리는 true(반사성 유지), 무효와 유효 주소는 false
//***************************************************************************
bool CNetAddress::operator==(const CNetAddress& rhs) const
{
    // 무효 주소는 항상 전부 0으로 지워져 있으므로 둘 다 무효면 같은 값이다.
    // 이렇게 해야 a == a가 항상 성립한다(std::find, 컨테이너 등이 전제하는 동치 관계).
    if( !_valid || !rhs._valid )
        return _valid == rhs._valid;

    // 흔한 경우(양쪽 모두 순수 IPv4)는 복사 없이 바로 비교한다.
    if( _addr.si_family == AF_INET && rhs._addr.si_family == AF_INET )
    {
        return _addr.Ipv4.sin_addr.s_addr == rhs._addr.Ipv4.sin_addr.s_addr
            && _addr.Ipv4.sin_port == rhs._addr.Ipv4.sin_port;
    }

    const CNetAddress a = Unmap();
    const CNetAddress b = rhs.Unmap();

    if( a._addr.si_family != b._addr.si_family )
        return false;

    switch( a._addr.si_family )
    {
    case AF_INET:
        return a._addr.Ipv4.sin_addr.s_addr == b._addr.Ipv4.sin_addr.s_addr
            && a._addr.Ipv4.sin_port == b._addr.Ipv4.sin_port;

    case AF_INET6:
        return ::memcmp(&a._addr.Ipv6.sin6_addr,
            &b._addr.Ipv6.sin6_addr, sizeof(IN6_ADDR)) == 0
            && a._addr.Ipv6.sin6_port == b._addr.Ipv6.sin6_port
            && a._addr.Ipv6.sin6_scope_id == b._addr.Ipv6.sin6_scope_id;

    default:
        return false;
    }
}

//***************************************************************************
// @brief 와일드카드(0.0.0.0) 주소 팩토리
// @param port 포트 번호 (Host Byte Order, 0이면 임시 포트)
//***************************************************************************
CNetAddress CNetAddress::Any(uint16 port)
{
    return CNetAddress(_T("0.0.0.0"), port);
}

//***************************************************************************
// @brief 와일드카드(::) 주소 팩토리 — 듀얼스택 서버의 바인드 주소
// @param port 포트 번호 (Host Byte Order, 0이면 임시 포트)
//***************************************************************************
CNetAddress CNetAddress::AnyV6(uint16 port)
{
    return CNetAddress(_T("::"), port);
}

//***************************************************************************
// @brief family에 맞는 와일드카드 주소를 만듭니다.
// @param family AF_INET 또는 AF_INET6
// @param port 포트 번호 (Host Byte Order, 0이면 임시 포트)
// @return 지원하지 않는 family이면 무효 주소
//***************************************************************************
CNetAddress CNetAddress::Wildcard(int family, uint16 port)
{
    switch( family )
    {
    case AF_INET:  return Any(port);
    case AF_INET6: return AnyV6(port);
    default:       return CNetAddress();
    }
}

//***************************************************************************
// @brief 루프백(127.0.0.1) 주소 팩토리
// @param port 포트 번호 (Host Byte Order)
//***************************************************************************
CNetAddress CNetAddress::Loopback(uint16 port)
{
    return CNetAddress(_T("127.0.0.1"), port);
}

//***************************************************************************
// @brief 루프백(::1) 주소 팩토리
// @param port 포트 번호 (Host Byte Order)
//***************************************************************************
CNetAddress CNetAddress::LoopbackV6(uint16 port)
{
    return CNetAddress(_T("::1"), port);
}

//***************************************************************************
// @brief 문자열 IPv4 주소를 네트워크 바이트 오더 형태의 IN_ADDR 구조체로 변환합니다.
// @param ip 변환할 IP 주소 문자열 (문자열 포인터)
// @param outAddress [out] 변환된 IN_ADDR (실패 시 0.0.0.0)
// @return 변환 성공 여부
//***************************************************************************
bool CNetAddress::TryIp2Address(const TCHAR* ip, IN_ADDR& outAddress)
{
    outAddress = {};
    if( ip == nullptr )
        return false;

    if( !CSocketUtils::IPToAddr(AF_INET, ip, &outAddress) )
    {
        outAddress = {};    // 부분 기록 가능성에 대비해 0.0.0.0으로 정리
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 문자열 IPv6 주소를 IN6_ADDR 구조체로 변환합니다.
// @param ip 변환할 IP 주소 문자열 (scope id 미지원)
// @param outAddress [out] 변환된 IN6_ADDR (실패 시 ::)
// @return 변환 성공 여부
//***************************************************************************
bool CNetAddress::TryIp2Address(const TCHAR* ip, IN6_ADDR& outAddress)
{
    outAddress = {};
    if( ip == nullptr )
        return false;

    if( !CSocketUtils::IPToAddr(AF_INET6, ip, &outAddress) )
    {
        outAddress = {};
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
    if( !TryIp2Address(ip, address) )
    {
        LOG_ERROR(_T("[CNetAddress] invalid IPv4 address string: '%s' - returning 0.0.0.0"),
            (ip != nullptr) ? ip : _T("(null)"));
    }
    return address;
}