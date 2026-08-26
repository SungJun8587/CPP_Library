//***************************************************************************
// HttpClient.h : interface for the CHttpClient class.
//***************************************************************************

#ifndef __HTTPCLIENT_H__
#define __HTTPCLIENT_H__

#ifndef	__HTTPURL_H__
#include <Network/HTTP/HttpUrl.h>
#endif

#ifndef	__HTTPCONNPOOLFACTORY_H__
#include <Network/HTTP/HttpConnPoolFactory.h>
#endif

#ifndef	__HTTPPACKETBUILDER_H__
#include <Network/HTTP/HttpPacketBuilder.h>
#endif

#ifndef	__HTTPFORMUTIL_H__
#include <Network/HTTP/HttpFormUtil.h>
#endif

#ifndef	__HTTPMULTIPARTBUILDER_H__
#include <Network/HTTP/HttpMultipartBuilder.h>
#endif

#include <string>
#include <vector>
#include <utility>
#include <memory>

//***************************************************************************
// @struct HttpResponse
// @brief CHttpClient가 호출부에 돌려주는 응답 스냅샷
//***************************************************************************
struct HttpResponse
{
	bool success = false;                                     // 요청이 끝까지 정상 처리됐는지 여부
	int statusCode = 0;                                       // HTTP 상태 코드
	std::string reasonPhrase;                                 // 상태 메시지 (Reason-Phrase)
	std::vector<std::pair<std::string, std::string>> headers; // 응답 헤더 (삽입 순서 보존)
	std::string body;                                         // 응답 본문 (바이트 그대로 전달)
};

using HttpClientResponseHandler = std::function<void(HttpResponse)>;

//***************************************************************************
// @class CHttpClient
// @brief HTTP 스택(빌더/파서/커넥션 풀/TLS)을 하나로 묶은 최상위 파사드
//***************************************************************************
class CHttpClient
{
public:
	//***************************************************************************
	// @brief IOCP 엔진 기반 클라이언트를 만듭니다.
	// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionIocp)
	// @param iocpCore 이 클라이언트가 여는 모든 host 풀이 공유할 IOCP 코어
	// @param sslCtx HTTPS용 SSL_CTX (nullptr인 경우 HTTPS 요청 실패 처리)
	// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수
	// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수
	// @param workerThreadCountPerHost host 풀 하나당 워커 스레드 개수
	// @return std::shared_ptr<CHttpClient> 생성된 클라이언트
	//***************************************************************************
	template<typename SessionType = CHttpSessionIocp>
	static std::shared_ptr<CHttpClient> CreateIocp(CIocpCoreRef iocpCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerIocp<SessionType>(iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		CHttpConnPoolManagerRef httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerIocp<SessionType>(sslCtx, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return std::shared_ptr<CHttpClient>(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	//***************************************************************************
	// @brief RIO 엔진 기반 클라이언트를 만듭니다.
	// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
	// @param rioCore 이 클라이언트가 여는 모든 host 풀이 공유할 RIO 코어
	// @param sslCtx HTTPS용 SSL_CTX
	// @param minIdlePerHost host당 항상 유지할 기준 커넥션 수
	// @param maxConnectionsPerHost host당 허용할 최대 커넥션 수
	// @param workerThreadCountPerHost host 풀 하나당 워커 스레드 개수
	// @return std::shared_ptr<CHttpClient> 생성된 클라이언트
	//***************************************************************************
	template<typename SessionType = CHttpSessionRio>
	static std::shared_ptr<CHttpClient> CreateRio(CRioCoreRef rioCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32_t workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerRio<SessionType>(rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		CHttpConnPoolManagerRef httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerRio<SessionType>(sslCtx, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return std::shared_ptr<CHttpClient>(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	//***************************************************************************
	// @brief 임의의 메서드/헤더/본문으로 요청을 보냅니다 (저수준 진입점).
	// @param method HTTP 메서드 (예: "GET", "POST", "PUT", "DELETE")
	// @param url 전체 URL
	// @param headers 추가로 보낼 헤더 목록
	// @param body 요청 본문
	// @param onComplete 응답 완료(또는 실패) 시 호출되는 콜백
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

		CHttpConnPoolManagerRef manager = parsedUrl.isHttps ? _httpsManager : _httpManager;
		if( !manager )
		{
			if( onComplete )
				onComplete(HttpResponse{});
			return;
		}

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
	// @param url 전체 URL
	// @param onComplete 응답 콜백
	//***************************************************************************
	void Get(std::string_view url, HttpClientResponseHandler onComplete)
	{
		Request("GET", url, {}, {}, std::move(onComplete));
	}

	//***************************************************************************
	// @brief 커스텀 헤더를 포함한 GET 요청을 보냅니다.
	// @param url 전체 URL
	// @param headers 커스텀 헤더
	// @param onComplete 응답 콜백
	//***************************************************************************
	void Get(std::string_view url, const std::vector<std::pair<std::string, std::string>>& headers, HttpClientResponseHandler onComplete)
	{
		Request("GET", url, headers, {}, std::move(onComplete));
	}

	//***************************************************************************
	// @brief JSON 본문으로 POST 요청을 보냅니다.
	// @param url 전체 URL
	// @param json JSON 직렬화 문자열
	// @param onComplete 응답 콜백
	//***************************************************************************
	void PostJson(std::string_view url, std::string_view json, HttpClientResponseHandler onComplete)
	{
		Request("POST", url, { {"Content-Type", "application/json"} }, json, std::move(onComplete));
	}

	//***************************************************************************
	// @brief application/x-www-form-urlencoded 본문으로 POST 요청을 보냅니다.
	// @param url 전체 URL
	// @param fields 폼 필드 키-값 목록
	// @param onComplete 응답 콜백
	//***************************************************************************
	void PostForm(std::string_view url, const std::vector<std::pair<std::string, std::string>>& fields, HttpClientResponseHandler onComplete)
	{
		std::string body = HTTP::BuildFormUrlEncodedBody(fields);
		Request("POST", url, { {"Content-Type", "application/x-www-form-urlencoded"} }, body, std::move(onComplete));
	}

	//***************************************************************************
	// @brief multipart/form-data 본문으로 POST 요청을 보냅니다.
	// @param url 전체 URL
	// @param form 멀티파트 폼 빌더 객체
	// @param onComplete 응답 콜백
	//***************************************************************************
	void PostMultipart(std::string_view url, CMultipartFormBuilder& form, HttpClientResponseHandler onComplete)
	{
		auto [data, len] = form.Build();
		Request("POST", url, { {"Content-Type", std::string(form.GetContentTypeHeaderValue())} },
			std::string_view(data, len), std::move(onComplete));
	}

	//***************************************************************************
	// @brief 캐싱된 모든 host 커넥션 풀을 닫습니다.
	//***************************************************************************
	void CloseAll()
	{
		if( _httpManager ) _httpManager->CloseAll();
		if( _httpsManager ) _httpsManager->CloseAll();
	}

private:
	// private 생성자 (헤더 내부 구현)
	CHttpClient(CHttpConnPoolManagerRef httpManager, CHttpConnPoolManagerRef httpsManager)
		: _httpManager(std::move(httpManager)), _httpsManager(std::move(httpsManager))
	{
	}

private:
	CHttpConnPoolManagerRef _httpManager;			// http:// 요청용
	CHttpConnPoolManagerRef _httpsManager;		// https:// 요청용
};

#endif // ndef __HTTPCLIENT_H__