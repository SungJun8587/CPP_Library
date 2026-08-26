
//***************************************************************************
// JsonFieldExtract.h : minimal, targeted JSON field extraction (NOT a general
// JSON parser).
//
//***************************************************************************

#ifndef __JSONFIELDEXTRACT_H__
#define __JSONFIELDEXTRACT_H__

#include <string>
#include <string_view>
#include <charconv>

//***************************************************************************
// @namespace json_extract
// @brief 최상위 평면 JSON 객체(중첩 없음, GCP 서비스 계정 키 파일이나 OAuth2
//        토큰 응답처럼 구조가 고정된 것)에서 특정 key의 문자열/숫자 값만
//        뽑아낸다. 일반 JSON 파서가 아니다 — 중첩 객체/배열/유니코드 이스케이프
//        (\uXXXX)는 지원하지 않는다. \n, \", \\ 이스케이프만 처리한다(서비스
//        계정 JSON의 private_key 필드가 PEM 안의 개행을 \n으로 이스케이프해서
//        담고 있으므로 이것만은 반드시 필요).
//***************************************************************************
namespace json_extract
{
	//***************************************************************************
	// @brief JSON 문자열 값 안의 \n, \", \\ 이스케이프를 해제합니다 (내부용).
	// @param raw 이스케이프 해제 전 원본 문자열 뷰
	// @return std::string 이스케이프가 해제된 문자열
	//***************************************************************************
	inline std::string Unescape(std::string_view raw)
	{
		std::string result;
		result.reserve(raw.size());
		for( size_t i = 0; i < raw.size(); ++i )
		{
			if( raw[i] == '\\' && i + 1 < raw.size() )
			{
				char next = raw[i + 1];
				if( next == 'n' ) { result.push_back('\n'); ++i; continue; }
				if( next == 't' ) { result.push_back('\t'); ++i; continue; }
				if( next == '"' ) { result.push_back('"'); ++i; continue; }
				if( next == '\\' ) { result.push_back('\\'); ++i; continue; }
				if( next == '/' ) { result.push_back('/'); ++i; continue; }
				// 그 외(\uXXXX 등)는 지원 범위 밖 — 백슬래시를 그대로 둔다.
			}
			result.push_back(raw[i]);
		}
		return result;
	}

	//***************************************************************************
	// @brief "key":"value" 형태의 문자열 필드 값을 찾습니다.
	// @param json 검색할 JSON 텍스트 (전체 문서 아무 위치나 가능 — 중첩 무시하고
	//        "\"key\":" 패턴만 텍스트 검색하므로, 여러 계층에 같은 key 이름이
	//        있으면 첫 번째로 발견되는 것을 반환한다는 한계가 있다)
	// @param key 찾을 필드 이름
	// @param out [OUT] 찾은 값 (이스케이프 해제됨)
	// @return bool 찾았는지 여부
	//***************************************************************************
	inline bool FindString(std::string_view json, std::string_view key, std::string& out)
	{
		std::string pattern;
		pattern.reserve(key.size() + 3);
		pattern.push_back('"');
		pattern.append(key);
		pattern.append("\":");

		size_t pos = json.find(pattern);
		if( pos == std::string_view::npos )
			return false;

		pos += pattern.size();
		while( pos < json.size() && (json[pos] == ' ' || json[pos] == '\t') ) ++pos;
		if( pos >= json.size() || json[pos] != '"' )
			return false;
		++pos; // 여는 따옴표 건너뜀

		size_t start = pos;
		while( pos < json.size() )
		{
			if( json[pos] == '\\' ) { pos += 2; continue; } // 이스케이프된 문자는 건너뜀(닫는 따옴표 오인 방지)
			if( json[pos] == '"' ) break;
			++pos;
		}
		if( pos >= json.size() )
			return false;

		out = Unescape(json.substr(start, pos - start));
		return true;
	}

	//***************************************************************************
	// @brief "key":숫자 형태의 숫자 필드 값을 찾습니다 (정수만 지원, 실수/지수 표기 미지원).
	// @param json 검색할 JSON 텍스트
	// @param key 찾을 필드 이름
	// @param out [OUT] 찾은 값
	// @return bool 찾았는지 여부
	//***************************************************************************
	inline bool FindInt(std::string_view json, std::string_view key, int64_t& out)
	{
		std::string pattern;
		pattern.reserve(key.size() + 3);
		pattern.push_back('"');
		pattern.append(key);
		pattern.append("\":");

		size_t pos = json.find(pattern);
		if( pos == std::string_view::npos )
			return false;

		pos += pattern.size();
		while( pos < json.size() && (json[pos] == ' ' || json[pos] == '\t') ) ++pos;

		size_t start = pos;
		while( pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '-') ) ++pos;
		if( pos == start )
			return false;

		auto res = std::from_chars(json.data() + start, json.data() + pos, out);
		return res.ec == std::errc();
	}
}

#endif // ndef __JSONFIELDEXTRACT_H__