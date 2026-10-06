
//***************************************************************************
// HttpClientChannel.h : interface for the CHttpClientChannel class.
//
//***************************************************************************

#ifndef UC_HTTPCLIENTCHANNEL_H
#define UC_HTTPCLIENTCHANNEL_H

#include <BaseRedefineDataType.h>
#include <Network/HTTP/HttpConnPoolCommon.h>
#include <Network/HTTP/HttpClientCore.h>
#include <Network/HTTP/TlsFilter.h>

#include <chrono>
#include <functional>
#include <string>
#include <utility>

namespace HTTP
{
	// HTTP 클라이언트 연결의 keep-alive 확인 주기: 유휴 30초 후 첫 확인, 이후 10초 간격.
	// 풀의 유휴 연결이 FIN/RST 없이 조용히 끊긴 것을 일찍 감지하기 위한 값이며, IOCP/RIO 세션이 공유한다.
	inline constexpr uint32 kClientKeepAliveIdleMs = 30000;
	inline constexpr uint32 kClientKeepAliveIntervalMs = 10000;
}

//***************************************************************************
// @class CHttpClientChannel
// @brief 연결 하나 위에서 HTTP 요청/응답을 주고받는 엔진 비의존 채널 (평문/TLS 겸용)
//
// @details
//      CHttpSessionIocp/CHttpSessionRio가 똑같이 필요로 하는 로직을 한 곳에 모은 클래스다 —
//      CHttpClientCore(요청/응답 오케스트레이션) + CTlsFilter(암복호화) + 연결 상태 통지 +
//      평문/TLS 분기. 세션 클래스는 엔진별 수신/종료 훅에서 이 클래스의 OnXxx()를 호출하고
//      엔진별 동작(바이트 전송, 연결 끊기, 자기 자신의 shared_ptr)만 Bind()로 알려주면 된다.
//
//      [평문/TLS 선택] 세션 생성 직후 EnableTls()를 호출하면 그 순간부터 HTTPS, 안 하면
//      평문 HTTP다. 두 모드의 차이는 세 지점뿐이다.
//        - OnConnected(): 평문은 바로 "연결됨" 통지, TLS는 핸드셰이크부터 시작하고
//          핸드셰이크 완료 콜백(CTlsFilter가 호출)에서야 통지한다.
//        - OnRecv(): 평문은 바이트를 CHttpClientCore로 직접, TLS는 CTlsFilter로 복호화한 뒤 전달한다.
//        - SendRequest(): 평문은 rawSend로 직접, TLS는 CTlsFilter::SendPlaintext()로 암호화해서 보낸다.
//      CTlsFilter는 평문 모드에서도 멤버로 갖고 있지만 EnableTls() 전에는 SSL 객체를 만들지 않아
//      비용이 거의 없다.
//
//      [수명] 채널은 세션의 멤버이므로 세션보다 오래 살지 않는다. 자기 자신을 가리키는
//      CSessionRef를 멤버로 보관하면 세션이 영원히 해제되지 않는 순환 참조가 되므로, Bind()의
//      self는 필요할 때마다 shared_ptr을 만들어 돌려주는 콜백이다.
//
//      [스레드 안전성] 요청 송신은 임의의 스레드, 수신/종료 훅은 I/O 스레드에서 호출될 수 있으며
//      동기화는 CHttpClientCore(요청 상태)와 CTlsFilter(SSL 객체)가 각자 책임진다. Bind()와
//      EnableTls()는 연결 시도 전에 한 번만 호출한다.
//***************************************************************************
class CHttpClientChannel
{
public:
	using SelfFn = std::function<CSessionRef()>;
	using DisconnectFn = std::function<void()>;

	//***************************************************************************
	// @brief 엔진별 동작을 연결합니다 (세션 생성자에서 한 번 호출).
	// @param self 세션 자신의 CSessionRef를 만들어 돌려주는 콜백 ([this] { return shared_from_this(); })
	// @param rawSend 세션의 Send(const void*, uint16) — 평문 송신과 TLS 암호문 송신에 쓴다
	// @param disconnect 세션을 끊는 콜백 — TLS 핸드셰이크/복호화 실패 시 호출한다
	//***************************************************************************
	void Bind(SelfFn self, TlsRawSendFn rawSend, DisconnectFn disconnect)
	{
		_self = std::move(self);
		_rawSend = std::move(rawSend);
		_disconnect = std::move(disconnect);
	}

	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	// @details 실제로는 CHttpConnPoolT::Create()의 세션 팩토리가 세션 생성 직후 이 콜백으로
	//          자기 자신(풀)에 연결한다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

