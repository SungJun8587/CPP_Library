
//***************************************************************************
// HttpFormUtil.h : URL encoding / query string / form-urlencoded body helpers.
//
//***************************************************************************

#ifndef UC_HTTPFORMUTIL_H
#define UC_HTTPFORMUTIL_H

#include <Network/HTTP/HttpParseUtil.h>
#include <Util/PercentCodec.h>

#include <string>
#include <string_view>
#include <vector>
#include <utility>

//***************************************************************************
// @namespace HTTP
// @brief 쿼리스트링/폼 인코딩 유틸리티. 디코딩은 HttpParseUtil.h의 HTTP::PercentDecode()를
//        그대로 쓰고, 이 파일은 인코딩과 쿼리스트링/폼 본문 조립을 담당한다.
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
	inline void AppendUrlEncoded(std::string& result, std::string_view input);

	inline std::string UrlEncode(std::string_view input)
	{
		std::string result;
		result.reserve(input.size() + input.size() / 2); // 인코딩이 일부만 필요한 일반적인 경우를 가정, 모자라면 자란다
		AppendUrlEncoded(result, input);
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
	//          실제 구현은 HTTP::PercentDecode()이며, '+'를 공백으로 바꾸는 폼 값 디코딩은
	//          FormUrlEncodedParser.h의 DecodeFormUrlEncodedValue()가 담당한다.
	//***************************************************************************
	inline std::string UrlDecode(std::string_view input)
	{
		return PercentDecode(input);
	}

	//***************************************************************************
	// @brief 문자열을 percent-encoding해 result 끝에 덧붙입니다 (UrlEncode()와 같은 규칙, 임시 문자열 없음).
	//***************************************************************************
	inline void AppendUrlEncoded(std::string& result, std::string_view input)
	{
		// unreserved(영숫자 + "-._~")만 그대로 두는 규칙은 WebUtil과 공유하는 코어에 있다.
		PercentCodec::AppendEncoded(result, input, PercentCodec::UnreservedTable(), false);
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
	//          std::string qs = HTTP::BuildQueryString({{"id","42"},{"q","c++ tls"}});
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
			AppendUrlEncoded(result, key);
			result.push_back('=');
			AppendUrlEncoded(result, value);
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
	//          std::string body = HTTP::BuildFormUrlEncodedBody({{"username","alice"},{"password","p@ss"}});
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

#endif // ndef UC_HTTPFORMUTIL_H