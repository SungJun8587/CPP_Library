
//***************************************************************************
// HttpParseUtil.h : shared utility functions and types for HTTP parser classes.
//
//***************************************************************************

#ifndef UC_HTTPPARSEUTIL_H
#define UC_HTTPPARSEUTIL_H

#include <string>
#include <string_view>

//***************************************************************************
// @namespace HTTP
// @brief CHttpResponseParser/CHttpRequestParser가 공유하는 파싱 상태/유틸리티.
//***************************************************************************
namespace HTTP
{
	//***************************************************************************
	// @enum EParseState
	// @brief CHttpRequestParser/CHttpResponseParser 공용 파싱 진행 상태
	// @details RFC 7230 §3.1의 문법 정의(start-line = request-line / status-line)를
	//          그대로 반영해, 요청/응답 파서가 원래 거의 동일했던 두 개의 enum
	//          (EHttpRequestParseState의 RequestLine, EHttpParseState의
	//          StatusLine — 그 외 8개 값은 완전히 동일)을 하나로 합쳤다.
	//          StartLine이 파서 종류에 따라 요청 라인("METHOD URI VERSION")
	//          또는 상태 라인("VERSION CODE REASON") 중 무엇을 의미하는지는
	//          호출하는 파서 클래스(CHttpRequestParser/CHttpResponseParser)가
	//          결정한다 — 이 enum 자체는 어느 쪽인지 구분하지 않는다.
	//***************************************************************************
	enum class EParseState
	{
		StartLine,      // 요청 라인 또는 상태 라인("GET /x HTTP/1.1" 또는 "HTTP/1.1 200 OK") 파싱 중
		Headers,        // 헤더 라인들 파싱 중
		Body,           // Content-Length 기준 body 수신 중
		ChunkedSize,    // chunked: 다음 청크 크기 라인 대기
		ChunkedData,    // chunked: 청크 데이터 수신 중
		ChunkedCRLF,    // chunked: 청크 데이터 뒤 CRLF 소비
		ChunkedTrailer, // chunked: 마지막 0-size 청크 뒤 trailer 헤더(있으면) 소비
		Complete,       // 메시지 하나 완전히 파싱됨
		Error           // 프로토콜 위반 등으로 더 이상 진행 불가 (커넥션 폐기 대상)
	};

	//***************************************************************************
	// @brief 대소문자 무시 비교 (헤더 이름/버전 문자열 등은 RFC 7230 기준 대소문자
	//        무관인 경우가 많음, ASCII 범위만 처리).
	//***************************************************************************
	inline bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept
	{
		if( a.size() != b.size() ) return false;
		for( size_t i = 0; i < a.size(); ++i )
		{
			char ca = a[i], cb = b[i];
			if( ca >= 'A' && ca <= 'Z' ) ca += 32;
			if( cb >= 'A' && cb <= 'Z' ) cb += 32;
			if( ca != cb ) return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief 문자열 앞뒤의 공백(스페이스/탭)을 제거합니다.
	//***************************************************************************
	inline std::string_view Trim(std::string_view sv) noexcept
	{
		while( !sv.empty() && (sv.front() == ' ' || sv.front() == '\t') ) sv.remove_prefix(1);
		while( !sv.empty() && (sv.back() == ' ' || sv.back() == '\t') ) sv.remove_suffix(1);
		return sv;
	}

	//***************************************************************************
	// @brief RFC 3986 퍼센트 인코딩("%XX")을 원래 바이트로 디코딩합니다.
	// @details MultipartFormParser.h(RFC 5987 filename*=)와
	//          FormUrlEncodedParser.h(application/x-www-form-urlencoded)가
	//          공통으로 쓰는 유틸이라 여기(공용 파싱 유틸리티)로 옮겼다.
	//          결과는 원본 바이트 그대로(문자셋이 UTF-8이라는 전제하에 별도
	//          변환 없이 그대로 둠). "%" 뒤에 유효한 16진수 두 글자가 없으면
	//          그 "%"는 원본 그대로 둔다(관용적 처리 — 엄격한 RFC 위반 검출이
	//          목적이 아니므로).
	//***************************************************************************
	inline std::string PercentDecode(std::string_view encoded)
	{
		auto hexDigit = [](char c) -> int
			{
				if( c >= '0' && c <= '9' ) return c - '0';
				if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
				if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
				return -1;
			};

		std::string result;
		result.reserve(encoded.size());

		for( size_t i = 0; i < encoded.size(); ++i )
		{
			if( encoded[i] == '%' && i + 2 < encoded.size() )
			{
				const int hi = hexDigit(encoded[i + 1]);
				const int lo = hexDigit(encoded[i + 2]);
				if( hi >= 0 && lo >= 0 )
				{
					result.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			result.push_back(encoded[i]);
		}

		return result;
	}
}

#endif // ndef UC_HTTPPARSEUTIL_H