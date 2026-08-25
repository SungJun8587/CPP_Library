
//***************************************************************************
// HttpsSessionIocp.h : interface for the CHttpsSessionIocp class.
//
//***************************************************************************

#ifndef __HTTPSSESSIONIOCP_H__
#define __HTTPSSESSIONIOCP_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__IOCPSESSION_H__
#include <Network/IOCP/IocpSession.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__TLSFILTER_H__
#include <Network/HTTP/TlsFilter.h>
#endif

//***************************************************************************
// @class CHttpsSessionIocp
// @brief CIocpSession + CTlsFilter + CHttpClientCore(합성) — IOCP 엔진에서
//        HTTPS 요청/응답을 주고받는 세션 (CHttpSessionIocp의 TLS 버전)
//
// @details
//      CHttpSessionIocp와의 유일한 차이는 CTlsFilter가 소켓과 CHttpClientCore
//      사이에 끼어든다는 것뿐이다:
//        - 송신: CHttpClientCore::BeginRequest() -> CTlsFilter::SendPlaintext()
//          (암호화) -> Send()(실제 소켓)
//        - 수신: OnRecv()(암호문) -> CTlsFilter::FeedNetworkData() -> 복호화 ->
//          CHttpClientCore::FeedRecv()(평문)
//      TCP 연결이 끝났다고 바로 "연결됨"으로 통지하지 않는다 — OnConnected()는
//      TLS 핸드셰이크를 시작만 하고, 실제 통지(_connStateHandler(true))는
//      핸드셰이크가 성공적으로 끝난 뒤(CTlsFilter의 handshake-complete 콜백)에야
//      나간다. 핸드셰이크가 실패하면(인증서 검증 실패 등) 그 콜백에서 바로
//      Disconnect()해서 세션을 폐기한다.
//
//      [사용 전 필수 설정] 이 세션은 기본 생성자로 만들어지므로(SessionFactory
//      계약), SSL_CTX와 SNI 호스트네임을 생성 직후 SetTlsConfig()로 주입해야
//      한다 — CHttpConnPoolT::Create()의 initSession 훅으로 자동화하는 게
//      정석이다(HttpConnPoolFactory.h의 CreateHttpsConnPoolIocp() 참고).
//      SetTlsConfig() 호출 전에 OnConnected()가 불리면(정상 흐름에서는 있을 수
//      없지만 방어적으로) TLS 필터 초기화가 안 된 상태이므로 즉시 실패 처리한다.
//***************************************************************************
class CHttpsSessionIocp final : public CIocpSession
{
public:
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

	//***************************************************************************
	// @brief TLS 설정을 주입합니다. 세션 생성 직후, 연결 시도 전에 반드시 호출해야 합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (CreateDefaultClientSslCtx() 등으로 생성,
	//        호출부가 소유권을 유지 — 이 세션은 참조만 함)
	// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 hostname
	// @return bool CTlsFilter 초기화 성공 여부
	//***************************************************************************
	bool SetTlsConfig(SSL_CTX* sslCtx, const std::string& sniHostname)
	{
		return _tlsFilter.Initialize(sslCtx, sniHostname,
			[this](const void* d, uint16_t n) { return Send(d, n); },
			[this](const char* data, size_t len) { _httpCore.FeedRecv(data, len); },
			[this](bool success)
			{
				if( success )
				{
					if( _connStateHandler )
						_connStateHandler(shared_from_this(), true);
				}
				else
				{
					Disconnect(_T("TLS handshake failed"));
				}
			});
	}

	//***************************************************************************
	// @brief 완성된 요청 패킷을 (암호화해서) 전송하고, 응답이 완결되면 onComplete를 호출합니다.
	// @param data 완성된 요청 패킷 바이트 (평문)
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool false면 (a) 이전 요청이 아직 진행 중이거나 (b) TLS 암호화/
	//         전송 자체가 실패한 것 — 어느 쪽이든 이 세션은 재사용하지 말고
	//         풀이 폐기 처리해야 한다.
	//***************************************************************************
	bool SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		return _httpCore.BeginRequest(
			[this](const void* d, uint16_t n) { return _tlsFilter.SendPlaintext(d, n); },
			data, len, std::move(onComplete));
	}

	bool IsConnectionCloseRequested() const { return _httpCore.IsConnectionCloseRequested(); }
	EHttpClientState GetHttpState() const { return _httpCore.GetState(); }

protected:
	//***************************************************************************
	// @brief TCP 연결 완료 시 호출됩니다. HTTP 계층에는 아직 알리지 않고 TLS
	//        핸드셰이크만 시작한다 — 실제 통지는 핸드셰이크 완료 콜백에서.
	//***************************************************************************
	void OnConnected() override
	{
		_tlsFilter.StartHandshake();
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(TCP 종료, TLS 핸드셰이크 실패로 인한
	//        Disconnect() 포함). 등록된 콜백이 있으면 connected=false로 통지합니다.
	//***************************************************************************
	void OnDisconnected() override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신한 암호문을 CTlsFilter로 넘겨 핸드셰이크 진행 또는 복호화를 수행합니다.
	// @param buffer 수신 바이트 포인터 (암호문)
	// @param len buffer의 길이
	// @return int32 처리한 바이트 수 (CTlsFilter가 부분 상태를 자체 보유하므로 항상 len 전량)
	//***************************************************************************
	int32 OnRecv(BYTE* buffer, int32 len) override
	{
		_tlsFilter.FeedNetworkData(reinterpret_cast<char*>(buffer), static_cast<size_t>(len));
		return len;
	}

private:
	CTlsFilter _tlsFilter;                   // 소켓과 CHttpClientCore 사이의 TLS 암복호화 계층
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진/TLS 비의존)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화(=TLS 핸드셰이크 완료 기준)를 풀에 통지하는 콜백
};

#endif // ndef __HTTPSSESSIONIOCP_H__