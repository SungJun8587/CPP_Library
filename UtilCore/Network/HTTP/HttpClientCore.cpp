
//***************************************************************************
// HttpClientCore.cpp : implementation of the CHttpClientCore class.
//
//***************************************************************************

#include "pch.h"
#include "HttpClientCore.h"

//***************************************************************************
// @brief 세션의 수신 훅(OnRecv/OnDataReceived)에서 호출합니다.
// @param data 수신 바이트 포인터
// @param len data의 길이
// @return bool 이번 호출로 응답이 완결(성공 또는 에러)되었는지 여부. true면
//         호출부(세션)가 커넥션 반납/폐기를 판단해야 함.
//***************************************************************************
bool CHttpClientCore::FeedRecv(const char* data, size_t len)
{
	HttpRequestCompletionHandler cb;
	bool success = false;
	{
		std::lock_guard<std::mutex> guard(m_lock);

		if( m_state.load(std::memory_order_relaxed) != EHttpClientState::AwaitingResponse )
			return false; // 요청도 안 했는데 온 데이터 — 프로토콜 위반, 상위에서 별도 처리

		if( m_hasDeferredResult )
			return false; // 이미 응답이 완결돼 통지를 기다리는 중 — 추가 데이터는 무시

		NoteActivity(); // 응답 데이터가 도착했다 — 무응답 타임아웃 기준 시각을 갱신한다

		HTTP::EParseState st = m_parser.Feed(data, len);
		if( st != HTTP::EParseState::Complete && st != HTTP::EParseState::Error )
			return false; // 아직 더 필요

		success = (st == HTTP::EParseState::Complete);

		// 응답이 완결됐는데 같은 수신에 바이트가 더 남아 있다 — 남은 바이트는 버려지므로 이 연결을 재사용하면
		// 뒤늦게 도착하는 나머지가 다음 응답과 섞인다. 응답은 정상 통지하되 연결은 폐기하게 표시한다.
		if( success && m_parser.GetLastFeedConsumed() < len )
			m_parser.RequestConnectionClose();

		if( m_sendInProgress )
		{
			// 요청 전송이 아직 끝나지 않았는데 응답이 완결됐다(서버의 조기 응답). 지금 통지하면
			// 풀이 이 세션을 다른 요청에 재사용해 아직 전송 중인 이 요청과 바이트가 섞이므로,
			// BeginRequest()가 전송을 마친 뒤 통지하도록 미룬다.
			m_hasDeferredResult = true;
			m_deferredSuccess = success;
			return true;
		}

		m_state.store(EHttpClientState::Idle, std::memory_order_release);
		cb = std::move(m_onComplete);
		m_onComplete = nullptr;
	}

	if( cb )
		cb(success, m_parser); // 락 밖에서 호출 — 콜백 안에서 다음 BeginRequest()를 걸 수도 있으므로 Idle 전이 이후 호출
	return true;
}

//***************************************************************************
// @brief 진행 중인 요청이 무응답 타임아웃을 넘겼으면 실패(false)로 통지하고 중단합니다.
// @param now 현재 시각
// @return 이번 호출로 요청을 타임아웃 처리했으면 true
// @details 판단과 중단을 m_lock 안에서 "현재 요청"에 대해 한 번에 하므로, 호출 스레드가 세션을 읽는
//          사이 세션이 다른 요청에 재사용됐더라도 엉뚱한 요청을 끊지 않는다. 기준 시각은
//          max(제출 시각, 마지막 활동 시각)이다. 완료 콜백은 락 밖에서 정확히 한 번 호출되며
//          파서에 타임아웃 표시(IsTimedOut())가 붙는다. 호출 계약(BeginRequest()가 true면
//          콜백은 정확히 한 번)을 지킨다.
//***************************************************************************
bool CHttpClientCore::CheckTimeout(Clock::time_point now)
{
	HttpRequestCompletionHandler cb;
	{
		std::lock_guard<std::mutex> guard(m_lock);

		if( m_timeout <= std::chrono::milliseconds::zero() )
			return false;
		if( m_state.load(std::memory_order_relaxed) != EHttpClientState::AwaitingResponse )
			return false;
		if( m_hasDeferredResult )
			return false; // 응답이 이미 완결돼 통지를 기다리는 중 — 정상 완료가 우선한다

		Clock::time_point base = m_submitTime;
		const int64_t lastNs = m_lastActivityNs.load(std::memory_order_relaxed);
		if( lastNs != 0 )
		{
			Clock::time_point last{ Clock::duration(lastNs) };
			if( last > base )
				base = last;
		}

		if( now - base < m_timeout )
			return false;

		m_parser.MarkTimedOut();
		m_state.store(EHttpClientState::Idle, std::memory_order_release);
		cb = std::move(m_onComplete);
		m_onComplete = nullptr;
	}

	if( cb )
		cb(false, m_parser);
	return true;
}