	//***************************************************************************
	// @brief 이 채널을 HTTPS 모드로 전환합니다. 연결 시도 전에 호출해야 합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지 — 채널은 참조만 함)
	// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 hostname
	// @return bool CTlsFilter 초기화 성공 여부. 실패해도 HTTPS 모드는 유지되어(평문으로 조용히
	//         떨어지지 않는다) 연결 직후 핸드셰이크 실패로 세션이 끊긴다.
	//***************************************************************************
	bool EnableTls(SSL_CTX* sslCtx, const std::string& sniHostname)
	{
		_tlsEnabled = true;
		return _tlsFilter.Initialize(sslCtx, sniHostname,
			_rawSend,
			[this](const char* data, size_t len) { _httpCore.FeedRecv(data, len); },
			[this](bool success)
			{
				if( success )
					NotifyConnState(true);
				else if( _disconnect )
					_disconnect();
			});
	}

	bool IsTlsEnabled() const noexcept { return _tlsEnabled; }

	//***************************************************************************
	// @brief 연결 완료 시 호출합니다. 평문이면 즉시 통지, HTTPS면 핸드셰이크만 시작하고 실제
	//        통지는 핸드셰이크 완료 콜백에서 합니다.
	//***************************************************************************
	void OnConnected()
	{
		if( _tlsEnabled )
		{
			_tlsFilter.StartHandshake();
			return;
		}

		NotifyConnState(true);
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출합니다. 진행 중이던 요청을 마무리하고 풀에 connected=false를 통지합니다.
	// @param graceful 상대의 정상 종료(FIN)였는지 — 연결 종료로 끝나는 응답 본문을 완결시키는 데 쓴다
	//***************************************************************************
	void OnDisconnected(bool graceful)
	{
		_httpCore.OnSessionDisconnected(graceful);
		NotifyConnState(false);
	}

	//***************************************************************************
	// @brief 수신 바이트를 처리합니다. 평문이면 CHttpClientCore로 직접, HTTPS면 CTlsFilter를 거쳐
	//        복호화한 뒤 전달합니다 (둘 다 부분 상태를 자체 보유하므로 len 전량을 소비한다).
	//***************************************************************************
	void OnRecv(const char* data, size_t len)
	{
		if( _tlsEnabled )
			_tlsFilter.FeedNetworkData(data, len);
		else
			_httpCore.FeedRecv(data, len);
	}

	//***************************************************************************
	// @brief 송신이 진행될 때마다 요청의 무응답 타임아웃 기준 시각을 갱신합니다 — 큰 요청 본문을 느린
	//        회선으로 올리는 동안 응답이 없다는 이유로 요청이 끊기지 않게 한다.
	//***************************************************************************
	void NoteSendProgress() noexcept { _httpCore.NoteActivity(); }

	//***************************************************************************
	// @brief 완성된 요청 패킷을 전송하고, 응답이 완결되면 onComplete를 호출합니다.
	//        HTTPS 모드면 CTlsFilter를 거쳐 자동으로 암호화된다.
	// @return bool false면 (a) 이전 요청이 아직 진행 중이거나 (b) 전송 자체가 실패한 것 — 어느 쪽이든
	//         이 세션은 재사용하지 말고 풀이 폐기 처리해야 한다.
	//***************************************************************************
	bool SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout, CHttpClientCore::Clock::time_point submitTime)
	{
		if( _tlsEnabled )
		{
			return _httpCore.BeginRequest(
				[this](const void* d, uint16 n) { return _tlsFilter.SendPlaintext(d, n); },
				data, len, std::move(onComplete), timeout, submitTime);
		}

		return _httpCore.BeginRequest(_rawSend, data, len, std::move(onComplete), timeout, submitTime);
	}

	bool IsConnectionCloseRequested() const noexcept { return _httpCore.IsConnectionCloseRequested(); }

	//***************************************************************************
	// @brief 진행 중인 요청이 무응답 타임아웃을 넘겼으면 실패로 통지하고 중단합니다 (풀의 주기 점검이 호출).
	// @return 이번 호출로 요청을 타임아웃 처리했으면 true
	//***************************************************************************
	bool CheckRequestTimeout(CHttpClientCore::Clock::time_point now) { return _httpCore.CheckTimeout(now); }

	EHttpClientState GetHttpState() const noexcept { return _httpCore.GetState(); }

private:
	void NotifyConnState(bool connected)
	{
		if( _connStateHandler && _self )
			_connStateHandler(_self(), connected);
	}

private:
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진/TLS 비의존)
	CTlsFilter _tlsFilter;                   // TLS 암복호화 계층 (_tlsEnabled==false면 미사용)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화를 풀에 통지하는 콜백
	SelfFn _self;                            // 세션 자신의 CSessionRef를 만들어 주는 콜백 (순환 참조 방지용으로 보관하지 않고 호출)
	TlsRawSendFn _rawSend;                   // 세션의 Send() — 평문 송신/TLS 암호문 송신
	DisconnectFn _disconnect;                // TLS 실패 시 세션을 끊는 콜백
	bool _tlsEnabled = false;                // EnableTls() 호출 여부 (true면 HTTPS 모드)
};

#endif // ndef UC_HTTPCLIENTCHANNEL_H