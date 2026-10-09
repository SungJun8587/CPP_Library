
//***************************************************************************
// HttpUrl.h : interface for the CHttpUrl struct and ParseHttpUrl() function.
//
//***************************************************************************

#ifndef UC_HTTPURL_H
#define UC_HTTPURL_H

#include <Network/HTTP/HttpParseUtil.h>
#include <Util/UrlParser.h>

#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>

//***************************************************************************
// @struct CHttpUrl
// @brief ParseHttpUrl()이 채우는 URL 분해 결과 (scheme/host/port/path+query)
//***************************************************************************
struct CHttpUrl
{
	bool isHttps = false;             // scheme이 "https"였는지 여부
	std::string host;                 // 호스트 이름 (IP 문자열일 수도 있음, DNS resolve는 안 함)
	uint16 port = 80;               // 포트 (명시 안 되면 scheme 기본값: http=80, https=443)
	std::string pathAndQuery = "/";   // 경로 + 쿼리스트링 ('/' 포함, fragment(#...)는 제외)
};

//***************************************************************************
// @brief "http://host[:port]/path?query" 형태의 URL을 분해합니다.
// @param url 파싱할 URL 문자열
// @param out [OUT] 분해 결과
// @return bool 파싱 성공 여부. scheme이 http/https가 아니거나 host가 비어있으면 false.
// @details 이 함수는 순수 문자열 파싱만 하고 DNS resolve나 네트워크 I/O는 전혀
//          하지 않는다 — 그래서 타이머/스레드 없이 단위 테스트 가능하다.
//          URL에 공백/제어 문자(CR/LF 포함)가 있으면 거부한다 — 요청 라인에 그대로 실려 요청이
//          분할(추가 헤더/요청 주입)되는 것을 막기 위함이다. 경로의 공백 등은 호출부가 미리
//          percent-encoding(HTTP::UrlEncode())해야 한다. 경로 없이 쿼리만 오는 형태
//          ("http://host?a=b")는 경로 "/"에 쿼리를 붙여 해석한다.
//          userinfo(user:pass@host, RFC 3986)는 지원하지 않는다 — 지원 필요시
//          별도로 추가해야 한다. fragment(#...)는 인식해서 잘라내지만 out에
//          담지 않는다(HTTP 요청에 fragment를 보내지 않는 것이 스펙에 맞음).
//***************************************************************************
inline bool ParseHttpUrl(std::string_view url, CHttpUrl& out)
{
	// 분해 자체는 StringUtil의 ParseURL과 공유하는 Util/UrlParser.h가 한다.
	// 공백/제어 문자는 거부한다(rejectControlOrSpace 기본값 true) — 요청 라인 주입 방지.
	UrlParser::ParsedUrl parsed;
	if( !UrlParser::Parse(url, parsed) || !parsed.hasScheme )
		return false;

	if( parsed.IsScheme("https") )
		out.isHttps = true;
	else if( parsed.IsScheme("http") )
		out.isHttps = false;
	else
		return false;

	// userinfo와 IPv6 리터럴은 인식은 하되 이 함수에서는 받아들이지 않는다.
	// IPv6는 Host 헤더에 대괄호가 필요한데(HttpClient가 host를 그대로 이어붙여 만듦)
	// 그 처리가 아직 없다 — 지원하려면 parsed.hostIsIpv6를 CHttpUrl에 싣고
	// HttpClient의 Host 헤더 조립에서 "[host]"로 감싸야 한다.
	if( parsed.hasUserinfo || parsed.hostIsIpv6 )
		return false;

	out.host = std::string(parsed.host);
	out.port = static_cast<uint16>(parsed.EffectivePort());
	out.pathAndQuery = parsed.RequestTarget();	// 경로가 비면 "/", fragment는 제외

	return true;
}

#endif // ndef UC_HTTPURL_H