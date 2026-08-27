
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
// @note _sessions 목록을 순회하며 Disconnect를 호출합니다.
//***************************************************************************
void CNetService::Close()
{
	// 1. _sessions를 순회하며 각 세션에 Disconnect()를 게시한다. Disconnect()는
	//    비동기라(실제 소켓 종료/완료 통지 처리는 워커 스레드가 나중에 처리)
	//    이 루프 자체는 즉시 끝난다 — "게시"일 뿐 "완료 대기"가 아니다.
	std::unique_lock<std::mutex> guard(_lock);
	for( const CSessionRef& session : _sessions )
		session->Disconnect(L"NetService Close");

	// 2. 각 세션의 disconnect가 실제로 완료되면(OnDisconnected() 훅 이후)
	//    ReleaseSession()이 호출되어 _sessions에서 제거되고 _sessionsEmptyCv가
	//    notify된다 — 그 순간이 올 때까지, 즉 _sessions가 실제로 빌 때까지
	//    여기서 블로킹 대기한다.
	// 2-1. wait()가 대기하는 동안엔 guard(락)를 자동으로 풀어주므로, 그 사이에
	//      워커 스레드가 ReleaseSession()에서 같은 락을 잡고 세션을 제거할 수
	//      있다 — 여기서 락을 계속 쥐고 있으면 ReleaseSession()이 락을 못 잡아
	//      데드락에 빠진다는 점에서 unique_lock(lock_guard 아님)이 필수적이다.
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

	if( std::find(_sessions.begin(), _sessions.end(), session) == _sessions.end() )
	{
		_sessions.push_back(session);
	}
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

	auto it = std::find(_sessions.begin(), _sessions.end(), session);
	if( it != _sessions.end() )
	{
		_sessions.erase(it);
		_sessionsEmptyCv.notify_all();
	}
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