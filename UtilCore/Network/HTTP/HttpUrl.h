
//***************************************************************************
// HttpUrl.h : interface for the CHttpUrl struct and ParseHttpUrl() function.
//
//***************************************************************************

#ifndef UC_HTTPURL_H
#define UC_HTTPURL_H

#include <Network/HTTP/HttpParseUtil.h>

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
	uint16_t port = 80;               // 포트 (명시 안 되면 scheme 기본값: http=80, https=443)
	std::string pathAndQuery = "/";   // 경로 + 쿼리스트링 ('/' 포함, fragment(#...)는 제외)
};

//***************************************************************************
// @brief "http://host[:port]/path?query" 형태의 URL을 분해합니다.
// @param url 파싱할 URL 문자열
// @param out [OUT] 분해 결과
// @return bool 파싱 성공 여부. scheme이 http/https가 아니거나 host가 비어있으면 false.
// @details 이 함수는 순수 문자열 파싱만 하고 DNS resolve나 네트워크 I/O는 전혀
//          하지 않는다 — 그래서 타이머/스레드 없이 단위 테스트 가능하다.
//          userinfo(user:pass@host, RFC 3986)는 지원하지 않는다 — 지원 필요시
//          별도로 추가해야 한다. fragment(#...)는 인식해서 잘라내지만 out에
//          담지 않는다(HTTP 요청에 fragment를 보내지 않는 것이 스펙에 맞음).
//***************************************************************************
inline bool ParseHttpUrl(std::string_view url, CHttpUrl& out)
{
	size_t schemeEnd = url.find("://");
	if (schemeEnd == std::string_view::npos)
		return false;

	std::string_view scheme = url.substr(0, schemeEnd);
	if (HTTP::EqualsIgnoreCaseAscii(scheme, "https"))
	{
		out.isHttps = true;
		out.port = 443;
	}
	else if (HTTP::EqualsIgnoreCaseAscii(scheme, "http"))
	{
		out.isHttps = false;
		out.port = 80;
	}
	else
	{
		return false;
	}

	std::string_view rest = url.substr(schemeEnd + 3);

	// fragment(#...) 제거 — HTTP 요청에는 포함시키지 않는다.
	size_t fragmentStart = rest.find('#');
	if (fragmentStart != std::string_view::npos)
		rest = rest.substr(0, fragmentStart);

	size_t pathStart = rest.find('/');
	std::string_view hostPort = (pathStart == std::string_view::npos) ? rest : rest.substr(0, pathStart);

	if (pathStart == std::string_view::npos)
		out.pathAndQuery = "/";
	else
		out.pathAndQuery = std::string(rest.substr(pathStart));

	size_t colon = hostPort.find(':');
	if (colon != std::string_view::npos)
	{
		out.host = std::string(hostPort.substr(0, colon));

		std::string_view portSv = hostPort.substr(colon + 1);
		int portValue = 0;
		auto res = std::from_chars(portSv.data(), portSv.data() + portSv.size(), portValue);
		if (res.ec != std::errc() || portValue <= 0 || portValue > 65535)
			return false;
		out.port = static_cast<uint16_t>(portValue);
	}
	else
	{
		out.host = std::string(hostPort);
	}

	return !out.host.empty();
}

#endif // ndef UC_HTTPURL_H
