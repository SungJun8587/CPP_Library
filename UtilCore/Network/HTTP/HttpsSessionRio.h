//***************************************************************************
// HttpsSessionRio.h : interface for the CHttpsSessionRio class.
//
//***************************************************************************

#ifndef __HTTPSSESSIONRIO_H__
#define __HTTPSSESSIONRIO_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__RIOSESSION_H__
#include <Network/RIO/RioSession.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__TLSFILTER_H__
#include <Network/HTTP/TlsFilter.h>
#endif

//***************************************************************************
// @class CHttpsSessionRio
// @brief CRioSession + CTlsFilter + CHttpClientCore(합성) — RIO 엔진에서
//        HTTPS 요청/응답을 주고받는 세션 (CHttpSessionRio의 TLS 버전)
//
// @details
//      CHttpSessionRio와의 관계는 CHttpsSessionIocp와 CHttpSessionIocp의
//      관계와 동일하다 — CTlsFilter가 소켓과 CHttpClientCore 사이에 끼어들고,
//      "연결됨" 통지는 TCP 연결이 아니라 TLS 핸드셰이크 완료를 기준으로 나간다.
//      OnDataReceived()가 GetRecvBuffer()를 직접 소비해야 하는 RIO 특유의
//      패턴(CHttpSessionRio.h 클래스 설명 참고)은 그대로이되, 꺼낸 바이트를
//      CHttpClientCore가 아니라 CTlsFilter로 먼저 넘긴다.
//
//      [사용 전 필수 설정] SetTlsConfig()를 세션 생성 직후, 연결 시도 전에
//      반드시 호출해야 한다 — HttpConnPoolFactory.h의 CreateHttpsConnPoolRio()가
//      CHttpConnPoolT::Create()의 initSession 훅으로 자동 처리한다.
//***************************************************************************
class CHttpsSessionRio final : public CRioSession
{
public:
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

	//***************************************************************************
	// @brief TLS 설정을 주입합니다. 세션 생성 직후, 연결 시도 전에 반드시 호출해야 합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
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
					Close(Rio::CloseReason::InternalError);
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
	// @brief TCP 연결(Init()) 완료 시 호출됩니다. HTTP 계층에는 아직 알리지
	//        않고 TLS 핸드셰이크만 시작한다 — 실제 통지는 핸드셰이크 완료 콜백에서.
	//***************************************************************************
	void OnConnected() override
	{
		_tlsFilter.StartHandshake();
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(TLS 핸드셰이크 실패로 인한 Close() 포함).
	//        등록된 콜백이 있으면 connected=false로 통지합니다. reason은 풀
	//        쪽에서 쓰지 않으므로 무시.
	//***************************************************************************
	void OnDisconnected(Rio::CloseReason /*reason*/) override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신 링버퍼를 직접 소비해 암호문을 CTlsFilter로 넘깁니다.
	// @details CHttpSessionRio::OnDataReceived()와 동일한 경계 래핑 방지 패턴 —
	//          차이는 넘기는 대상이 CHttpClientCore가 아니라 CTlsFilter라는 것뿐.
	//***************************************************************************
	void OnDataReceived() override
	{
		while( true )
		{
			int64 dataSize = GetRecvBuffer().GetSizeUsed();
			if( dataSize <= 0 )
				break;

			int64 directSize = GetRecvBuffer().GetSizeDirectDequeueAble();
			BYTE* readPos = reinterpret_cast<BYTE*>(GetRecvBuffer().GetReadBuffer());

			_tlsFilter.FeedNetworkData(reinterpret_cast<char*>(readPos), static_cast<size_t>(directSize));

			GetRecvBuffer().MoveReadBuffer(directSize);
		}
	}

private:
	CTlsFilter _tlsFilter;                   // 소켓과 CHttpClientCore 사이의 TLS 암복호화 계층
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진/TLS 비의존)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화(=TLS 핸드셰이크 완료 기준)를 풀에 통지하는 콜백
};

#endif // ndef __HTTPSSESSIONRIO_H__