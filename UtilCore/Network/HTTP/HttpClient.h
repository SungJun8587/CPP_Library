
//***************************************************************************
// HttpClient.h : interface for the CHttpClient class.
//
//***************************************************************************

#ifndef UC_HTTPCLIENT_H
#define UC_HTTPCLIENT_H

#include <Network/HTTP/HttpParseUtil.h>
#include <Network/HTTP/HttpUrl.h>
#include <Network/HTTP/HttpConnPoolFactory.h>
#include <Network/HTTP/HttpPacketBuilder.h>
#include <Network/HTTP/HttpFormUtil.h>
#include <Network/HTTP/HttpMultipartBuilder.h>

#include <functional>
#include <string>
#include <string_view>
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
	bool timedOut = false;     // 요청이 타임아웃(무응답 시간 초과)으로 실패했는지 (success가 false일 때만 의미 있음)
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
	static CHttpClientRef CreateIocp(CIocpCoreRef iocpCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerIocp<SessionType>(iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		CHttpConnPoolManagerRef httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerIocp<SessionType>(sslCtx, iocpCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return CHttpClientRef(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	//***************************************************************************
	// @brief RIO 엔진 기반 클라이언트를 만듭니다. 파라미터는 CreateIocp()와 대응.
	// @tparam SessionType 사용할 세션 클래스 (기본값 CHttpSessionRio)
	//***************************************************************************
	template<typename SessionType = CHttpSessionRio>
	static CHttpClientRef CreateRio(CRioCoreRef rioCore, SSL_CTX* sslCtx = nullptr,
		int32 minIdlePerHost = 2, int32 maxConnectionsPerHost = 8, uint32 workerThreadCountPerHost = 1)
	{
		auto httpManager = CreateHttpConnPoolManagerRio<SessionType>(rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);
		CHttpConnPoolManagerRef httpsManager;
		if( sslCtx != nullptr )
			httpsManager = CreateHttpsConnPoolManagerRio<SessionType>(sslCtx, rioCore, minIdlePerHost, maxConnectionsPerHost, workerThreadCountPerHost);

		return CHttpClientRef(new CHttpClient(std::move(httpManager), std::move(httpsManager)));
	}

	// 각 멤버 함수의 상세 설명은 HttpClient.cpp 참고.

	// 임의의 메서드/헤더/본문으로 요청을 보낸다(저수준 진입점 — Get/PostJson/PostForm/PostMultipart는 이 함수의 얇은 래퍼).
	// URL 파싱 실패, 메서드/헤더의 제어 문자, HTTPS 미지원 구성 등으로 요청을 만들 수 없으면 onComplete를 즉시 success=false로 호출한다.
	void Request(std::string_view method, std::string_view url,
		const std::vector<std::pair<std::string, std::string>>& headers,
		std::string_view body, HttpClientResponseHandler onComplete);

	// 요청별 타임아웃을 지정하는 Request() 오버로드. timeout 0이면 기본값, 음수면 이 요청은 타임아웃 없음.
	void Request(std::string_view method, std::string_view url,
		const std::vector<std::pair<std::string, std::string>>& headers,
		std::string_view body, std::chrono::milliseconds timeout, HttpClientResponseHandler onComplete);

	// GET 요청 (커스텀 헤더 오버로드 포함).
	void Get(std::string_view url, HttpClientResponseHandler onComplete);
	void Get(std::string_view url, const std::vector<std::pair<std::string, std::string>>& headers, HttpClientResponseHandler onComplete);

	// 이미 직렬화된 JSON 본문으로 POST.
	void PostJson(std::string_view url, std::string_view json, HttpClientResponseHandler onComplete);

	// application/x-www-form-urlencoded 본문으로 POST (fields는 내부에서 인코딩된다).
	void PostForm(std::string_view url, const std::vector<std::pair<std::string, std::string>>& fields, HttpClientResponseHandler onComplete);

	// multipart/form-data(파일 업로드 포함) 본문으로 POST. form은 이 함수가 Build()로 완성한다.
	void PostMultipart(std::string_view url, CMultipartFormBuilder& form, HttpClientResponseHandler onComplete);

	// 모든 호스트 풀의 기본 요청 타임아웃을 설정한다(0 이하면 타임아웃 없음, 미설정 시 30초).
	void SetDefaultTimeout(std::chrono::milliseconds timeout);

	// 호스트 주소 재해석 주기를 설정한다(기본 60초, 0 이하면 하지 않음).
	void SetDnsRefreshInterval(std::chrono::milliseconds interval);

	// 캐싱된 모든 host 커넥션 풀을 닫는다. 종료 시퀀스는 아래 WaitUntilAllSessionsClosed() 참고.
	void CloseAll();

	// 평문/HTTPS 매니저를 통틀어 현재 활성 세션 수(캐시값).
	size_t GetTotalActiveSessionCount() const;

	// 양쪽 매니저의 세션 정리가 끝날 때까지(또는 timeout까지) 블로킹 대기한다.
	// 권장 종료 시퀀스: CloseAll() -> WaitUntilAllSessionsClosed() -> 코어 종료(PostQuit) -> 워커 join.
	void WaitUntilAllSessionsClosed();
	bool WaitUntilAllSessionsClosed(std::chrono::milliseconds timeout);

	// 양쪽 매니저의 세션이 모두 정리되는 순간 한 번 호출되는 콜백을 등록한다(이미 0이면 즉시 호출).
	void SetAllSessionsClosedHandler(std::function<void()> handler);

private:
	// 메서드와 사용자 헤더가 요청 분할(헤더/요청 주입)로 이어질 수 있는 문자를 담고 있지 않은지 검사한다.
	static bool IsValidRequestHead(std::string_view method, const std::vector<std::pair<std::string, std::string>>& headers);

	CHttpClient(CHttpConnPoolManagerRef httpManager, CHttpConnPoolManagerRef httpsManager);

private:
	CHttpConnPoolManagerRef _httpManager;		// http:// 요청용 (항상 유효)
	CHttpConnPoolManagerRef _httpsManager;	// https:// 요청용 (sslCtx 없이 생성했으면 nullptr)
};

#endif // ndef UC_HTTPCLIENT_H