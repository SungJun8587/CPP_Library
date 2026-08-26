
//***************************************************************************
// HttpSessionIocp.h : interface for the CHttpSessionIocp class.
//
//***************************************************************************

#ifndef __HTTPSESSIONIOCP_H__
#define __HTTPSESSIONIOCP_H__

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

class CHttpSessionIocp;
using CHttpSessionIocpRef = std::shared_ptr<CHttpSessionIocp>;

//***************************************************************************
// @class CHttpSessionIocp
// @brief CIocpSession + CHttpClientCore(합성) — IOCP 엔진에서 HTTP/HTTPS 요청/
//        응답을 주고받는 세션 (평문/TLS 겸용, 구 CHttpSessionIocp+CHttpsSessionIocp 통합)
//
// @details
//      평문과 TLS는 세션 생성 직후 SetTlsConfig() 호출 여부로 결정된다 —
//      호출 안 하면(기본값) 평문 HTTP, 호출하면 그 순간부터 HTTPS로 동작한다.
//      두 모드의 차이는 딱 세 지점뿐이다(_tlsEnabled 플래그로 분기):
//        - OnConnected(): 평문은 바로 "연결됨" 통지, TLS는 핸드셰이크부터 시작
//          하고 핸드셰이크 완료 콜백에서야 통지(CTlsFilter가 대신 호출)
//        - OnRecv(): 평문은 바이트를 CHttpClientCore로 직접, TLS는 CTlsFilter를
//          거쳐 복호화한 뒤 전달
//        - SendRequest(): 평문은 Send()로 직접, TLS는 CTlsFilter::SendPlaintext()
//          로 암호화해서 전송
//      CTlsFilter는 평문 모드에서도 멤버로 항상 갖고 있지만(크기가 작고
//      Initialize() 전에는 SSL 객체를 만들지 않으므로 비용이 거의 없음),
//      _tlsEnabled==false인 동안은 전혀 쓰이지 않는다.
//
//      [분리했다가 다시 합친 이유] 원래는 CHttpSessionIocp(평문)/
//      CHttpsSessionIocp(TLS) 두 클래스로 나눠져 있었다 — 클래스마다 하는
//      일이 하나뿐이라 더 명확하긴 했지만, 두 파일에 걸쳐 똑같은 보일러플레이트
//      (SetConnStateHandler, IsConnectionCloseRequested, GetHttpState 등)가
//      중복되고, 그 중복 때문에 실제로 한쪽 파일에만 실수(HttpConnPoolCommon.h
//      include 누락)가 반복된 적이 있어 — 파일 수를 줄이는 쪽이 유지보수
//      관점에서 낫다고 판단해 다시 합쳤다.
//
//      [사용 전 설정] SetTlsConfig()를 호출하려면 연결 시도(ConnectAsync())
//      전에 해야 한다 — CHttpConnPoolT::Create()의 initSession 훅으로 자동화
//      하는 게 정석이다(HttpConnPoolFactory.h의 CreateHttpsConnPoolIocp() 참고).
//
//      [final이 아닌 이유] HttpConnPoolFactory.h/HttpClient.h의 Create* 함수들이
//      SessionType 템플릿 파라미터(기본값 CHttpSessionIocp)를 받도록 확장되면서,
//      호출부가 이 클래스를 상속해 OnConnected()/OnDisconnected()/OnRecv() 등을
//      오버라이드하거나 멤버를 추가한 커스텀 세션을 대신 꽂아 넣을 수 있게
//      됐다 — 그러려면 이 클래스가 상속 가능해야 하므로 final을 뗐다.
//      SendRequest()/SetTlsConfig() 등은 virtual이 아니지만, CHttpConnPoolT가
//      항상 TSessionRef(=구체 타입의 shared_ptr)를 통해 정적으로 호출하므로
//      서브클래스가 이 함수들을 재정의(이름 가리기)해도 정상적으로 동작한다.
//***************************************************************************
class CHttpSessionIocp : public CIocpSession
{
public:
	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	// @details 실제로는 CHttpConnPoolT::Create()의 세션 팩토리가 세션 생성
	//          직후 이 함수로 자기 자신(풀)에 연결한다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

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
					Disconnect(_T("TLS handshake failed"));
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
	// @brief 연결 완료 시 호출됩니다. 평문이면 즉시 통지, HTTPS면 핸드셰이크만
	//        시작하고 실제 통지는 핸드셰이크 완료 콜백(SetTlsConfig() 참고)에서.
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
	// @brief 연결 종료 시 호출됩니다(TLS 핸드셰이크 실패로 인한 Disconnect() 포함).
	//        등록된 콜백이 있으면 connected=false로 통지합니다.
	//***************************************************************************
	void OnDisconnected() override
	{
		_httpCore.OnSessionDisconnected();

		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신 바이트를 처리합니다. 평문이면 CHttpClientCore로 직접, HTTPS면
	//        CTlsFilter를 거쳐 복호화한 뒤 전달합니다.
	// @param buffer 수신 바이트 포인터
	// @param len buffer의 길이
	// @return int32 처리한 바이트 수 (둘 다 부분 상태를 자체 보유하므로 항상 len 전량)
	//***************************************************************************
	int32 OnRecv(BYTE* buffer, int32 len) override
	{
		if( _tlsEnabled )
			_tlsFilter.FeedNetworkData(reinterpret_cast<char*>(buffer), static_cast<size_t>(len));
		else
			_httpCore.FeedRecv(reinterpret_cast<char*>(buffer), static_cast<size_t>(len));

		return len;
	}

private:
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진/TLS 비의존)
	CTlsFilter _tlsFilter;                   // TLS 암복호화 계층 (_tlsEnabled==false면 미사용)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화를 풀에 통지하는 콜백
	bool _tlsEnabled = false;                // SetTlsConfig() 호출 여부 (true면 HTTPS 모드)
};

#endif // ndef __HTTPSESSIONIOCP_H__