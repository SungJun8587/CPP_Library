
//***************************************************************************
// NetAddress.h : interface for the CNetAddress class.
//
//***************************************************************************

#ifndef UC_NETADDRESS_H
#define UC_NETADDRESS_H

#include <winsock2.h>
#include <BaseRedefineDataType.h>

//***************************************************************************
// @class CNetAddress
// @brief IPv4 소켓 주소(SOCKADDR_IN) 패킹 및 IP/Port 문자열 변환 래퍼 클래스
//***************************************************************************
class CNetAddress
{
public:
	CNetAddress() = default;
	CNetAddress(const SOCKADDR_IN& sockAddr);	// 암시적 변환을 쓰는 호출부가 있을 수 있어 explicit은 붙이지 않음
	CNetAddress(const _tstring& ip, uint16 port);

	//***************************************************************************
	// @brief IP 문자열 파싱에 성공해 사용 가능한 주소인지 반환합니다.
	// @details (ip, port) 생성자에서 IP 문자열이 잘못되면 false가 됩니다. 이 경우
	//          주소는 0.0.0.0으로 남으므로, CSocketUtils::Bind()/Connect()는
	//          무효 주소를 거부해 "서버가 의도치 않게 모든 인터페이스에 열리는"
	//          상황을 막습니다.
	//***************************************************************************
	bool IsValid() const { return _valid; }

	//***************************************************************************
	// @brief 내부 SOCKADDR_IN 구조체의 참조를 반환합니다.
	//***************************************************************************
	SOCKADDR_IN& GetSockAddr() { return _sockAddr; }

	//***************************************************************************
	// @brief 내부 SOCKADDR_IN 구조체의 const 참조를 반환합니다.
	// @details const CNetAddress&로 받은 파라미터(예: 함수 인자로 전달받은 주소)에서도
	//          조회만 할 수 있도록 하는 const 오버로드. non-const 버전만 있으면
	//          const 참조 문맥에서 이 함수를 호출할 수 없어(호출부가 값 복사로
	//          우회해야 하는 불편이 있었음), 단순 조회 목적에는 이 오버로드가
	//          더 자연스럽다.
	//***************************************************************************
	const SOCKADDR_IN& GetSockAddr() const { return _sockAddr; }

	_tstring		GetIpAddress() const;

	//***************************************************************************
	// @brief 포트 번호를 반환합니다 (Host Byte Order).
	//***************************************************************************
	uint16			GetPort() const { return ::ntohs(_sockAddr.sin_port); }

public:
	//***************************************************************************
	// @brief IPv4 문자열을 IN_ADDR로 변환합니다. 실패 시 false(outAddress는 0.0.0.0).
	//***************************************************************************
	static bool		TryIp2Address(const TCHAR* ip, IN_ADDR& outAddress);

	// 실패 시 0.0.0.0을 반환하고 에러 로그를 남깁니다(기존 호환용). 성공 여부가
	// 필요하면 TryIp2Address()를 사용할 것.
	static IN_ADDR	Ip2Address(const TCHAR* ip);

private:
	SOCKADDR_IN		_sockAddr = {}; // 소켓 주소(IP, Port, Family) 정보 구조체
	bool			_valid = true;  // IP 문자열 파싱 성공 여부
};

#endif // ndef UC_NETADDRESS_H