//***************************************************************************
// @brief 세션이 끊어졌을 때 세션(CHttpSessionIocp/Rio)의 OnDisconnected()가
//        호출해주는 훅입니다. 진행 중이던 요청이 있으면 실패로 마무리합니다.
// @details 요청 응답을 기다리는 중(AwaitingResponse)이 아니면 아무것도 하지
//          않는다 — Idle 상태에서 세션이 끊기는 건 정상적인 keep-alive
//          커넥션 종료(또는 idle 상태에서의 서버측 종료)라 실패로 통지할
//          대상 요청 자체가 없기 때문이다.
//
//          진행 중이던 요청이 있으면 m_onComplete를 로컬 변수로 옮겨 담고
//          멤버는 즉시 nullptr로 비운 뒤에 호출한다 — 콜백을 호출하기
//          "전에" 먼저 비우는 순서가 중요하다. 이 콜백(주로 CHttpConnPoolT::
//          DispatchToSession()이 넘긴 것)은 이 세션을 참조할 수 있어, 비우지 않은 채
//          호출하면 콜백 안의 재진입(예: 즉시 다음 요청을 이 세션에 거는 경우)에서 이전
//          m_onComplete와 상태가 꼬이고, 콜백이 붙잡은 참조가 세션 자신을 향하는 순환이
//          오래 남는다(풀은 세션을 원시 포인터로만 캡처해 순환을 만들지 않는다).
//
//          통지 결과: 이미 완결된 응답이 있었거나(전송 중 조기 응답), "연결 종료까지" 본문을
//          읽던 중 서버가 정상 종료한 경우는 success=true, 그 밖에는 false다. 어느 쪽이든
//          연결이 끊긴 세션이므로 받는 쪽(풀)은 재사용하지 말고 폐기해야 한다.
//***************************************************************************
void CHttpClientCore::OnSessionDisconnected(bool graceful)
{
	HttpRequestCompletionHandler cb;
	bool success = false;
	{
		std::lock_guard<std::mutex> guard(m_lock);

		if( m_state.load(std::memory_order_relaxed) != EHttpClientState::AwaitingResponse )
			return;

		if( m_hasDeferredResult )
		{
			// 전송 중에 이미 완결된 응답이 있었다 — 그 뒤에 연결이 끊겼어도 응답 자체는 유효하다.
			success = m_deferredSuccess;
		}
		else if( graceful && !m_sendInProgress && m_parser.IsReadingUntilClose() )
		{
			// 본문이 "연결 종료까지"인 응답을 읽던 중 서버가 연결을 정상 종료(FIN)했다면 그것이 본문의
			// 끝이다. 리셋/로컬 종료(graceful == false)로 끊긴 경우는 본문이 잘렸을 수 있으므로 실패로 둔다.
			m_parser.FinishAtConnectionClose();
			success = true;
		}

		m_state.store(EHttpClientState::Idle, std::memory_order_release);
		m_hasDeferredResult = false;
		cb = std::move(m_onComplete);
		m_onComplete = nullptr;               // 호출 전에 비워서 self-cycle을 즉시 끊음
	}

	if( cb )
		cb(success, m_parser);                // 실패(또는 연결 종료로 완결된 응답) 통지 (락 밖에서 호출)
}