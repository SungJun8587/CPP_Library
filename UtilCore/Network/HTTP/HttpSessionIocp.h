
//***************************************************************************
// HttpSessionIocp.h : interface for the CHttpSessionIocp class.
//
//***************************************************************************

#ifndef UC_HTTPSESSIONIOCP_H
#define UC_HTTPSESSIONIOCP_H

#include <Network/NetworkRedefineDataType.h>
#include <Network/IOCP/IocpSession.h>
#include <Network/HTTP/HttpClientChannel.h>

//***************************************************************************
// @class CHttpSessionIocp
// @brief CIocpSession + CHttpClientChannel(합성) — IOCP 엔진에서 HTTP/HTTPS 요청/응답을
//        주고받는 세션 (평문/TLS 겸용)
//
// @details
//      HTTP 요청/응답 처리, 평문/TLS 분기, 연결 상태 통지는 엔진에 의존하지 않는
//      CHttpClientChannel이 담당하고, 이 클래스는 IOCP 세션의 훅(OnConnected/OnDisconnected/
//      OnRecv/OnSend)을 채널로 연결하는 얇은 어댑터다. RIO 쪽 CHttpSessionRio도 같은 채널을 쓴다.
//
//      평문과 TLS는 세션 생성 직후 SetTlsConfig() 호출 여부로 결정된다 — 호출 안 하면(기본값)
//      평문 HTTP, 호출하면 그 순간부터 HTTPS로 동작한다. SetTlsConfig()는 연결 시도
//      (ConnectAsync()) 전에 해야 하며, CHttpConnPoolT::Create()의 initSession 훅으로 자동화하는
//      것이 정석이다(HttpConnPoolFactory.h의 CreateHttpsConnPoolIocp() 참고).
//
//      [상속 가능] HttpConnPoolFactory.h/HttpClient.h의 Create* 함수들은 SessionType 템플릿
//      파라미터(기본값 CHttpSessionIocp)를 받으므로, 호출부가 이 클래스를 상속해
//      OnConnected()/OnDisconnected()/OnRecv() 등을 오버라이드하거나 멤버를 추가한 커스텀
//      세션을 꽂아 넣을 수 있다. SendRequest()/SetTlsConfig() 등은 virtual이 아니지만,
//      CHttpConnPoolT가 항상 TSessionRef(=구체 타입의 shared_ptr)를 통해 정적으로 호출하므로
//      서브클래스가 이 함수들을 재정의(이름 가리기)해도 정상적으로 동작한다.
//***************************************************************************
class CHttpSessionIocp : public CIocpSession
{
public:
	// HTTP 클라이언트 연결의 keep-alive 확인 주기: 유휴 30초 후 첫 확인, 이후 10초 간격
	static constexpr uint32 kKeepAliveIdleMs = HTTP::kClientKeepAliveIdleMs;
	static constexpr uint32 kKeepAliveIntervalMs = HTTP::kClientKeepAliveIntervalMs;

	//***************************************************************************
	// @brief 생성자 — HTTP 클라이언트 연결의 기본 소켓 옵션을 지정하고 채널을 세션에 연결합니다.
	// @details TCP_NODELAY를 켜서 작은 요청/응답이 Nagle 알고리즘과 상대의 지연 ACK에 걸려 지연되지 않게 하고,
	//          keep-alive를 켜서 풀의 유휴 연결이 FIN/RST 없이 조용히 끊긴 것을 일찍 감지한다. 다르게 쓰려면
	//          CHttpConnPoolT::Create()의 initSession 콜백에서 SetSocketOptions()로 덮어쓰면 된다.
	//***************************************************************************
	CHttpSessionIocp()
	{
		SocketOptions options;
		options.noDelay = true;
		options.keepAlive = true;
		options.keepAliveIdleMs = kKeepAliveIdleMs;
		options.keepAliveIntervalMs = kKeepAliveIntervalMs;
		SetSocketOptions(options);

		_channel.Bind(
			[this]() -> CSessionRef { return shared_from_this(); },
			[this](const void* data, uint16 size) { return Send(data, size); },
			[this]() { Disconnect(_T("TLS failure")); });
	}

	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	// @details 실제로는 CHttpConnPoolT::Create()의 세션 팩토리가 세션 생성
	//          직후 이 함수로 자기 자신(풀)에 연결한다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _channel.SetConnStateHandler(std::move(handler)); }

