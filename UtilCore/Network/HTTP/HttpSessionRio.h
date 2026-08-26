
//***************************************************************************
// HttpSessionRio.h : interface for the CHttpSessionRio class.
//
//***************************************************************************

#ifndef __HTTPSESSIONRIO_H__
#define __HTTPSESSIONRIO_H__

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

class CHttpSessionRio;
using CHttpSessionRioRef = std::shared_ptr<CHttpSessionRio>;

//***************************************************************************
// @class CHttpSessionRio
// @brief CRioSession + CHttpClientCore(합성) — RIO 엔진에서 HTTP/HTTPS 요청/
//        응답을 주고받는 세션 (평문/TLS 겸용, 구 CHttpSessionRio+CHttpsSessionRio 통합)
//
// @details
//      CHttpSessionIocp와의 관계는 CIocpSession/CRioSession의 관계와 같다 —
//      평문/TLS 분기 방식(SetTlsConfig() 호출 여부, _tlsEnabled 플래그)은
//      완전히 동일하고, 차이는 OnDataReceived()가 RIO 특유의 패턴(파라미터
//      없이 호출되어 GetRecvBuffer()를 직접 소비해야 함)을 쓴다는 점뿐이다.
//
//      [사용 전 설정] SetTlsConfig()를 호출하려면 연결 시도 전에 해야 한다 —
//      HttpConnPoolFactory.h의 CreateHttpsConnPoolRio()가 CHttpConnPoolT::
//      Create()의 initSession 훅으로 자동 처리한다.
//
//      [final이 아닌 이유] CHttpSessionIocp와 동일 — HttpConnPoolFactory.h/
//      HttpClient.h의 Create* 함수들이 SessionType 템플릿 파라미터(기본값
//      CHttpSessionRio)를 받도록 확장되면서, 호출부가 이 클래스를 상속한
//      커스텀 세션을 대신 꽂아 넣을 수 있게 됐다.
//***************************************************************************
class CHttpSessionRio : public CRioSession
{
public:
	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

	//***************************************************************************
	// @brief 이 세션을 HTTPS 모드로 전환합니다. 연결 시도 전에 호출해야 합니다.
	//        호출하지 않으면 평문 HTTP로 동작합니다.
	// @param sslCtx 여러 커넥션이 공유하는 SSL_CTX (호출부가 소유권 유지)
	// @param sniHostname TLS SNI 및 인증서 호스트네임 검증에 쓸 hostname
	// @return bool CTlsFilter 초기화 성공 여부
	//***************************************************************************
	bool SetTlsConfig(SSL_CTX* sslCtx, const std::string& sniHostname)
	{
		_tlsEnabled = true;
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
	// @brief 완성된 요청 패킷을 전송하고, 응답이 완결되면 onComplete를 호출합니다.
	//        HTTPS 모드면 CTlsFilter를 거쳐 자동으로 암호화된다.
	// @param data 완성된 요청 패킷 바이트 (평문)
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool false면 (a) 이전 요청이 아직 진행 중이거나 (b) 전송 자체가
	//         실패한 것 — 어느 쪽이든 이 세션은 재사용하지 말고 풀이 폐기 처리해야 한다.
	//***************************************************************************
	bool SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		if( _tlsEnabled )
		{
			return _httpCore.BeginRequest(
				[this](const void* d, uint16_t n) { return _tlsFilter.SendPlaintext(d, n); },
				data, len, std::move(onComplete));
		}

		return _httpCore.BeginRequest(
			[this](const void* d, uint16_t n) { return Send(d, n); },
			data, len, std::move(onComplete));
	}

	bool IsConnectionCloseRequested() const { return _httpCore.IsConnectionCloseRequested(); }
	EHttpClientState GetHttpState() const { return _httpCore.GetState(); }

protected:
	//***************************************************************************
	// @brief 연결(Init()) 완료 시 호출됩니다. 평문이면 즉시 통지, HTTPS면
	//        핸드셰이크만 시작하고 실제 통지는 핸드셰이크 완료 콜백에서.
	//***************************************************************************
	void OnConnected() override
	{
		if( _tlsEnabled )
		{
			_tlsFilter.StartHandshake();
			return;
		}

		if( _connStateHandler )
			_connStateHandler(shared_from_this(), true);
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(TLS 핸드셰이크 실패로 인한 Close() 포함).
	//        등록된 콜백이 있으면 connected=false로 통지합니다. reason은 풀
	//        쪽에서 쓰지 않으므로 무시.
	//***************************************************************************
	void OnDisconnected(Rio::CloseReason /*reason*/) override
	{
		_httpCore.OnSessionDisconnected();

		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신 링버퍼를 직접 소비해 처리합니다. 평문이면 CHttpClientCore로
	//        직접, HTTPS면 CTlsFilter를 거쳐 복호화한 뒤 전달합니다.
	// @details 경계 래핑(wrap-around) 오버런 방지를 위해 GetSizeDirectDequeueAble()
	//          크기만큼만 한 번에 처리하며, 남은 데이터가 있으면 루프를 반복한다
	//          (CIocpSession::ProcessRecv()가 프레임워크 차원에서 해주는 것과
	//          달리 RIO는 OnDataReceived()가 직접 이 처리를 해야 함).
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

			if( _tlsEnabled )
				_tlsFilter.FeedNetworkData(reinterpret_cast<char*>(readPos), static_cast<size_t>(directSize));
			else
				_httpCore.FeedRecv(reinterpret_cast<char*>(readPos), static_cast<size_t>(directSize));

			GetRecvBuffer().MoveReadBuffer(directSize);
		}
	}

private:
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진/TLS 비의존)
	CTlsFilter _tlsFilter;                   // TLS 암복호화 계층 (_tlsEnabled==false면 미사용)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화를 풀에 통지하는 콜백
	bool _tlsEnabled = false;                // SetTlsConfig() 호출 여부 (true면 HTTPS 모드)
};

#endif // ndef __HTTPSESSIONRIO_H__