
//***************************************************************************
// HttpClient.cpp : implementation of the CHttpClient class.
//
//***************************************************************************

#include "pch.h"
#include "HttpClient.h"

//***************************************************************************
// @brief 임의의 메서드/헤더/본문으로 요청을 보냅니다 (저수준 진입점 — 아래
//        Get/PostJson/PostForm/PostMultipart는 전부 이 함수의 얇은 래퍼).
// @param method HTTP 메서드 (예: "GET", "POST", "PUT", "DELETE")
// @param url 전체 URL ("http://" 또는 "https://"로 시작해야 함)
// @param headers 추가로 보낼 헤더 목록 (Host/Content-Length는 이 함수가 자동
//        처리하므로 직접 넣지 않아도 됨 — 넣으면 중복 헤더가 생길 수 있음)
// @param body 요청 본문 (없으면 빈 문자열)
// @param onComplete 응답 완료(또는 실패) 시 호출되는 콜백
// @details URL 파싱 실패, 메서드/헤더의 제어 문자(요청 분할 방지), HTTPS 미지원 구성 등으로 요청을
//          만들 수 없으면 onComplete를 즉시 success=false로 호출한다.
//***************************************************************************
void CHttpClient::Request(std::string_view method, std::string_view url,
	const std::vector<std::pair<std::string, std::string>>& headers,
	std::string_view body, HttpClientResponseHandler onComplete)
{
	Request(method, url, headers, body, std::chrono::milliseconds::zero(), std::move(onComplete));
}

//***************************************************************************
// @brief 요청별 타임아웃을 지정하는 Request() 오버로드.
// @param timeout 무응답 타임아웃. 0이면 기본값(SetDefaultTimeout, 설정하지 않았으면 30초), 음수면 이 요청은
//        타임아웃 없음. 기준 시각은 호출 시점(연결 대기 시간 포함)이며 응답 데이터가 도착할 때마다 갱신된다.
//        초과하면 onComplete에 success=false, timedOut=true인 HttpResponse가 전달된다.
//***************************************************************************
void CHttpClient::Request(std::string_view method, std::string_view url,
	const std::vector<std::pair<std::string, std::string>>& headers,
	std::string_view body, std::chrono::milliseconds timeout, HttpClientResponseHandler onComplete)
{
	CHttpUrl parsedUrl;
	if( !ParseHttpUrl(url, parsedUrl) || !IsValidRequestHead(method, headers) )
	{
		if( onComplete )
			onComplete(HttpResponse{});
		return;
	}

	CHttpConnPoolManagerRef manager = parsedUrl.isHttps ? _httpsManager : _httpManager;
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
			resp.timedOut = !success && parser.IsTimedOut();
			if( success )
			{
				// 콜백이 끝나면 파서는 다음 응답을 위해 재사용되므로, 내용을 복사하지 않고 넘겨받는다.
				resp.statusCode = parser.GetStatusCode();
				resp.reasonPhrase = parser.GetReasonPhrase();
				resp.headers = parser.TakeHeaders();
				resp.body = parser.TakeBody();
			}
			if( onComplete )
				onComplete(std::move(resp));
		},
		timeout);
}

//***************************************************************************
// @brief GET 요청을 보냅니다.
// @param url 전체 URL (쿼리스트링을 붙이고 싶으면 HTTP::BuildQueryString()으로
//        미리 만들어서 url에 직접 이어붙이면 됨 — HttpFormUtil.h 참고)
//***************************************************************************
void CHttpClient::Get(std::string_view url, HttpClientResponseHandler onComplete)
{
	Request("GET", url, {}, {}, std::move(onComplete));
}

//***************************************************************************
// @brief 커스텀 헤더를 포함한 GET 요청을 보냅니다.
//***************************************************************************
void CHttpClient::Get(std::string_view url, const std::vector<std::pair<std::string, std::string>>& headers, HttpClientResponseHandler onComplete)
{
	Request("GET", url, headers, {}, std::move(onComplete));
}

//***************************************************************************
// @brief JSON 본문으로 POST 요청을 보냅니다. json은 이미 직렬화된 문자열이어야
//        한다 — 이 클래스는 JSON 직렬화를 제공하지 않는다(호출부가 준비).
//***************************************************************************
void CHttpClient::PostJson(std::string_view url, std::string_view json, HttpClientResponseHandler onComplete)
{
	Request("POST", url, { {"Content-Type", "application/json"} }, json, std::move(onComplete));
}

//***************************************************************************
// @brief application/x-www-form-urlencoded 본문으로 POST 요청을 보냅니다.
// @param fields 폼 필드 (key, value) 목록 — HTTP::BuildFormUrlEncodedBody()로
//        내부에서 자동 인코딩된다.
//***************************************************************************
void CHttpClient::PostForm(std::string_view url, const std::vector<std::pair<std::string, std::string>>& fields, HttpClientResponseHandler onComplete)
{
	std::string body = HTTP::BuildFormUrlEncodedBody(fields);
	Request("POST", url, { {"Content-Type", "application/x-www-form-urlencoded"} }, body, std::move(onComplete));
}

//***************************************************************************
// @brief multipart/form-data(파일 업로드 포함) 본문으로 POST 요청을 보냅니다.
// @param form 미리 필드/파일을 채워둔 CMultipartFormBuilder — 이 함수가
//        Build()를 호출해 완성한다(form을 다시 쓰려면 호출부가 Reset()해야 함).
//***************************************************************************
void CHttpClient::PostMultipart(std::string_view url, CMultipartFormBuilder& form, HttpClientResponseHandler onComplete)
{
	auto [data, len] = form.Build();
	Request("POST", url, { {"Content-Type", std::string(form.GetContentTypeHeaderValue())} },
		std::string_view(data, len), std::move(onComplete));
}