	//***************************************************************************
	// @brief 이 세션을 HTTPS 모드로 전환합니다. 연결 시도 전에 호출해야 합니다.
	//        호출하지 않으면 평문 HTTP로 동작합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (CreateDefaultClientSslCtx() 등으로
	//        생성, 호출부가 소유권 유지 — 이 세션은 참조만 함)
	// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 hostname
	// @return bool CTlsFilter 초기화 성공 여부
	//***************************************************************************
	bool SetTlsConfig(SSL_CTX* sslCtx, const std::string& sniHostname)
	{
		return _channel.EnableTls(sslCtx, sniHostname);
	}

	//***************************************************************************
	// @brief 완성된 요청 패킷을 전송하고, 응답이 완결되면 onComplete를 호출합니다.
	//        HTTPS 모드면 CTlsFilter를 거쳐 자동으로 암호화된다.
	// @param data 완성된 요청 패킷 바이트 (평문)
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool false면 (a) 이전 요청이 아직 진행 중이거나 (b) 전송 자체가
	//         실패한 것 — 어느 쪽이든 이 세션은 재사용하지 말고 풀이 폐기 처리해야 한다.
	//***************************************************************************
	bool SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
		CHttpClientCore::Clock::time_point submitTime = CHttpClientCore::Clock::now())
	{
		return _channel.SendRequest(data, len, std::move(onComplete), timeout, submitTime);
	}

	bool IsConnectionCloseRequested() const { return _channel.IsConnectionCloseRequested(); }

	//***************************************************************************
	// @brief 진행 중인 요청이 무응답 타임아웃을 넘겼으면 실패로 통지하고 중단합니다 (풀의 주기 점검이 호출).
	// @return 이번 호출로 요청을 타임아웃 처리했으면 true
	//***************************************************************************
	bool CheckRequestTimeout(CHttpClientCore::Clock::time_point now) { return _channel.CheckRequestTimeout(now); }
	EHttpClientState GetHttpState() const { return _channel.GetHttpState(); }

protected:
	// 송신이 진행될 때마다 요청의 무응답 타임아웃 기준 시각을 갱신한다 — 큰 요청 본문을 느린 회선으로
	// 올리는 동안 응답이 없다는 이유로 요청이 끊기지 않게 한다. (RIO 세션에는 대응하는 송신 완료 훅이 없다.)
	void OnSend(int32 len) override { _channel.NoteSendProgress(); }

	//***************************************************************************
	// @brief 연결 완료 시 호출됩니다. 평문이면 즉시 통지, HTTPS면 핸드셰이크만
	//        시작하고 실제 통지는 핸드셰이크 완료 시점에 합니다.
	//***************************************************************************
	void OnConnected() override { _channel.OnConnected(); }

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(TLS 실패로 인한 Disconnect() 포함). 진행 중이던 요청을
	//        마무리하고 풀에 connected=false를 통지합니다. 상대의 정상 종료(FIN)였는지 함께
	//        알려, 연결 종료로 끝나는 응답 본문을 완결시킵니다.
	//***************************************************************************
	void OnDisconnected() override { _channel.OnDisconnected(WasGracefulClose()); }

	//***************************************************************************
	// @brief 수신 바이트를 채널로 넘깁니다.
	// @return int32 처리한 바이트 수 (채널이 부분 상태를 자체 보유하므로 항상 len 전량)
	//***************************************************************************
	int32 OnRecv(BYTE* buffer, int32 len) override
	{
		_channel.OnRecv(reinterpret_cast<const char*>(buffer), static_cast<size_t>(len));
		return len;
	}

private:
	CHttpClientChannel _channel; // HTTP 요청/응답 + TLS + 연결 상태 통지 (엔진 비의존)
};

#endif // ndef UC_HTTPSESSIONIOCP_H