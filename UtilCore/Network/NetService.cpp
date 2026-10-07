
//***************************************************************************
// NetService.cpp : implementation of the CNetService class.
//
//***************************************************************************

#include "pch.h"
#include "NetService.h"

//***************************************************************************
// @brief CNetService 생성자 구현
// @param type 서비스 타입
// @param address 네트워크 주소
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
//***************************************************************************
CNetService::CNetService(NetServiceType type, const CNetAddress& address, SessionFactory factory, int32 maxSessionCount)
	: _type(type), _address(address), _sessionFactory(std::move(factory)), _maxSessionCount(maxSessionCount)
{
}

//***************************************************************************
// @brief CNetService 소멸자 구현
// @note 소멸자가 실행되는 시점에는 shared_ptr strong count가 이미 0이라
//       CreateSession()의 DisconnectHandler 안 weak_ptr::lock()이 항상 실패하고
//       ReleaseSession()이 호출되지 않는다. 따라서 여기서 _sessions가 빌 때까지
//       대기하면 영원히 끝나지 않을 수 있다 — 대기 없이 Disconnect 요청만 한다.
//       또한 소멸자 안의 가상 호출은 CNetService::Close()로 고정되어 파생 클래스의
//       정리 로직이 실행되지 않으므로 Close()를 호출하지 않는다.
//       정상 종료는 소유자가 Close()를 명시적으로 호출해야 한다.
//***************************************************************************
CNetService::~CNetService()
{
	_closing.store(true);	// 소멸 이후에는 되돌리지 않는다
	CloseSessions(false);
}

//***************************************************************************
// @brief 서비스에 등록된 모든 세션을 종료합니다.
// @note
// 락 안에서는 세션 목록의 스냅샷만 수집하고, 실제 Disconnect() 호출은 락 밖에서 한다.
// CIocpSession::Disconnect(Iocp::CloseReason)의 "아직 연결 완료 전(Accept/ConnectEx 진행 중)"
// 경로는 그 자리에서 동기적으로 OnDisconnected()→CSession::OnDisconnected()→(DisconnectHandler)→
// CNetService::ReleaseSession()까지 호출하며 같은 _lock(non-recursive std::mutex)을 다시 잡는다.
// 락을 쥔 채 Disconnect()를 호출하면 같은 스레드의 재진입이라 즉시 데드락이 되므로, Disconnect()가
// 동기/비동기 어느 쪽이든 안전하도록 이 패턴을 쓴다(CIocpSessionManager의 Broadcast()/
// BeginCloseAllSessions()도 동일).
//
// _closing 플래그: 종료 중에는 AddSession()이 신규 세션을 거부한다. 스냅샷 이후 등록된 세션이
// Disconnect 대상에서 빠져 _sessions가 영원히 비지 않는 일을 막는다. 대기에는 타임아웃을 두어
// 한 세션의 disconnect가 끝나지 않아도(워커 정체 등) 종료 절차 전체가 멈추지 않게 하고, 소멸자는
// 대기 없이 같은 로직(waitForDrain=false)만 사용한다.
//***************************************************************************
void CNetService::Close()
{
	CloseSessions(true);
}

//***************************************************************************
// @brief 세션 종료 공통 구현.
// @param waitForDrain true면 _sessions가 빌 때까지 최대 kCloseWaitTimeout 대기
//***************************************************************************
void CNetService::CloseSessions(bool waitForDrain)
{
	static constexpr std::chrono::seconds kCloseWaitTimeout{ 10 };

	// 0. 이후 AddSession()은 신규 세션을 거부한다.
	_closing.store(true);

	// 1. 락 안에서는 세션 목록 스냅샷만 수집한다(shared_ptr 복사이므로 이
	//    시점 이후 다른 스레드가 원본 세션을 정리해도 여기 보관된 참조는
	//    안전하게 유효하다).
	std::vector<CSessionRef> sessionsToClose = GetSessionsSnapshot();

	// 2. 락 밖에서 Disconnect()를 호출한다. Disconnect()가 동기적으로
	//    ReleaseSession()(같은 _lock)을 재진입하더라도, 이 스레드는 더 이상
	//    _lock을 쥐고 있지 않으므로 안전하다.
	for( const CSessionRef& session : sessionsToClose )
		session->Disconnect(_T("NetService Close"));

	sessionsToClose.clear();

	if( waitForDrain )
	{
		// 3. 각 세션의 disconnect가 실제로 완료되면(OnDisconnected() 훅 이후)
		//    ReleaseSession()이 호출되어 _sessions에서 제거되고 _sessionsEmptyCv가
		//    notify된다. wait_for()는 대기 중 guard(락)를 자동으로 풀어주므로 그
		//    사이 ReleaseSession()이 락을 잡을 수 있다.
		std::unique_lock<std::mutex> guard(_lock);
		if( !_sessionsEmptyCv.wait_for(guard, kCloseWaitTimeout, [this] { return _sessions.empty(); }) )
		{
			LOG_ERROR(_T("[CNetService] Close timeout: %d session(s) remaining"), static_cast<int32>(_sessions.size()));
		}

		// 4. 종료 절차가 끝났으므로 재시작(Start() 재호출)이 가능하도록 되돌린다.
		_closing.store(false);
	}
}

