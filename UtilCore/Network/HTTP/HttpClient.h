
//***************************************************************************
// HttpClient.h : interface for the CHttpClient class.
//
//***************************************************************************

#ifndef UC_HTTPCLIENT_H
#define UC_HTTPCLIENT_H

#include <Network/HTTP/HttpUrl.h>
#include <Network/HTTP/HttpConnPoolFactory.h>
#include <Network/HTTP/HttpPacketBuilder.h>
#include <Network/HTTP/HttpFormUtil.h>
#include <Network/HTTP/HttpMultipartBuilder.h>

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <atomic>
#include <chrono>

//***************************************************************************
// @struct HttpResponse
// @brief CHttpClient가 호출부에 돌려주는 응답 스냅샷 (전부 소유한 값 — CHttpResponseParser
//        처럼 세션 내부 상태에 묶인 참조가 아니라, 콜백이 끝난 뒤에도 자유롭게 보관 가능)
//***************************************************************************
struct HttpResponse
{
	bool success = false;      // 요청이 끝까지 정상 처리됐는지 (false면 아래 필드는 의미 없음 — DNS
	// 실패/연결 실패/타임아웃/프로토콜 에러 등을 통틀어 구분 없이 표시)
	int statusCode = 0;        // HTTP 상태 코드
	std::string reasonPhrase;  // 상태 메시지 (Reason-Phrase)
	std::vector<std::pair<std::string, std::string>> headers; // 응답 헤더 (삽입 순서 보존)
	std::string body;          // 응답 본문 (바이트 그대로, Content-Type에 맞는 해석은 호출부 책임)
};

using HttpClientResponseHandler = std::function<void(HttpResponse)>;

