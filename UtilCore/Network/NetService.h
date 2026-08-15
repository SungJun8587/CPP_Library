
//***************************************************************************
// NetService.h : interface for the CNetService class.
//
//***************************************************************************

#ifndef __NET_SERVICE_H__
#define __NET_SERVICE_H__

#ifndef __NETADDRESS_H__
#include <Network/NetAddress.h>
#endif

#include <functional>
#include <set>
#include <mutex>

//***************************************************************************
// @enum NetServiceType
// @brief 네트워크 서비스의 역할(서버 / 클라이언트)을 구별하는 열거형입니다.
//***************************************************************************
enum class NetServiceType : uint8
{
	Server,
	Client
};

//***************************************************************************
// @class CNetService
// @brief 서버 및 클라이언트 서비스를 관리하는 최상위 추상 기반 클래스.
// @details
// 역할:
//     1. SessionFactory를 통한 세션 생성 관리
//     2. 생성된 세션 객체 집합(_sessions)의 수명 주기 및 스레드 세이프 관리
//     3. 세션과 100% 디커플링을 유지하기 위해 DisconnectHandler(람다) 바인딩
//***************************************************************************
class CNetService : public std::enable_shared_from_this<CNetService>
{
public:
	CNetService(NetServiceType type, CNetAddress address, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~CNetService();

	virtual bool			Start() = 0;
	virtual void			Close();

	//***************************************************************************
	// @brief 서비스의 타입을 반환합니다.
	// @return NetServiceType 서비스 타입 (Server 또는 Client)
	//***************************************************************************
	NetServiceType			GetServiceType() const { return _type; }

	//***************************************************************************
	// @brief 바인딩된 네트워크 주소를 반환합니다.
	// @return CNetAddress 네트워크 주소 객체
	//***************************************************************************
	CNetAddress				GetNetAddress() const { return _address; }

	//***************************************************************************
	// @brief 최대 수용 가능 세션 수를 반환합니다.
	// @return int32 최대 세션 수
	//***************************************************************************
	int32					GetMaxSessionCount() const { return _maxSessionCount; }

	int32					GetCurrentSessionCount();

	// 세션 생성 및 컨테이너 관리 API
	CSessionRef				CreateSession();
	void					AddSession(CSessionRef session);
	void					ReleaseSession(CSessionRef session);

	//***************************************************************************
	// @brief 서비스가 정상적으로 시작할 수 있는 상태인지 검증합니다.
	// @return bool SessionFactory 보유 여부
	//***************************************************************************
	bool					CanStart() const { return _sessionFactory != nullptr; }

protected:
	NetServiceType			_type;               // 서비스 동작 유형 (Server / Client)
	CNetAddress				_address;            // 서비스 대상 바인딩/접속 주소
	SessionFactory			_sessionFactory;     // 세션 생성 콜백 함수

	std::mutex				_lock;               // 세션 컨테이너 동기화용 뮤텍스
	int32					_maxSessionCount = 0;// 최대 허용 세션 수
	CSet<CSessionRef>		_sessions;           // 현재 관리 중인 활성 세션 컨테이너
};

#endif // ndef __NET_SERVICE_H__