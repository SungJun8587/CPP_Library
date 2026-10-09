
//***************************************************************************
// UrlParser.h : URL 분해(scheme/userinfo/host/port/path/query/fragment) 코어 (header-only).
//
// @details
// RFC 3986의 "scheme://[userinfo@]host[:port][/path][?query][#fragment]" 형태를
// 문자열 복사 없이 분해합니다. 결과의 string_view는 모두 "입력 문자열"을 가리키므로
// 입력이 살아있는 동안만 유효합니다.
//  - <string>/<string_view>만 사용 — 플랫폼(Windows/TCHAR) 의존이 없어 서버·클라이언트·
//    테스트가 그대로 공유합니다. 순수 문자열 파싱만 하고 DNS resolve/네트워크 I/O는 없습니다.
//  - 두 호출부의 공용 코어입니다: StringUtil의 ParseURL(범용, 모든 scheme)과
//    Network/HTTP/HttpUrl.h의 ParseHttpUrl(http/https 전용).
//  - IPv6 리터럴("[::1]:8080")과 userinfo("user:pw@host")를 인식합니다. 다만 이를 받아들일지
//    여부는 호출부가 정합니다(예: ParseHttpUrl은 Host 헤더 조립 규칙 때문에 아직 거부).
//***************************************************************************

#ifndef UC_URLPARSER_H
#define UC_URLPARSER_H

#include <Util/Regular.h>

#include <string>
#include <string_view>

namespace UrlParser
{
	//***************************************************************************
	// @struct ParsedUrl
	// @brief Parse()가 채우는 분해 결과. 모든 string_view는 입력 문자열의 일부.
	//***************************************************************************
	struct ParsedUrl
	{
		bool				hasScheme = false;	// "scheme://"이 명시되었는지 (false면 scheme은 빈 값)
		std::string_view	scheme;				// 원문 그대로(대소문자 보존). 비교는 IsScheme() 사용
		bool				hasUserinfo = false;
		std::string_view	userinfo;			// '@' 앞부분 ("user:password" 등, 디코딩 안 함)
		std::string_view	host;				// IPv6 리터럴이면 대괄호를 뗀 주소("::1")
		bool				hostIsIpv6 = false;
		int					port = 0;			// 명시된 포트(1~65535). 명시 안 되면 0
		std::string_view	path;				// '/' 포함, 없으면 빈 값
		bool				hasQuery = false;
		std::string_view	query;				// '?' 뒤 ~ '#' 앞 ('?' 제외)
		bool				hasFragment = false;
		std::string_view	fragment;			// '#' 뒤 ('#' 제외)
		std::string_view	rest;				// authority 뒤 전체 ("/path?query#fragment", 없으면 빈 값)

		// scheme이 name과 같은지 ASCII 대소문자 무시로 비교
		bool IsScheme(std::string_view name) const noexcept
		{
			if( scheme.size() != name.size() ) return false;
			for( size_t i = 0; i < name.size(); ++i )
				if( Lower(scheme[i]) != Lower(name[i]) ) return false;
			return true;
		}

		// 포트가 명시되지 않았을 때 scheme의 기본 포트(모르는 scheme이면 0)
		int DefaultPort() const noexcept
		{
			if( IsScheme("http") || IsScheme("ws") ) return 80;
			if( IsScheme("https") || IsScheme("wss") ) return 443;
			if( IsScheme("ftp") ) return 21;
			return 0;
		}

		// 명시된 포트, 없으면 scheme 기본 포트
		int EffectivePort() const noexcept { return port > 0 ? port : DefaultPort(); }

		//***************************************************************************
		// @brief HTTP 요청 라인에 쓰는 request-target(path + ?query, fragment 제외).
		//        경로가 비어 있으면 "/"를 쓴다("http://host?a=b" -> "/?a=b").
		//***************************************************************************
		std::string RequestTarget() const
		{
			std::string target = path.empty() ? std::string("/") : std::string(path);
			if( hasQuery )
			{
				target.push_back('?');
				target.append(query);
			}
			return target;
		}

	private:
		static char Lower(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
	};

	namespace detail
	{
		using AsciiChar::IsAlpha;
		using AsciiChar::IsDigit;

		// scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
		inline bool IsValidScheme(std::string_view s) noexcept
		{
			if( s.empty() || !IsAlpha(s[0]) ) return false;
			for( char c : s )
				if( !(IsAlpha(c) || IsDigit(c) || c == '+' || c == '-' || c == '.') ) return false;
			return true;
		}

