
//***************************************************************************
// HttpClientCore.h : interface for the CHttpClientCore class.
//
//***************************************************************************

#ifndef UC_HTTPCLIENTCORE_H
#define UC_HTTPCLIENTCORE_H

#include <BaseRedefineDataType.h>
#include <Network/HTTP/HttpResponseParser.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <utility>

// 요청 완료 통지. success=false면 파서가 Error 상태(프로토콜 위반 등)로 멈춘 것 —
// 이 경우 호출부는 커넥션을 재사용하지 말고 폐기해야 함.
using HttpRequestCompletionHandler = std::function<void(bool success, CHttpResponseParser& parser)>;

//***************************************************************************
// @enum EHttpClientState
// @brief CHttpClientCore의 요청/응답 진행 상태
//***************************************************************************
enum class EHttpClientState
{
	Idle,             // 요청 없음, 새 요청 받을 수 있음
	AwaitingResponse  // 요청 전송 완료, 응답 수신/파싱 대기 중
};

//***************************************************************************
// @class CHttpClientCore
// @brief 엔진(IOCP/RIO) 비의존 HTTP 요청/응답 오케스트레이션 로직
//
// @details
//      세션의 Send(const void*, uint16) 인터페이스(호출부가 sender 콜백으로 넘김)에만
//      의존한다. CIocpSession과 CRioSession은 OnRecv()/OnDataReceived() 수신
//      훅의 계약이 서로 달라(전자는 반환값으로 처리 바이트 수 통지, 후자는
//      파라미터 없이 호출되고 스스로 GetRecvBuffer()를 소비) 상속으로 공유할 수
//      없다. 그래서 상속이 아니라 "멤버로 보유(합성)"하는 방식으로 두 세션
//      클래스(CHttpSessionIocp/CHttpSessionRio) 양쪽에서 재사용한다 — 평문/TLS 분기와
//      연결 상태 통지까지 묶은 CHttpClientChannel(HttpClientChannel.h)이 이 클래스를 보유한다.
//
//      [책임 분리]
//      - 이 클래스: 요청 송신(65535바이트 단위 자동 분할) + 응답 파싱 상태 관리 +
//        완료 콜백
//      - 세션 클래스: 실제 소켓 I/O, 수신 훅에서 받은 바이트를 FeedRecv()로 전달
//      - 상위(CHttpConnPoolT 등): 커넥션 획득/반납, 재연결, keep-alive 판단
//
//      [파이프라이닝 미지원] 이 클래스는 "커넥션 1개 = HTTP 요청 1개씩 순차
//      처리"만 지원한다(대부분의 서버가 HTTP/1.1 파이프라이닝을 사실상 지원 안
//      하므로 의도적 선택). BeginRequest()는 이전 요청이 AwaitingResponse
//      상태면 false를 반환하니, 동시 요청은 상위 풀에서 커넥션을 여러 개 굴려서
//      병렬화해야 한다.
//
//      [스레드 안전성] BeginRequest()(요청을 보내는 스레드)와 FeedRecv()/
//      OnSessionDisconnected()(IOCP 워커 등 I/O 스레드)는 서로 다른 스레드에서
//      호출될 수 있으므로 상태(m_state/m_onComplete/m_parser)를 m_lock으로
//      보호한다. 완료 콜백은 항상 락 밖에서 호출한다 — 콜백이 BeginRequest()를
//      재진입하거나(풀의 DispatchToSession()) 세션을 폐기(Disconnect → 
//      OnSessionDisconnected())할 수 있기 때문이다.
//
//      [호출 계약] BeginRequest()가 true를 반환하면 onComplete는 정확히 한 번
//      호출되고(응답 완결, 파싱 에러, 또는 세션 끊김 중 하나), false를 반환하면
//      onComplete는 호출되지 않는다.
//***************************************************************************
class CHttpClientCore
{
public:
	using Clock = std::chrono::steady_clock;

