
//***************************************************************************
// NetService.cpp : implementation of the CNetService class.
//
//***************************************************************************

#include "pch.h"
#include "NetService.h"

#include <vector>

//***************************************************************************
// @brief CNetService 생성자 구현
// @param type 서비스 타입
// @param address 네트워크 주소
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
//***************************************************************************
CNetService::CNetService(NetServiceType type, CNetAddress address, SessionFactory factory, int32 maxSessionCount)
	: _type(type), _address(address), _sessionFactory(factory), _maxSessionCount(maxSessionCount)
{
}

//***************************************************************************
// @brief CNetService 소멸자 구현
//***************************************************************************
CNetService::~CNetService()
{
	Close();
}

//***************************************************************************
// @brief 서비스에 등록된 모든 세션을 종료합니다.
// @note
// [수정] 기존에는 _lock을 쥔 채로 각 session->Disconnect()를 호출했다. 이는
// Disconnect()가 항상 순수 비동기(게시만 하고 즉시 반환)라는 가정 위에서만
// 안전했는데, CIocpSession::Disconnect(Iocp::CloseReason)의 "아직 연결 완료
// 전(Accept/ConnectEx 진행 중)" 경로는 그 자리에서 동기적으로
// OnDisconnected()→CSession::OnDisconnected()→(DisconnectHandler)→
// CNetService::ReleaseSession()까지 호출하며 같은 _lock(non-recursive
// std::mutex)을 다시 잡으려 한다 — 같은 스레드의 재진입이라 그 즉시
// 데드락이었다.
//
// CIocpSessionManager::Broadcast()/BeginCloseAllSessions()가 이미 쓰고 있는
// 패턴과 동일하게, 락 안에서는 세션 목록의 스냅샷만 수집하고 실제
// Disconnect() 호출은 락 밖에서 수행하도록 바꿔 이 재진입 데드락을 근본적으로
// 제거한다(Disconnect()가 동기/비동기 어느 쪽이든 안전).
//***************************************************************************
void CNetService::Close()
{
	// 1. 락 안에서는 세션 목록 스냅샷만 수집한다(shared_ptr 복사이므로 이
	//    시점 이후 다른 스레드가 원본 세션을 정리해도 여기 보관된 참조는
	//    안전하게 유효하다).
	std::vector<CSessionRef> sessionsToClose;
	{
		std::lock_guard<std::mutex> guard(_lock);
		sessionsToClose.reserve(_sessions.size());
		for( const CSessionRef& session : _sessions )
			sessionsToClose.push_back(session);
	}

	// 2. 락 밖에서 Disconnect()를 호출한다. Disconnect()가 동기적으로
	//    ReleaseSession()(같은 _lock)을 재진입하더라도, 이 스레드는 더 이상
	//    _lock을 쥐고 있지 않으므로 안전하다.
	for( const CSessionRef& session : sessionsToClose )
		session->Disconnect(L"NetService Close");

	// 3. 각 세션의 disconnect가 실제로 완료되면(OnDisconnected() 훅 이후)
	//    ReleaseSession()이 호출되어 _sessions에서 제거되고 _sessionsEmptyCv가
	//    notify된다 — 그 순간이 올 때까지, 즉 _sessions가 실제로 빌 때까지
	//    여기서 블로킹 대기한다. wait()는 대기 중 guard(락)를 자동으로
	//    풀어주므로 그 사이 ReleaseSession()이 락을 잡을 수 있다.
	std::unique_lock<std::mutex> guard(_lock);
	_sessionsEmptyCv.wait(guard, [this] { return _sessions.empty(); });
}

//***************************************************************************
// @brief 현재 활성화된 세션 수 조회
// @return int32 관리 세션 수
//***************************************************************************
int32 CNetService::GetCurrentSessionCount()
{
	std::lock_guard<std::mutex> guard(_lock);
	return static_cast<int32>(_sessions.size());
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
//***************************************************************************
void CNetService::AddSession(CSessionRef session)
{
	if( session == nullptr )
		return;

	std::lock_guard<std::mutex> guard(_lock);

	// [수정] std::find() O(n) 선형탐색 대신 _sessionIndex(unordered_map)로
	// O(1) 평균 중복 체크.
	if( _sessionIndex.find(session.get()) != _sessionIndex.end() )
		return;

	_sessionIndex.emplace(session.get(), _sessions.size());
	_sessions.push_back(session);
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

	// [수정] std::find() O(n) 선형탐색 + erase()의 O(n) 원소 시프트 대신,
	// _sessionIndex로 대상 위치를 O(1) 평균으로 찾고 맨 뒤 원소와 자리를
	// 바꾼 뒤(swap-and-pop) 맨 뒤를 제거 — 순서 보장이 필요 없는 컨테이너라
	// 안전하다. 자리를 옮긴 세션의 인덱스도 함께 갱신해야 한다.
	auto it = _sessionIndex.find(session.get());
	if( it == _sessionIndex.end() )
		return;

	size_t removeIdx = it->second;
	size_t lastIdx = _sessions.size() - 1;

	if( removeIdx != lastIdx )
	{
		_sessions[removeIdx] = _sessions[lastIdx];
		_sessionIndex[_sessions[removeIdx].get()] = removeIdx;
	}

	_sessions.erase(_sessions.begin() + lastIdx); // 맨 뒤 원소 제거 — 시프트 없음
	_sessionIndex.erase(it);

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