//***************************************************************************
// @class CHttpClient
// @brief 지금까지 만든 HTTP 스택(빌더/파서/커넥션 풀/TLS)을 하나로 묶은 최상위
//        파사드 — URL 문자열 하나로 GET/POST(쿼리스트링/폼/JSON/파일업로드)
//        요청을 보낼 수 있다.
//
// @details
//      내부적으로 CHttpConnPoolManager 두 개(평문용/HTTPS용)를 갖고, URL의
//      scheme(http/https)에 따라 자동으로 골라 쓴다. 요청은 CHttpRequestBuilder로
//      조립하고, 응답은 CHttpConnPoolT가 최종 전달하는 CHttpResponseParser의
//      결과를 HttpResponse(소유 값 struct)로 복사해서 돌려준다 — 호출부가
//      CHttpResponseParser의 내부 수명 규칙을 신경 쓸 필요가 없게 하기 위함.
//
//      [HTTPS 미지원 상태로 생성 가능] CreateIocp/CreateRio()에 sslCtx를
//      nullptr로 넘기면 https:// URL 요청 시 즉시 실패(success=false) 처리된다
//      — 평문 전용으로만 쓸 거면 SSL_CTX를 안 만들어도 된다.
//
//      [사용 예]
//      auto client = CHttpClient::CreateIocp(iocpCore, CreateDefaultClientSslCtx());
//      client->Get("https://api.example.com/users?id=42",
//          [](HttpResponse resp) {
//              if (resp.success && resp.statusCode == 200) { ... resp.body ... }
//          });
//
//      std::string json = R"({"name":"claude"})";
//      client->PostJson("https://api.example.com/users", json,
//          [](HttpResponse resp) { ... });
//
//      client->PostForm("https://api.example.com/login",
//          {{"username","alice"},{"password","p@ss"}},
//          [](HttpResponse resp) { ... });
//
//      CMultipartFormBuilder form;
//      form.AddField("user_id", "42").AddFile("avatar", "photo.jpg", "image/jpeg", fileBytes);
//      client->PostMultipart("https://api.example.com/upload", form,
//          [](HttpResponse resp) { ... });
//***************************************************************************
class CHttpClient
{
public:
	//***************************************************************************
	// @brief IOCP 엔진 기반 클라이언트를 만듭니다.
	// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp). 커스텀
	//         세션(CHttpSessionIocp를 상속해 로깅/추가 상태 등을 붙인 클래스)을
	//         쓰려면 명시적으로 지정한다 — HttpConnPoolFactory.h의 Create*
	//         함수들이 이 타입을 그대로 실어 나른다.
	// @param iocpCore 이 클라이언트가 여는 모든 host 풀이 공유할 IOCP 코어
	// @param sslCtx HTTPS를 쓸 거면 CreateDefaultClientSslCtx() 등으로 만든
	//        SSL_CTX를 넘긴다(호출부가 소유권 유지, 클라이언트보다 오래
	//        살아있어야 함). nullptr이면 HTTPS 요청은 항상 실패 처리된다.
	// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수
	// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수
	// @param workerThreadCountPerHost host 풀 하나당 워커 스레드 개수
	// @return std::shared_ptr<CHttpClient> 생성된 클라이언트
	//***************************************************************************
	template<typename SessionType = CHttpSessionIocp>
	static std::shared_ptr<CHttpClient> CreateIocp(CIocpCoreRef iocpCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerIocp<SessionType>(iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		std::shared_ptr<CHttpConnPoolManager> httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerIocp<SessionType>(sslCtx, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return std::shared_ptr<CHttpClient>(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	//***************************************************************************
	// @brief RIO 엔진 기반 클라이언트를 만듭니다. 파라미터는 CreateIocp()와 대응.
	// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
	//***************************************************************************
	template<typename SessionType = CHttpSessionRio>
	static std::shared_ptr<CHttpClient> CreateRio(CRioCoreRef rioCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerRio<SessionType>(rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		std::shared_ptr<CHttpConnPoolManager> httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerRio<SessionType>(sslCtx, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return std::shared_ptr<CHttpClient>(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	//***************************************************************************
	// @brief 임의의 메서드/헤더/본문으로 요청을 보냅니다 (저수준 진입점 — 아래
	//        Get/PostJson/PostForm/PostMultipart는 전부 이 함수의 얇은 래퍼).
	// @param method HTTP 메서드 (예: "GET", "POST", "PUT", "DELETE")
	// @param url 전체 URL ("http://" 또는 "https://"로 시작해야 함)
	// @param headers 추가로 보낼 헤더 목록 (Host/Content-Length는 이 함수가 자동
	//        처리하므로 직접 넣지 않아도 됨 — 넣으면 중복 헤더가 생길 수 있음)
	// @param body 요청 본문 (없으면 빈 문자열)
	// @param onComplete 응답 완료(또는 실패) 시 호출되는 콜백
	// @details URL 파싱에 실패하면 onComplete를 즉시 success=false로 호출한다.
	//***************************************************************************
	void Request(std::string_view method, std::string_view url,
		const std::vector<std::pair<std::string, std::string>>& headers,
		std::string_view body, HttpClientResponseHandler onComplete)
	{
		CHttpUrl parsedUrl;
		if( !ParseHttpUrl(url, parsedUrl) )
		{
			if( onComplete )
				onComplete(HttpResponse{});
			return;
		}

		std::shared_ptr<CHttpConnPoolManager> manager = parsedUrl.isHttps ? _httpsManager : _httpManager;
		if( !manager )
		{
			// HTTPS 요청인데 SSL_CTX 없이 클라이언트를 만든 경우 등.
			if( onComplete )
				onComplete(HttpResponse{});
			return;
		}

		// Host 헤더: 기본 포트(80/443)면 hostname만, 아니면 hostname:port.
		std::string hostHeader = parsedUrl.host;
		bool isDefaultPort = (parsedUrl.isHttps && parsedUrl.port == 443) || (!parsedUrl.isHttps && parsedUrl.port == 80);
		if( !isDefaultPort )
			hostHeader += ":" + std::to_string(parsedUrl.port);

		CHttpRequestBuilder reqBuilder;
		reqBuilder.SetMethod(method).SetPath(parsedUrl.pathAndQuery).AddHeader("Host", hostHeader);
		for( const auto& [k, v] : headers )
			reqBuilder.AddHeader(k, v);
		if( !body.empty() )
			reqBuilder.SetBody(body);

		auto [data, len] = reqBuilder.Build();

		manager->SendRequest(parsedUrl.host, parsedUrl.port, data, len,
			[onComplete](bool success, CHttpResponseParser& parser)
			{
				HttpResponse resp;
				resp.success = success;
				if( success )
				{
					resp.statusCode = parser.GetStatusCode();
					resp.reasonPhrase = parser.GetReasonPhrase();
					resp.headers = parser.GetHeaders();
					resp.body = parser.GetBody();
				}
				if( onComplete )
					onComplete(std::move(resp));
			});
	}

	//***************************************************************************
	// @brief GET 요청을 보냅니다.
	// @param url 전체 URL (쿼리스트링을 붙이고 싶으면 http::BuildQueryString()으로
	//        미리 만들어서 url에 직접 이어붙이면 됨 — HttpFormUtil.h 참고)
	//***************************************************************************
	void Get(std::string_view url, HttpClientResponseHandler onComplete)
	{
		Request("GET", url, {}, {}, std::move(onComplete));
	}

	//***************************************************************************
	// @brief 커스텀 헤더를 포함한 GET 요청을 보냅니다.
	//***************************************************************************
	void Get(std::string_view url, const std::vector<std::pair<std::string, std::string>>& headers, HttpClientResponseHandler onComplete)
	{
		Request("GET", url, headers, {}, std::move(onComplete));
	}

	//***************************************************************************
	// @brief JSON 본문으로 POST 요청을 보냅니다. json은 이미 직렬화된 문자열이어야
	//        한다 — 이 클래스는 JSON 직렬화를 제공하지 않는다(호출부가 준비).
	//***************************************************************************
	void PostJson(std::string_view url, std::string_view json, HttpClientResponseHandler onComplete)
	{
		Request("POST", url, { {"Content-Type", "application/json"} }, json, std::move(onComplete));
	}

	//***************************************************************************
	// @brief application/x-www-form-urlencoded 본문으로 POST 요청을 보냅니다.
	// @param fields 폼 필드 (key, value) 목록 — HTTP::BuildFormUrlEncodedBody()로
	//        내부에서 자동 인코딩된다.
	//***************************************************************************
	void PostForm(std::string_view url, const std::vector<std::pair<std::string, std::string>>& fields, HttpClientResponseHandler onComplete)
	{
		std::string body = HTTP::BuildFormUrlEncodedBody(fields);
		Request("POST", url, { {"Content-Type", "application/x-www-form-urlencoded"} }, body, std::move(onComplete));
	}

	//***************************************************************************
	// @brief multipart/form-data(파일 업로드 포함) 본문으로 POST 요청을 보냅니다.
	// @param form 미리 필드/파일을 채워둔 CMultipartFormBuilder — 이 함수가
	//        Build()를 호출해 완성한다(form을 다시 쓰려면 호출부가 Reset()해야 함).
	//***************************************************************************
	void PostMultipart(std::string_view url, CMultipartFormBuilder& form, HttpClientResponseHandler onComplete)
	{
		auto [data, len] = form.Build();
		Request("POST", url, { {"Content-Type", std::string(form.GetContentTypeHeaderValue())} },
			std::string_view(data, len), std::move(onComplete));
	}

	//***************************************************************************
	// @brief 캐싱된 모든 host 커넥션 풀을 닫습니다.
	// @details Close()는 세션 정리를 비동기로 게시만 한다 — 실제 정리 완료는
	//          IOCP/RIO 워커 스레드가 완료 통지를 처리해야 끝난다. 워커 스레드를
	//          멈추기 전에 WaitUntilAllSessionsClosed() 또는
	//          SetAllSessionsClosedHandler()로 정리가 끝났음을 확인해야 하는
	//          이유는 그 함수들 설명 참고.
	//***************************************************************************
	void CloseAll()
	{
		if( _httpManager ) _httpManager->CloseAll();
		if( _httpsManager ) _httpsManager->CloseAll();
	}

	//***************************************************************************
	// @brief 평문/HTTPS 매니저를 통틀어 현재 활성 세션 수를 합산해 반환합니다.
	// @return size_t 전체 활성 세션 수 (마지막으로 갱신된 캐시값 — 폴링 아님)
	//***************************************************************************
	size_t GetTotalActiveSessionCount() const
	{
		size_t total = 0;
		if( _httpManager ) total += _httpManager->GetTotalActiveSessionCount();
		if( _httpsManager ) total += _httpsManager->GetTotalActiveSessionCount();
		return total;
	}

	//***************************************************************************
	// @brief 평문/HTTPS 양쪽 매니저의 세션 정리가 전부 끝날 때까지 블로킹 대기합니다.
	// @details condition_variable 기반이라 폴링하지 않는다 — 마지막 세션이 실제로
	//          정리되는 순간 즉시 깨어난다.
	//
	//          [권장 종료 시퀀스]
	//          client->CloseAll();
	//          client->WaitUntilAllSessionsClosed(); // 폴링 없이 즉시 깨어남
	//          iocpCore->PostQuit();
	//          workerThread.join();
	//***************************************************************************
	void WaitUntilAllSessionsClosed()
	{
		if( _httpManager ) _httpManager->WaitUntilAllSessionsClosed();
		if( _httpsManager ) _httpsManager->WaitUntilAllSessionsClosed();
	}

	//***************************************************************************
	// @brief 평문/HTTPS 양쪽 매니저의 세션 정리가 전부 끝나거나 timeout이 지날 때까지 대기합니다.
	// @param timeout 최대 대기 시간 (양쪽 매니저에 각각 적용 — 최악의 경우 최대 2*timeout)
	// @return bool 둘 다 정리 완료면 true, 하나라도 timeout이면 false
	//***************************************************************************
	bool WaitUntilAllSessionsClosed(std::chrono::milliseconds timeout)
	{
		bool ok = true;
		if( _httpManager ) ok = _httpManager->WaitUntilAllSessionsClosed(timeout) && ok;
		if( _httpsManager ) ok = _httpsManager->WaitUntilAllSessionsClosed(timeout) && ok;
		return ok;
	}

	//***************************************************************************
	// @brief 평문/HTTPS 양쪽 매니저의 세션 정리가 전부 끝나는 순간(비동기) 호출되는
	//        콜백을 등록합니다.
	// @param handler 양쪽 매니저 모두 세션 수 0에 도달했을 때 호출될 콜백
	// @details 블로킹 대기 대신 완전 비동기로 종료 흐름을 짜고 싶을 때 쓴다
	//          (예: 콜백 안에서 iocpCore->PostQuit() 호출). HTTPS 매니저가 없는
	//          클라이언트(sslCtx 없이 생성)면 평문 매니저 하나만으로 판단한다.
	//
	//          [사용 예 — 완전 비동기 종료]
	//          client->SetAllSessionsClosedHandler([iocpCore, &workerThread] {
	//              iocpCore->PostQuit();
	//              // 워커 스레드 join()은 여전히 워커 스레드가 아닌 다른
	//              // 스레드에서 해야 함 — 이 콜백 자체가 워커 스레드 위에서
	//              // 실행될 수 있으므로 여기서 join()을 직접 부르면 안 됨.
	//          });
	//          client->CloseAll();
	//***************************************************************************
	void SetAllSessionsClosedHandler(std::function<void()> handler)
	{
		if( !_httpsManager )
		{
			if( _httpManager ) _httpManager->SetAllSessionsClosedHandler(std::move(handler));
			return;
		}

		// 매니저가 둘이면, 둘 다 0에 도달한 뒤에만 호출해야 한다 — 공유 카운터로
		// "먼저 도달한 쪽"을 기록해두고 두 번째가 도달할 때 실제 handler를 부른다.
		auto remaining = std::make_shared<std::atomic<int>>(2);
		auto fireOnce = [remaining, handler]()
			{
				if( remaining->fetch_sub(1, std::memory_order_acq_rel) == 1 && handler )
					handler();
			};
		_httpManager->SetAllSessionsClosedHandler(fireOnce);
		_httpsManager->SetAllSessionsClosedHandler(fireOnce);
	}

private:
	CHttpClient(std::shared_ptr<CHttpConnPoolManager> httpManager, std::shared_ptr<CHttpConnPoolManager> httpsManager)
		: _httpManager(std::move(httpManager)), _httpsManager(std::move(httpsManager))
	{
	}

private:
	std::shared_ptr<CHttpConnPoolManager> _httpManager;		// http:// 요청용 (항상 유효)
	std::shared_ptr<CHttpConnPoolManager> _httpsManager;	// https:// 요청용 (sslCtx 없이 생성했으면 nullptr)
};

#endif // ndef UC_HTTPCLIENT_H