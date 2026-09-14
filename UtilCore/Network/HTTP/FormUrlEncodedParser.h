
//***************************************************************************
// FormUrlEncodedParser.h : application/x-www-form-urlencoded 본문 파싱 유틸리티.
//
//***************************************************************************

#ifndef UC_FORMURLENCODEDPARSER_H
#define UC_FORMURLENCODEDPARSER_H

#include <Network/HTTP/HttpParseUtil.h>

#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace HTTP
{
	//***************************************************************************
	// @brief application/x-www-form-urlencoded 값 하나를 디코딩합니다.
	// @details 이 인코딩은 일반 퍼센트 인코딩(PercentDecode())과 규칙이 하나
	//          다르다 — 공백을 "%20"이 아니라 '+'로 인코딩하는 게 관례다
	//          (RFC 1866에서 유래한 HTML 폼의 오래된 관행, 지금도 거의 모든
	//          브라우저/클라이언트가 이렇게 보낸다). 그래서 '+' -> ' ' 치환을
	//          먼저 한 뒤 나머지를 PercentDecode()로 넘긴다 — 순서가 중요하다
	//          (퍼센트 디코딩을 먼저 하면 "%2B"로 인코딩된 진짜 '+' 문자와
	//          구분이 안 됨).
	//***************************************************************************
	inline std::string DecodeFormUrlEncodedValue(std::string_view raw)
	{
		std::string withSpaces;
		withSpaces.reserve(raw.size());

		for( char c : raw )
			withSpaces.push_back(c == '+' ? ' ' : c);

		return PercentDecode(withSpaces);
	}

	//***************************************************************************
	// @brief "key1=value1&key2=value2..." 형식의 body를 파싱합니다.
	// @details 같은 key가 여러 번 나올 수 있어(체크박스 여러 개 선택 등)
	//          std::vector로 순서를 보존한 채 전부 돌려준다 — map으로 뭉개지
	//          않는다. '='가 없는 조각(값 없는 키)은 value를 빈 문자열로 채운다.
	//***************************************************************************
	inline std::vector<std::pair<std::string, std::string>> ParseFormUrlEncoded(std::string_view body)
	{
		std::vector<std::pair<std::string, std::string>> result;

		size_t pos = 0;
		while( pos <= body.size() )
		{
			const size_t ampPos = body.find('&', pos);
			const std::string_view pair = (ampPos == std::string_view::npos)
				? body.substr(pos)
				: body.substr(pos, ampPos - pos);

			if( !pair.empty() )
			{
				const size_t eqPos = pair.find('=');
				std::string key, value;

				if( eqPos != std::string_view::npos )
				{
					key = DecodeFormUrlEncodedValue(pair.substr(0, eqPos));
					value = DecodeFormUrlEncodedValue(pair.substr(eqPos + 1));
				}
				else
				{
					key = DecodeFormUrlEncodedValue(pair);
				}

				result.emplace_back(std::move(key), std::move(value));
			}

			if( ampPos == std::string_view::npos )
				break;
			pos = ampPos + 1;
		}

		return result;
	}

	//***************************************************************************
	// @brief 파싱된 필드 목록에서 이름으로 하나를 찾습니다(첫 번째 일치만).
	// @return 찾으면 그 값의 포인터(원본 벡터를 가리킴), 없으면 nullptr.
	//***************************************************************************
	inline const std::string* FindFormValue(const std::vector<std::pair<std::string, std::string>>& fields, std::string_view key)
	{
		for( const auto& [k, v] : fields )
			if( k == key )
				return &v;
		return nullptr;
	}
}

#endif // ndef UC_FORMURLENCODEDPARSER_H