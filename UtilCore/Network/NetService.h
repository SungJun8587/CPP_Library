
//***************************************************************************
// NetService.h : interface for the CNetService class.
//
//***************************************************************************

#ifndef UC_NETSERVICE_H
#define UC_NETSERVICE_H

#include <Network/NetAddress.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

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
	CNetService(NetServiceType type, const CNetAddress& address, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~CNetService();

	virtual bool			Start() = 0;

	//***************************************************************************
	// @brief 등록된 모든 세션을 종료하고 완료될 때까지(타임아웃 있음) 대기합니다.
	// @note 반드시 소유자가 shared_ptr을 쥔 상태에서 명시적으로 호출해야 합니다.
	//       소멸자는 대기 없이 세션 Disconnect만 요청합니다(소멸 중에는
	//       DisconnectHandler의 weak_ptr::lock()이 항상 실패하므로 대기가 불가능).
	//       IOCP/RIO 워커 스레드 안에서 호출하면 완료 처리를 못 해 타임아웃까지
	//       대기하게 되므로 호출하지 말 것.
	//***************************************************************************
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

	int32					GetCurrentSessionCount() const;

	// 세션 생성 및 컨테이너 관리 API
	CSessionRef				CreateSession();

	//***************************************************************************
	// @brief 세션을 관리 목록에 추가합니다.
	// @return 추가(또는 이미 등록됨)되면 true. 서비스가 종료 중이면 세션을 등록하지
	//         않고 Disconnect를 요청한 뒤 false를 반환합니다.
	//***************************************************************************
	bool					AddSession(CSessionRef session);
	void					ReleaseSession(CSessionRef session);
	CSessionRef				GetSession(int32 index);

	//***************************************************************************
	// @brief 현재 세션 목록의 스냅샷을 반환합니다.
	// @details GetCurrentSessionCount() + GetSession(i) 순회는 원자적이지 않고
	//          swap-and-pop 때문에 인덱스가 안정적이지 않아 누락/중복이 생길 수
	//          있다. 순회가 필요하면 이 함수를 사용할 것(락은 1회만 잡음).
	//***************************************************************************
	std::vector<CSessionRef>	GetSessionsSnapshot() const;

	//***************************************************************************
	// @brief 서비스가 정상적으로 시작할 수 있는 상태인지 검증합니다.
	// @return bool SessionFactory 보유 여부
	//***************************************************************************
	bool					CanStart() const { return _sessionFactory != nullptr; }

protected:
	//***************************************************************************
	// @brief 세션 종료 공통 구현. 락 밖에서 Disconnect()를 호출합니다.
	// @param waitForDrain true면 _sessions가 빌 때까지(타임아웃 있음) 대기합니다.
	//***************************************************************************
	void					CloseSessions(bool waitForDrain);

	//***************************************************************************
	// @brief _sessions가 빌 때까지(해제 통지가 모두 끝날 때까지) 최대 timeout 동안 대기합니다.
	// @return 제한 시간 안에 비었으면 true, 시간 초과면 false
	// @details 파생 서비스가 워커 스레드를 멈추기 전에 해제 통지를 기다리는 용도입니다 —
	//          Close()와 달리 시간 초과 시 로그를 남기거나 종료 상태를 바꾸지 않습니다.
	//***************************************************************************
	bool					WaitForSessionsEmpty(std::chrono::milliseconds timeout);

	NetServiceType			_type;               // 서비스 동작 유형 (Server / Client)
	CNetAddress				_address;            // 서비스 대상 바인딩/접속 주소
	SessionFactory			_sessionFactory;     // 세션 생성 콜백 함수

	mutable std::mutex		_lock;               // 세션 컨테이너 동기화용 뮤텍스
	std::atomic<bool>		_closing{ false };   // 종료 진행 중 — true인 동안 AddSession()은 신규 세션을 거부
	int32					_maxSessionCount = 0;// 최대 허용 세션 수
	CVector<CSessionRef>	_sessions;           // 현재 관리 중인 활성 세션 컨테이너 (GetSession(index)의 O(1) 인덱스 접근 계약 유지용)

	// raw pointer→_sessions 인덱스 맵. AddSession()/ReleaseSession()이 _sessions를 선형탐색하지
	// 않고 O(1) 평균으로 대상 위치를 찾게 한다. CSession*를 키로 쓰는 이유: shared_ptr 자체를
	// 키로 쓰면 해시/비교마다 컨트롤 블록 접근 비용이 붙고, 세션 소유권은 이미
	// _sessions(CVector<CSessionRef>)가 갖고 있어 여기선 식별자만 있으면 충분하다.
	// _sessions에 없는 raw pointer가 이 맵에 남는 dangling 우려는 없다(둘은 항상
	// AddSession/ReleaseSession 안에서 같은 락 하에 함께 갱신됨). ReleaseSession()은
	// swap-and-pop으로 제거하므로 삭제 시 맨 뒤 원소와 자리를 바꾼 세션의 인덱스도 함께 갱신해야 한다.
	std::unordered_map<CSession*, size_t> _sessionIndex;

	std::condition_variable _sessionsEmptyCv;
};

#endif // ndef UC_NETSERVICE_H