//***************************************************************************
// @brief _sessions가 빌 때까지 최대 timeout 동안 대기합니다.
// @return 제한 시간 안에 비었으면 true
// @details wait_for()는 대기 중 락을 풀어 주므로 그 사이 ReleaseSession()이 락을 잡고
//          _sessionsEmptyCv를 notify할 수 있다.
//***************************************************************************
bool CNetService::WaitForSessionsEmpty(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> guard(_lock);
	return _sessionsEmptyCv.wait_for(guard, timeout, [this] { return _sessions.empty(); });
}

//***************************************************************************
// @brief 현재 활성화된 세션 수 조회
// @return int32 관리 세션 수
//***************************************************************************
int32 CNetService::GetCurrentSessionCount() const
{
	std::lock_guard<std::mutex> guard(_lock);
	return static_cast<int32>(_sessions.size());
}

//***************************************************************************
// @brief 현재 세션 목록의 스냅샷을 반환합니다.
// @return 세션 shared_ptr 복사본 목록 (락은 1회만 획득)
//***************************************************************************
std::vector<CSessionRef> CNetService::GetSessionsSnapshot() const
{
	std::lock_guard<std::mutex> guard(_lock);

	std::vector<CSessionRef> snapshot;
	snapshot.reserve(_sessions.size());
	for( const CSessionRef& session : _sessions )
		snapshot.push_back(session);

	return snapshot;
}

//***************************************************************************
// @brief SessionFactory를 호출하여 신규 세션을 생성하고 이벤트 핸들러를 바인딩합니다.
// @return CSessionRef 생성된 세션 객체 참조
// @note 생성된 세션에는 CNetService의 weak_ptr을 이용한 ReleaseSession 콜백이 자동 바인딩됩니다.
//***************************************************************************
CSessionRef CNetService::CreateSession()
{
	CSessionRef session = _sessionFactory();
	if( session )
	{
		std::weak_ptr<CNetService> serviceWeak = shared_from_this();

		session->SetDisconnectHandler([serviceWeak](CSessionRef disconnectedSession)
			{
				if( CNetServiceRef service = serviceWeak.lock() )
				{
					service->ReleaseSession(disconnectedSession);
				}
			});
	}

	return session;
}

//***************************************************************************
// @brief 세션 관리 추가
// @param session 추가할 세션 객체
// @return 등록(또는 이미 등록됨)되면 true, 종료 중이라 거부되면 false
// @note 종료 중 거부된 세션은 여기서 Disconnect를 요청한다(락 밖에서 호출 —
//       Disconnect()가 동기적으로 ReleaseSession()을 재진입해도 안전하며,
//       등록된 적 없는 세션이라 ReleaseSession()은 무시한다).
//***************************************************************************
bool CNetService::AddSession(CSessionRef session)
{
	if( session == nullptr )
		return false;

	{
		std::lock_guard<std::mutex> guard(_lock);

		// _closing 확인과 등록이 같은 락 안이어야, CloseSessions()의 스냅샷 이후에
		// 등록되는 세션이 생기지 않는다(스냅샷도 같은 락으로 수집).
		if( !_closing.load() )
		{
			// _sessionIndex(unordered_map)로 O(1) 평균 중복 체크.
			if( _sessionIndex.find(session.get()) != _sessionIndex.end() )
				return true;

			_sessionIndex.emplace(session.get(), _sessions.size());
			_sessions.push_back(session);
			return true;
		}
	}

	session->Disconnect(_T("NetService Closing"));
	return false;
}

//***************************************************************************
// @brief 세션 관리 제거
// @param session 제거할 세션 객체
//***************************************************************************
void CNetService::ReleaseSession(CSessionRef session)
{
	if( session == nullptr )
		return;

	std::lock_guard<std::mutex> guard(_lock);

	// _sessionIndex로 대상 위치를 O(1) 평균으로 찾고, 맨 뒤 원소와 자리를 바꾼 뒤(swap-and-pop)
	// 맨 뒤를 제거한다 — 순서 보장이 필요 없는 컨테이너라 안전하다. 자리를 옮긴 세션의 인덱스도
	// 함께 갱신해야 한다.
	auto it = _sessionIndex.find(session.get());
	if( it == _sessionIndex.end() )
		return;

	size_t removeIdx = it->second;
	size_t lastIdx = _sessions.size() - 1;

	if( removeIdx != lastIdx )
	{
		_sessions[removeIdx] = std::move(_sessions[lastIdx]);
		_sessionIndex[_sessions[removeIdx].get()] = removeIdx;
	}

	_sessions.erase(_sessions.begin() + lastIdx); // 맨 뒤 원소 제거 — 시프트 없음
	_sessionIndex.erase(it);

	// 대기자는 "_sessions가 빔"만 기다리므로 비었을 때만 깨우면 된다.
	if( _sessions.empty() )
		_sessionsEmptyCv.notify_all();
}

//***************************************************************************
// @brief 지정된 인덱스에 해당하는 세션 객체를 반환합니다.
// @param index 조회할 세션의 인덱스 (0부터 _sessions.size() - 1까지)
// @return CSessionRef 인덱스에 해당하는 세션 객체 스마트 포인터 (범위를 벗어날 경우 nullptr)
//***************************************************************************
CSessionRef CNetService::GetSession(int32 index)
{
	std::lock_guard<std::mutex> lock(_lock);
	// index 유효성 검사 후 반환
	if( index < 0 || index >= (int32)_sessions.size() )
		return nullptr;

	return _sessions[index];
}