
//***************************************************************************
// HttpSessionRio.h : interface for the CHttpSessionRio class.
//
//***************************************************************************

#ifndef UC_HTTPSESSIONRIO_H
#define UC_HTTPSESSIONRIO_H

#include <Network/NetworkRedefineDataType.h>
#include <Network/RIO/RioSession.h>
#include <Network/HTTP/HttpClientChannel.h>
#include <Network/SocketUtils.h>

//***************************************************************************
// @class CHttpSessionRio
// @brief CRioSession + CHttpClientChannel(합성) — RIO 엔진에서 HTTP/HTTPS 요청/응답을
//        주고받는 세션 (평문/TLS 겸용)
//
// @details
//      CHttpSessionIocp와 같은 구조다 — HTTP 요청/응답 처리, 평문/TLS 분기, 연결 상태 통지는
//      엔진 비의존 CHttpClientChannel이 담당하고, 이 클래스는 RIO 세션의 훅을 채널로 연결한다.
//      차이는 수신 훅뿐이다: OnDataReceived()가 파라미터 없이 호출되어 GetRecvBuffer()를 직접
//      소비해야 하고, 송신 완료 훅이 없다.
//
//      [사용 전 설정] SetTlsConfig()를 호출하려면 연결 시도 전에 해야 한다 —
//      HttpConnPoolFactory.h의 CreateHttpsConnPoolRio()가 CHttpConnPoolT::
//      Create()의 initSession 훅으로 자동 처리한다.
//
//      [상속 가능] CHttpSessionIocp와 동일 — SessionType 템플릿 파라미터(기본값
//      CHttpSessionRio)로 이 클래스를 상속한 커스텀 세션을 꽂아 넣을 수 있다.
//***************************************************************************
class CHttpSessionRio : public CRioSession
{
public:
	// HTTP 클라이언트 연결의 keep-alive 확인 주기: 유휴 30초 후 첫 확인, 이후 10초 간격
	static constexpr uint32 kKeepAliveIdleMs = HTTP::kClientKeepAliveIdleMs;
	static constexpr uint32 kKeepAliveIntervalMs = HTTP::kClientKeepAliveIntervalMs;

	//***************************************************************************
	// @brief 생성자 — 채널을 세션에 연결합니다.
	//***************************************************************************
	CHttpSessionRio()
	{
		_channel.Bind(
			[this]() -> CSessionRef { return shared_from_this(); },
			[this](const void* data, uint16 size) { return Send(data, size); },
			[this]() { Close(Rio::CloseReason::InternalError); });
	}

	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _channel.SetConnStateHandler(std::move(handler)); }

	//***************************************************************************
	// @brief 이 세션을 HTTPS 모드로 전환합니다. 연결 시도 전에 호출해야 합니다.
	//        호출하지 않으면 평문 HTTP로 동작합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
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
	//***************************************************************************
	// @brief 연결(Init()) 완료 시 호출됩니다. HTTP 클라이언트 연결의 기본 소켓 옵션(CHttpSessionIocp와
	//        동일)을 적용한 뒤 채널에 알립니다. 평문이면 즉시 통지, HTTPS면 핸드셰이크만 시작하고
	//        실제 통지는 핸드셰이크 완료 시점에 합니다.
	// @details Nagle를 끄고 keep-alive로 조용히 끊긴 연결을 일찍 감지한다. 옵션 설정이 실패해도
	//          연결은 계속한다.
	//***************************************************************************
	void OnConnected() override
	{
		CSocketUtils::SetNoDelay(GetSocket(), true);
		CSocketUtils::SetKeepAlive(GetSocket(), true, kKeepAliveIdleMs, kKeepAliveIntervalMs);

		_channel.OnConnected();
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(TLS 실패로 인한 Close() 포함). 진행 중이던 요청을 마무리하고
	//        풀에 connected=false를 통지합니다. reason은 상대의 정상 종료(RemoteClosed)였는지
	//        판단하는 데만 쓰며, 연결 종료로 끝나는 응답 본문을 완결시키는 데 쓰인다.
	//***************************************************************************
	void OnDisconnected(Rio::CloseReason reason) override
	{
		_channel.OnDisconnected(reason == Rio::CloseReason::RemoteClosed);
	}

	//***************************************************************************
	// @brief 수신 링버퍼를 직접 소비해 채널로 넘깁니다.
	// @details 경계 래핑(wrap-around) 오버런 방지를 위해 GetSizeDirectDequeueAble()
	//          크기만큼만 한 번에 처리하며, 남은 데이터가 있으면 루프를 반복한다
	//          (CIocpSession::ProcessRecv()가 프레임워크 차원에서 해주는 것과
	//          달리 RIO는 OnDataReceived()가 직접 이 처리를 해야 함).
	//***************************************************************************
	void OnDataReceived() override
	{
		while( true )
		{
			const int64 dataSize = GetRecvBuffer().GetSizeUsed();
			if( dataSize <= 0 )
				break;

			const int64 directSize = GetRecvBuffer().GetSizeDirectDequeueAble();
			if( directSize <= 0 )
				break; // 방어: 사용 중인 데이터가 있는데 연속 구간이 0이면 무한 루프를 피한다

			const char* readPos = reinterpret_cast<const char*>(GetRecvBuffer().GetReadBuffer());
			_channel.OnRecv(readPos, static_cast<size_t>(directSize));

			GetRecvBuffer().MoveReadBuffer(directSize);
		}
	}

private:
	CHttpClientChannel _channel; // HTTP 요청/응답 + TLS + 연결 상태 통지 (엔진 비의존)
};

#endif // ndef UC_HTTPSESSIONRIO_H