		// reg-name/IPv4: 영숫자와 "-._~" 및 퍼센트 인코딩('%')만 허용
		inline bool IsValidRegName(std::string_view s) noexcept
		{
			if( s.empty() ) return false;
			for( char c : s )
				if( !(IsAlpha(c) || IsDigit(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == '%') ) return false;
			return true;
		}

		// IPv6 리터럴 내부: 16진수/':'/'.'(IPv4 접미사) 및 zone id("%eth0")에 쓰는 문자
		inline bool IsValidIpv6(std::string_view s) noexcept
		{
			bool hasColon = false;
			for( char c : s )
			{
				if( c == ':' ) hasColon = true;
				else if( !(IsAlpha(c) || IsDigit(c) || c == '.' || c == '%' || c == '-' || c == '_' || c == '~') ) return false;
			}
			return hasColon;
		}

		// 숫자만 허용하고 1~65535 범위를 확인. 빈 문자열/비숫자/범위 밖이면 false.
		inline bool ParsePort(std::string_view s, int& out) noexcept
		{
			if( s.empty() ) return false;
			long long v = 0;
			for( char c : s )
			{
				if( !IsDigit(c) ) return false;
				v = v * 10 + (c - '0');
				if( v > 65535 ) return false;
			}
			if( v <= 0 ) return false;
			out = static_cast<int>(v);
			return true;
		}
	}

	//***************************************************************************
	// @brief URL을 분해합니다.
	// @param url 파싱할 문자열
	// @param out [OUT] 분해 결과 (실패 시 내용은 불확정)
	// @param rejectControlOrSpace true(기본)면 공백/제어문자(0x00~0x20, 0x7F)가 하나라도
	//        있을 때 실패. 요청 라인에 그대로 실려 요청이 분할되는 주입을 막으려는 용도이며,
	//        HTTP에 쓰는 호출부는 반드시 true로 쓸 것. 범용 용도에서 경로의 공백 등을 허용하려면 false.
	// @return 성공 여부. host가 비었거나 문자가 잘못됐거나 포트가 숫자가 아니거나(빈 값 포함)
	//         1~65535 밖이거나, "scheme://"의 scheme이 문법에 맞지 않으면 false.
	// @details scheme이 없는 입력("example.com:8080/x")도 허용한다(hasScheme=false, 호출부가
	//          기본 scheme을 정함). "://"가 경로/쿼리 안에 있는 경우("host/p?u=http://x")는
	//          scheme 구분자로 오인하지 않는다.
	//***************************************************************************
	inline bool Parse(std::string_view url, ParsedUrl& out, bool rejectControlOrSpace = true)
	{
		out = ParsedUrl{};
		if( url.empty() ) return false;

		if( rejectControlOrSpace )
		{
			for( unsigned char c : url )
				if( c <= 0x20 || c == 0x7F ) return false;
		}

		// 1) scheme
		std::string_view afterScheme = url;
		const size_t sep = url.find("://");
		if( sep != std::string_view::npos )
		{
			const std::string_view candidate = url.substr(0, sep);
			if( candidate.find_first_of("/?#") == std::string_view::npos )
			{
				if( !detail::IsValidScheme(candidate) ) return false;
				out.hasScheme = true;
				out.scheme = candidate;
				afterScheme = url.substr(sep + 3);
			}
			// '/', '?', '#'가 먼저 나오면 scheme 없는 입력 — "://"는 경로/쿼리의 일부
		}

		// 2) authority / rest
		const size_t authorityEnd = afterScheme.find_first_of("/?#");
		const std::string_view authority = (authorityEnd == std::string_view::npos) ? afterScheme : afterScheme.substr(0, authorityEnd);
		out.rest = (authorityEnd == std::string_view::npos) ? std::string_view() : afterScheme.substr(authorityEnd);

		// 3) userinfo
		std::string_view hostPort = authority;
		const size_t at = authority.rfind('@');
		if( at != std::string_view::npos )
		{
			out.hasUserinfo = true;
			out.userinfo = authority.substr(0, at);
			hostPort = authority.substr(at + 1);
		}

		// 4) host[:port]
		std::string_view portSv;
		bool hasPortDelimiter = false;
		if( !hostPort.empty() && hostPort[0] == '[' )
		{
			const size_t close = hostPort.find(']');
			if( close == std::string_view::npos ) return false;

			out.host = hostPort.substr(1, close - 1);
			if( !detail::IsValidIpv6(out.host) ) return false;
			out.hostIsIpv6 = true;

			const std::string_view tail = hostPort.substr(close + 1);
			if( !tail.empty() )
			{
				if( tail[0] != ':' ) return false;
				hasPortDelimiter = true;
				portSv = tail.substr(1);
			}
		}
		else
		{
			const size_t colon = hostPort.rfind(':');
			if( colon == std::string_view::npos )
			{
				out.host = hostPort;
			}
			else
			{
				out.host = hostPort.substr(0, colon);
				hasPortDelimiter = true;
				portSv = hostPort.substr(colon + 1);
			}
			if( !detail::IsValidRegName(out.host) ) return false;
		}

		if( hasPortDelimiter && !detail::ParsePort(portSv, out.port) ) return false;

		// 5) path / query / fragment
		std::string_view r = out.rest;
		const size_t hashPos = r.find('#');
		if( hashPos != std::string_view::npos )
		{
			out.hasFragment = true;
			out.fragment = r.substr(hashPos + 1);
			r = r.substr(0, hashPos);
		}
		const size_t qPos = r.find('?');
		if( qPos != std::string_view::npos )
		{
			out.hasQuery = true;
			out.query = r.substr(qPos + 1);
			r = r.substr(0, qPos);
		}
		out.path = r;

		return true;
	}
}

#endif // ndef UC_URLPARSER_H