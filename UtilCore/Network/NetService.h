
//***************************************************************************
// NetService.h : interface for the CNetService class.
//
//***************************************************************************

#ifndef UC_NETSERVICE_H
#define UC_NETSERVICE_H

#include <Network/NetAddress.h>

#include <functional>
#include <set>
#include <unordered_map>
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
	CSessionRef				GetSession(int32 index);

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
	CVector<CSessionRef>	_sessions;           // 현재 관리 중인 활성 세션 컨테이너 (GetSession(index)의 O(1) 인덱스 접근 계약 유지용)

	// [수정] AddSession()/ReleaseSession()이 매번 std::find()로 _sessions를
	// 선형탐색하던 것을 O(1) 평균으로 개선하기 위한 raw pointer→_sessions
	// 인덱스 맵. CSession*를 키로 쓰는 이유: shared_ptr 자체를 키로 쓰면
	// 해시/비교마다 컨트롤 블록 접근 비용이 붙고, 세션 소유권은 이미
	// _sessions(CVector<CSessionRef>)가 갖고 있어 여기선 식별자만 있으면
	// 충분하다 — _sessions에 없는 raw pointer가 이 맵에 남는 dangling 우려는
	// 없다(둘은 항상 AddSession/ReleaseSession 안에서 같은 락 하에 함께
	// 갱신됨). ReleaseSession()은 swap-and-pop으로 제거하므로 삭제 시 맨
	// 뒤 원소와 자리를 바꾼 세션의 인덱스도 함께 갱신해야 한다.
	std::unordered_map<CSession*, size_t> _sessionIndex;

	std::condition_variable _sessionsEmptyCv;
};

#endif // ndef UC_NETSERVICE_H