//***************************************************************************
// @brief 모든 호스트 풀의 기본 요청 타임아웃(무응답 시간)을 설정합니다. 0 이하면 기본적으로 타임아웃 없음.
//        설정하지 않으면 30초가 적용된다.
//***************************************************************************
void CHttpClient::SetDefaultTimeout(std::chrono::milliseconds timeout)
{
	if( _httpManager ) _httpManager->SetDefaultRequestTimeout(timeout);
	if( _httpsManager ) _httpsManager->SetDefaultRequestTimeout(timeout);
}

//***************************************************************************
// @brief 호스트 주소를 다시 해석하는 주기를 설정합니다 (기본 60초, 0 이하면 하지 않음).
//        주소가 바뀌었으면 이후 새로 만드는 연결부터 새 주소를 쓴다. 자세한 설명은
//        CHttpConnPoolManager::SetDnsRefreshInterval() 참고.
//***************************************************************************
void CHttpClient::SetDnsRefreshInterval(std::chrono::milliseconds interval)
{
	if( _httpManager ) _httpManager->SetDnsRefreshInterval(interval);
	if( _httpsManager ) _httpsManager->SetDnsRefreshInterval(interval);
}

//***************************************************************************
// @brief 캐싱된 모든 host 커넥션 풀을 닫습니다.
// @details 세션 정리 완료 통지는 Close() 이후에도 뒤늦게 올 수 있다. 워커 스레드를 멈추기 전에
//          WaitUntilAllSessionsClosed() 또는 SetAllSessionsClosedHandler()로 정리가 끝났음을
//          확인한다.
//***************************************************************************
void CHttpClient::CloseAll()
{
	if( _httpManager ) _httpManager->CloseAll();
	if( _httpsManager ) _httpsManager->CloseAll();
}

//***************************************************************************
// @brief 평문/HTTPS 매니저를 통틀어 현재 활성 세션 수를 합산해 반환합니다.
// @return size_t 전체 활성 세션 수 (마지막으로 갱신된 캐시값 — 폴링 아님)
//***************************************************************************
size_t CHttpClient::GetTotalActiveSessionCount() const
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
void CHttpClient::WaitUntilAllSessionsClosed()
{
	if( _httpManager ) _httpManager->WaitUntilAllSessionsClosed();
	if( _httpsManager ) _httpsManager->WaitUntilAllSessionsClosed();
}

//***************************************************************************
// @brief 평문/HTTPS 양쪽 매니저의 세션 정리가 전부 끝나거나 timeout이 지날 때까지 대기합니다.
// @param timeout 최대 대기 시간 (양쪽 매니저에 각각 적용 — 최악의 경우 최대 2*timeout)
// @return bool 둘 다 정리 완료면 true, 하나라도 timeout이면 false
//***************************************************************************
bool CHttpClient::WaitUntilAllSessionsClosed(std::chrono::milliseconds timeout)
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
// @details 블로킹 대기 대신 비동기로 종료 흐름을 짜고 싶을 때 쓴다(예: 콜백 안에서 코어 종료 요청).
//          1회성이며, 이미 0이면 즉시 호출된다. HTTPS 매니저가 없는 클라이언트(sslCtx 없이 생성)면
//          평문 매니저 하나만으로 판단한다.
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
void CHttpClient::SetAllSessionsClosedHandler(std::function<void()> handler)
{
	if( !_httpsManager )
	{
		if( _httpManager ) _httpManager->SetAllSessionsClosedHandler(std::move(handler));
		return;
	}

	// 매니저가 둘이면 둘 다 0에 도달한 뒤에만 호출한다 — 공유 카운터로 먼저 도달한 쪽을 기록해 두고
	// 두 번째가 도달할 때 실제 handler를 부른다.
	auto remaining = std::make_shared<std::atomic<int>>(2);
	auto fireOnce = [remaining, handler]()
		{
			if( remaining->fetch_sub(1, std::memory_order_acq_rel) == 1 && handler )
				handler();
		};
	_httpManager->SetAllSessionsClosedHandler(fireOnce);
	_httpsManager->SetAllSessionsClosedHandler(fireOnce);
}

//***************************************************************************
// @brief 메서드와 사용자 헤더가 요청 라인/헤더에 그대로 실려도 안전한지 검사합니다.
// @details 공백/제어 문자가 든 메서드나 헤더 이름, CR/LF 등 제어 문자가 든 헤더 값은 요청 분할
//          (추가 헤더/요청 주입)로 이어지므로 요청 자체를 거부한다.
//***************************************************************************
bool CHttpClient::IsValidRequestHead(std::string_view method, const std::vector<std::pair<std::string, std::string>>& headers)
{
	if( method.empty() || HTTP::ContainsControlOrSpace(method) )
		return false;

	for( const auto& [name, value] : headers )
	{
		if( name.empty() || HTTP::ContainsControlOrSpace(name) || name.find(':') != std::string::npos )
			return false;
		if( HTTP::ContainsControlChar(value) )
			return false;
	}
	return true;
}

CHttpClient::CHttpClient(CHttpConnPoolManagerRef httpManager, CHttpConnPoolManagerRef httpsManager)
	: _httpManager(std::move(httpManager)), _httpsManager(std::move(httpsManager))
{
}