	//***************************************************************************
	// @brief 완성된 요청 패킷을 전송하고 응답 대기 상태로 전이합니다.
	// @tparam SendFn 실제 바이트 전송 콜백 타입, 시그니처는
	//         bool(const void* data, uint16 size) — 세션의 Send()를 그대로
	//         람다로 감싸서 넘기면 됨: [this](const void* d, uint16 n) { return Send(d, n); }
	// @param data 완성된 요청 패킷 (예: CHttpRequestBuilderT::Build() 결과)
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool 송신 개시 성공 여부(false면 sender가 중간에 실패했거나 이미
	//         다른 요청이 진행 중)
	//***************************************************************************
	template<typename SendFn>
	bool BeginRequest(SendFn&& sender, const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(), Clock::time_point submitTime = Clock::now())
	{
		// 응답 대기 상태로 먼저 전이한 뒤 전송한다. 서버가 아주 빨리 응답해서 전송 직후
		// I/O 스레드가 FeedRecv()를 호출해도 그 응답을 받을 수 있어야 한다 — 전송 "후에"
		// 전이하면 그 응답은 "요청도 안 했는데 온 데이터"로 버려지고 요청이 영원히
		// 완료되지 않는다.
		{
			std::lock_guard<std::mutex> guard(m_lock);
			if( m_state.load(std::memory_order_relaxed) != EHttpClientState::Idle )
				return false;

			m_parser.Reset();
			// HEAD 요청의 응답은 헤더만 오므로 파서에 알린다 — 요청 첫 단어가 메서드다.
			m_parser.SetHeadResponse(len >= 5 && std::memcmp(data, "HEAD ", 5) == 0);
			m_onComplete = std::move(onComplete);
			m_timeout = timeout;           // 0 이하면 이 요청에는 타임아웃이 없다
			m_submitTime = submitTime;     // 타임아웃 기준 시각 (풀 대기 시간 포함해서 호출부가 넘긴다)
			m_lastActivityNs.store(0, std::memory_order_relaxed);
			m_sendInProgress = true;
			m_hasDeferredResult = false;
			m_state.store(EHttpClientState::AwaitingResponse, std::memory_order_release);
		}

		// 전송은 락 밖에서 한다 — sender(세션의 Send())가 실패하면 Disconnect()가
		// 동기적으로 OnSessionDisconnected()를 부를 수 있기 때문이다.
		bool sendOk = true;
		size_t offset = 0;
		while( offset < len )
		{
			size_t chunk = std::min<size_t>(len - offset, 65535);
			if( !sender(data + offset, static_cast<uint16>(chunk)) )
			{
				sendOk = false; // 부분 전송된 상태로 실패 — 호출부가 커넥션을 폐기해야 함
				break;
			}
			offset += chunk;
		}

		HttpRequestCompletionHandler cb;
		bool deferredSuccess = false;
		bool result = true;
		{
			std::lock_guard<std::mutex> guard(m_lock);
			m_sendInProgress = false;

			if( m_hasDeferredResult && m_onComplete )
			{
				// 전송 중에 응답이 완결됐다(서버의 조기 응답, 예: 큰 본문에 대한 413 후 연결 종료). 전송이
				// 끝났든 중간에 실패했든 이미 받은 완결된 응답이 있으므로 그것을 통지한다 — 전송 실패로
				// 덮어쓰면 서버가 보낸 응답이 사라진다.
				m_hasDeferredResult = false;
				deferredSuccess = m_deferredSuccess;
				cb = std::move(m_onComplete);
				m_onComplete = nullptr;
				m_state.store(EHttpClientState::Idle, std::memory_order_release);
			}
			else if( !sendOk )
			{
				m_hasDeferredResult = false;
				if( m_onComplete )
				{
					// 아직 통지되지 않았다 — 요청을 취소하고 호출부가 실패를 직접 처리하게 한다
					// (이 경우 onComplete는 호출되지 않는다).
					m_onComplete = nullptr;
					m_state.store(EHttpClientState::Idle, std::memory_order_release);
					result = false;
				}
				// 그렇지 않으면 전송 중 세션이 끊겨 OnSessionDisconnected()가 이미 실패를 통지했다.
				// 콜백이 정확히 한 번 호출됐으므로 true를 반환해 호출부의 이중 처리를 막는다.
			}
		}

		if( cb )
			cb(deferredSuccess, m_parser);

		return result;
	}

	// 세션의 수신 훅(OnRecv/OnDataReceived)에서 호출한다. 이번 호출로 응답이 완결(성공 또는 에러)되면 true.
	// 상세 설명은 HttpClientCore.cpp 참고.
	bool FeedRecv(const char* data, size_t len);

	//***************************************************************************
	// @brief 연결이 살아있다는 신호(응답 수신, 요청 송신 진행)를 기록해 무응답 타임아웃 기준을 갱신합니다.
	// @details 락 없이 호출할 수 있다. 큰 요청 본문을 느린 회선으로 올리는 동안 응답이 없다는 이유로
	//          요청이 끊기지 않도록 세션이 송신 완료 시점에도 호출한다.
	//***************************************************************************
	void NoteActivity() noexcept
	{
		m_lastActivityNs.store(Clock::now().time_since_epoch().count(), std::memory_order_relaxed);
	}

	// 진행 중인 요청이 무응답 타임아웃을 넘겼으면 실패(false)로 통지하고 중단한다. 이번 호출로 타임아웃 처리했으면 true.
	bool CheckTimeout(Clock::time_point now);

	EHttpClientState GetState() const noexcept { return m_state.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 직전 응답이 "Connection: close"를 명시했는지 반환합니다.
	// @return bool true면 keep-alive 재사용 금지
	//***************************************************************************
	bool IsConnectionCloseRequested() const noexcept { return m_parser.IsConnectionCloseRequested(); }

	// 세션의 OnDisconnected()가 호출하는 훅. 진행 중이던 요청이 있으면 마무리하고 완료 콜백을 부른다.
	// graceful: 서버가 정상 종료(FIN)했는지 여부.
	void OnSessionDisconnected(bool graceful = false);

private:
	std::mutex m_lock;                                   // 아래 상태 전체를 보호 (완료 콜백은 락 밖에서 호출)
	std::atomic<EHttpClientState> m_state{ EHttpClientState::Idle }; // 요청/응답 진행 상태 (쓰기는 m_lock 안에서만)
	CHttpResponseParser m_parser;                      // 응답 파서 (Idle 전이 시 재사용)
	HttpRequestCompletionHandler m_onComplete;          // 진행 중인 요청의 완료 콜백
	std::chrono::milliseconds m_timeout{ 0 };           // 진행 중인 요청의 무응답 타임아웃 (0 이하면 없음)
	Clock::time_point m_submitTime{};                   // 진행 중인 요청의 제출 시각 (타임아웃 기준)
	std::atomic<int64_t> m_lastActivityNs{ 0 };         // 마지막 활동(응답 수신/송신 진행) 시각, 0이면 아직 없음 (락 없이 갱신)
	bool m_sendInProgress = false;                      // BeginRequest()가 요청을 전송하는 중인지
	bool m_hasDeferredResult = false;                   // 전송 중에 응답이 완결돼 통지가 미뤄졌는지
	bool m_deferredSuccess = false;                     // 미뤄진 통지의 성공 여부
};

#endif // ndef UC_HTTPCLIENTCORE_H