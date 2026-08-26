
//***************************************************************************
// HttpFormUtil.h : URL encoding / query string / form-urlencoded body helpers.
//
//***************************************************************************

#ifndef __HTTPFORMUTIL_H__
#define __HTTPFORMUTIL_H__

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <cstdio>

//***************************************************************************
// @namespace HTTP
// @brief 쿼리스트링/폼 인코딩 유틸리티. HttpParseUtil.h의 http 네임스페이스와
//        같은 네임스페이스를 쓰지만 별개 파일이라 서로 몰라도 된다(서로 겹치는
//        함수 이름이 없어 같은 TU에서 함께 include해도 재정의 충돌 없음).
//***************************************************************************
namespace HTTP
{
	//***************************************************************************
	// @brief 문자열을 percent-encoding(RFC 3986)합니다.
	// @param input 인코딩할 원본 문자열
	// @return std::string 인코딩된 문자열
	// @details 영문자/숫자와 "-_.~"만 그대로 두고 나머지는 전부 %XX로 변환한다.
	//          공백을 '+'로 바꾸는 전통적인 application/x-www-form-urlencoded
	//          관례 대신 %20을 쓴다 — 쿼리스트링과 폼 바디에 동일한 인코딩
	//          함수를 재사용하기 위함이며(둘을 구분해서 따로 관리할 이유가
	//          없음), 현대 서버는 대부분 %20도 정상 처리한다.
	//***************************************************************************
	inline std::string UrlEncode(std::string_view input)
	{
		std::string result;
		result.reserve(input.size() * 3); // 최악의 경우(전부 인코딩) 대비 예약

		for( unsigned char c : input )
		{
			bool isUnreserved =
				(c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~';

			if( isUnreserved )
			{
				result.push_back(static_cast<char>(c));
			}
			else
			{
				char buf[4];
				std::snprintf(buf, sizeof(buf), "%%%02X", c);
				result.append(buf, 3);
			}
		}

		return result;
	}

	//***************************************************************************
	// @brief percent-encoding(%XX)된 문자열을 원래 바이트로 디코딩합니다.
	// @param input 디코딩할 문자열 (URL 경로 또는 쿼리스트링 일부)
	// @return std::string 디코딩된 문자열
	// @details '+'는 공백으로 바꾸지 않는다 — 그건 application/x-www-form-urlencoded
	//          쿼리 파라미터 값에만 해당하는 관례고, URL 경로(path) 세그먼트에서는
	//          '+'가 그냥 리터럴 '+' 문자다(RFC 3986). 잘못된 %XX 시퀀스(뒤에 hex가
	//          아닌 문자가 오는 등)는 원본 그대로 통과시킨다(엄격 실패 대신 관대하게 처리).
	//***************************************************************************
	inline std::string UrlDecode(std::string_view input)
	{
		auto hexVal = [](char c) -> int
			{
				if( c >= '0' && c <= '9' ) return c - '0';
				if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
				if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
				return -1;
			};

		std::string result;
		result.reserve(input.size());

		for( size_t i = 0; i < input.size(); ++i )
		{
			if( input[i] == '%' && i + 2 < input.size() )
			{
				int hi = hexVal(input[i + 1]);
				int lo = hexVal(input[i + 2]);
				if( hi >= 0 && lo >= 0 )
				{
					result.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			result.push_back(input[i]);
		}

		return result;
	}

	//***************************************************************************
	// @brief key=value 쌍 목록을 "key1=value1&key2=value2" 형태로 인코딩합니다.
	// @param params (key, value) 목록 — 순서대로 직렬화됨
	// @return std::string 인코딩된 쿼리스트링 (앞에 '?' 없음 — 경로에 붙일 때
	//         호출부가 직접 붙여야 함, 아래 사용 예 참고)
	// @details 키/값 각각 UrlEncode()로 인코딩한다. 결과는 CHttpRequestBuilder::
	//          SetPath()에 넘길 std::string_view의 수명 관리를 위해 호출부가
	//          소유해야 하는 owned std::string이다 — Build() 호출 시점까지
	//          살아있어야 함(제로카피 빌더의 일반 규칙과 동일).
	//
	//          [사용 예 — GET 쿼리스트링]
	//          std::string qs = http::BuildQueryString({{"id","42"},{"q","c++ tls"}});
	//          std::string path = "/search?" + qs;   // path도 Build() 시점까지 살아있어야 함
	//          CHttpRequestBuilder req;
	//          req.SetMethod("GET").SetPath(path);
	//          auto [data, len] = req.Build();
	//***************************************************************************
	inline std::string BuildQueryString(const std::vector<std::pair<std::string, std::string>>& params)
	{
		std::string result;
		bool first = true;
		for( const auto& [key, value] : params )
		{
			if( !first ) result.push_back('&');
			first = false;
			result += UrlEncode(key);
			result.push_back('=');
			result += UrlEncode(value);
		}
		return result;
	}

	//***************************************************************************
	// @brief key=value 쌍 목록을 application/x-www-form-urlencoded 요청 본문으로 인코딩합니다.
	// @param params (key, value) 목록
	// @return std::string 인코딩된 폼 바디. BuildQueryString()과 인코딩 결과 자체는
	//         동일하다(별도 함수로 나눈 이유는 "폼 바디"라는 의도를 코드에서
	//         명시적으로 드러내기 위함).
	// @details [사용 예 — 폼 POST]
	//          std::string body = http::BuildFormUrlEncodedBody({{"username","alice"},{"password","p@ss"}});
	//          CHttpRequestBuilder req;
	//          req.SetMethod("POST").SetPath("/login")
	//             .AddHeader("Content-Type", "application/x-www-form-urlencoded")
	//             .SetBody(body); // body는 Build() 호출 시점까지 살아있어야 함
	//          auto [data, len] = req.Build();
	//***************************************************************************
	inline std::string BuildFormUrlEncodedBody(const std::vector<std::pair<std::string, std::string>>& params)
	{
		return BuildQueryString(params);
	}
}

#endif // ndef __HTTPFORMUTIL_H__