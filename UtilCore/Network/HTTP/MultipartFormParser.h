
//***************************************************************************
// MultipartFormParser.h : multipart/form-data 본문 파싱 유틸리티.
//
// [설계] CHttpRequestParser는 body를 그대로(GetBody()) 돌려줄 뿐 그 내용을
// 해석하지 않는다 — Content-Type이 뭐든 상관없이 body는 body이기 때문이다
// (HttpRequestParser.h 클래스 설명 참고). multipart/form-data는 그 body의
// 여러 인코딩 중 하나일 뿐이라, 별도 파일로 분리해서 여기서만 다룬다.
// HttpParseUtil.h(HTTP::EqualsIgnoreCaseAscii/HTTP::Trim)를 그대로 재사용해
// CHttpRequestParser와 동일한 "char 전용" 원칙을 따른다.
//***************************************************************************

#ifndef UC_MULTIPARTFORMPARSER_H
#define UC_MULTIPARTFORMPARSER_H

#include <Network/HTTP/HttpParseUtil.h>

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace HTTP
{
	//***************************************************************************
	// @brief multipart/form-data의 파트(필드) 하나.
	//***************************************************************************
	struct SMultipartField
	{
		std::string name;			// Content-Disposition의 name="..."
		std::string filename;		// filename="..."이 있으면(파일 필드) 그 값, 없으면 빈 문자열
		std::string contentType;	// 이 파트의 Content-Type(명시 안 됐으면 빈 문자열)
		std::string data;			// 필드 값(텍스트) 또는 파일 바이트 그대로
	};

	//***************************************************************************
	// @brief Content-Type 헤더 값에서 boundary 파라미터를 추출합니다.
	// @details 예: "multipart/form-data; boundary=----WebKitFormBoundaryXXX"
	//          -> "----WebKitFormBoundaryXXX". 값이 따옴표로 감싸인 경우
	//          (`boundary="..."`)도 처리한다.
	//***************************************************************************
	inline bool ExtractBoundary(std::string_view contentType, std::string& outBoundary)
	{
		constexpr std::string_view kKey = "boundary=";
		size_t pos = contentType.find(kKey);
		if( pos == std::string_view::npos )
			return false;

		std::string_view rest = contentType.substr(pos + kKey.size());
		if( !rest.empty() && rest.front() == '"' )
		{
			rest.remove_prefix(1);
			size_t end = rest.find('"');
			if( end != std::string_view::npos )
				rest = rest.substr(0, end);
		}
		else
		{
			size_t end = rest.find_first_of("; \t\r\n");
			if( end != std::string_view::npos )
				rest = rest.substr(0, end);
		}

		outBoundary = std::string(rest);
		return !outBoundary.empty();
	}

	//***************************************************************************
	// @brief Content-Disposition 헤더 값에서 name="..."/filename="..."을 추출합니다.
	//***************************************************************************
	//***************************************************************************
	// @brief RFC 3986 퍼센트 인코딩("%XX") 디코딩은 HttpParseUtil.h의
	//        HTTP::PercentDecode()를 그대로 쓴다(FormUrlEncodedParser.h와
	//        공유하는 공용 유틸로 옮겨감 — 이 파일엔 더 이상 자체 정의가 없다).
	//***************************************************************************

	//***************************************************************************
	// @brief Content-Disposition 헤더 값에서 name="..."/filename="..."을 추출합니다.
	// @details [추가] filename*=<charset>'<language>'<percent-encoded> 형식
	//          (RFC 5987, RFC 6266 §4.3)도 처리한다 — 최신 브라우저/클라이언트가
	//          비-ASCII(한글 등) 파일명을 실어 보낼 때 흔히 쓰는 방식이다.
	//          RFC 6266에 따라 filename*가 있으면 일반 filename보다 우선하므로,
	//          평범한 filename="..."을 먼저 채운 뒤 filename*가 있으면 그 값으로
	//          덮어쓴다. charset이 UTF-8이 아닌 경우(드묾, 예: ISO-8859-1)의
	//          코드셋 변환까지는 하지 않는다 — UTF-8 바이트를 그대로 쓴다는
	//          가정 하에 퍼센트 디코딩만 수행한다.
	//***************************************************************************
	inline void ParseContentDisposition(std::string_view value, std::string& outName, std::string& outFilename)
	{
		// [수정] 예전엔 value.find("name=\"")로 헤더 값 전체에서 무작정
		// 찾았는데, "filename=\"..." 문자열 안에 "name=\""가 우연히 부분
		// 문자열로 포함돼 있어서(fileNAME=") 파라미터 순서가
		// "filename=...; name=..."처럼 filename이 name보다 먼저 오면(실제로
		// .NET의 HttpClient가 비-ASCII 파일명에서 이렇게 보내는 걸 확인함)
		// filename 안의 값을 name으로 잘못 채가는 버그가 있었다.
		//
		// 이제 ';'로 파라미터를 하나씩 잘라서, 각 조각이 "name="/"filename="로
		// 정확히 시작하는지(부분 문자열 매치가 아니라 접두사 매치)만 확인한다
		// — 그래서 값 순서가 어떻든 안전하다.
		size_t pos = 0;
		while( pos <= value.size() )
		{
			const size_t semi = value.find(';', pos);
			std::string_view param = (semi == std::string_view::npos) ? value.substr(pos) : value.substr(pos, semi - pos);
			param = HTTP::Trim(param);

			if( param.size() > 5 && HTTP::EqualsIgnoreCaseAscii(param.substr(0, 5), "name=") )
			{
				std::string_view v = param.substr(5);
				if( v.size() >= 2 && v.front() == '"' && v.back() == '"' )
					v = v.substr(1, v.size() - 2);
				outName = std::string(v);
			}
			// "filename*="는 아래 9글자 접두사("filename=")와 안 겹친다 —
			// 9번째 글자가 '*'라 "filename="(정확히 =로 끝남)과 다르므로
			// 이 분기에 안 걸린다. RFC 5987 확장은 별도로 마지막에 처리.
			else if( param.size() > 9 && HTTP::EqualsIgnoreCaseAscii(param.substr(0, 9), "filename=") )
			{
				std::string_view v = param.substr(9);
				if( v.size() >= 2 && v.front() == '"' && v.back() == '"' )
					v = v.substr(1, v.size() - 2);
				outFilename = std::string(v);
			}

			if( semi == std::string_view::npos )
				break;
			pos = semi + 1;
		}

		// RFC 5987 확장 파라미터(filename*=UTF-8''퍼센트인코딩) — 있으면
		// 위에서 채운 일반 filename을 덮어쓴다(더 정확한 표현이므로 우선).
		size_t extFilePos = value.find("filename*=");
		if( extFilePos != std::string_view::npos )
		{
			std::string_view rest = value.substr(extFilePos + 10); // strlen("filename*=")
			const size_t firstQuote = rest.find('\'');
			if( firstQuote != std::string_view::npos )
			{
				const size_t secondQuote = rest.find('\'', firstQuote + 1);
				if( secondQuote != std::string_view::npos )
				{
					std::string_view encodedPart = rest.substr(secondQuote + 1);

					// 값 끝(다음 파라미터 구분자 ';' 또는 헤더 줄 끝)까지만 취한다.
					const size_t endPos = encodedPart.find(';');
					if( endPos != std::string_view::npos )
						encodedPart = encodedPart.substr(0, endPos);

					outFilename = PercentDecode(HTTP::Trim(encodedPart));
				}
			}
		}
	}

	//***************************************************************************
	// @brief multipart/form-data 본문을 boundary로 나눠 각 파트를 파싱합니다.
	// @param body CHttpRequestParser::GetBody()가 돌려준 원본 body(char 그대로,
	//        어떤 바이트가 섞여도 안전 — std::string_view라 임베디드 NUL도 처리).
	// @param boundary ExtractBoundary()로 뽑아낸 값(앞의 "--"는 이 함수가 붙임).
	// @param outFields [out] 파싱된 필드 목록(순서 보존).
	// @return 형식이 심하게 깨져 있으면 false(호출부는 400 Bad Request로 응답).
	//***************************************************************************
	inline bool ParseMultipartFormData(std::string_view body, const std::string& boundary, std::vector<SMultipartField>& outFields)
	{
		const std::string delimiter = "--" + boundary;

		size_t pos = body.find(delimiter);
		if( pos == std::string_view::npos )
			return false;
		pos += delimiter.size();

		while( true )
		{
			// 이 지점이 종료 마커("--")인지 먼저 확인.
			if( pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0 )
				break;

			// 파트 사이의 CRLF 스킵.
			if( pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0 )
				pos += 2;

			size_t nextBoundary = body.find(delimiter, pos);
			if( nextBoundary == std::string_view::npos )
				return false; // 닫는 boundary를 못 찾음 — 잘린/깨진 본문

			std::string_view partData = body.substr(pos, nextBoundary - pos);

			// partData 형식: "헤더줄들\r\n\r\n실제값\r\n" — 헤더/값 경계(빈 줄) 탐색.
			size_t headerEnd = partData.find("\r\n\r\n");
			if( headerEnd == std::string_view::npos )
				return false;

			std::string_view headerSection = partData.substr(0, headerEnd);
			std::string_view valueSection = partData.substr(headerEnd + 4);

			// 다음 boundary 직전에 항상 붙어있는 CRLF 하나를 값에서 떼어낸다.
			if( valueSection.size() >= 2 && valueSection.substr(valueSection.size() - 2) == "\r\n" )
				valueSection.remove_suffix(2);

			SMultipartField field;

			size_t lineStart = 0;
			while( lineStart < headerSection.size() )
			{
				size_t lineEnd = headerSection.find("\r\n", lineStart);
				if( lineEnd == std::string_view::npos )
					lineEnd = headerSection.size();

				std::string_view line = headerSection.substr(lineStart, lineEnd - lineStart);
				size_t colon = line.find(':');
				if( colon != std::string_view::npos )
				{
					std::string_view headerName = line.substr(0, colon);
					std::string_view headerValue = HTTP::Trim(line.substr(colon + 1));

					if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Disposition") )
						ParseContentDisposition(headerValue, field.name, field.filename);
					else if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Type") )
						field.contentType = std::string(headerValue);
				}

				lineStart = (lineEnd < headerSection.size()) ? lineEnd + 2 : lineEnd;
			}

			field.data = std::string(valueSection);
			outFields.push_back(std::move(field));

			pos = nextBoundary + delimiter.size();
		}

		return true;
	}

	//***************************************************************************
	// @brief 파싱된 필드 목록에서 이름으로 하나를 찾습니다.
	// @return 찾으면 그 필드의 포인터(원본 벡터를 가리킴), 없으면 nullptr.
	//***************************************************************************
	inline const SMultipartField* FindMultipartField(const std::vector<SMultipartField>& fields, std::string_view name)
	{
		for( const auto& f : fields )
			if( f.name == name )
				return &f;
		return nullptr;
	}
}

#endif // ndef UC_MULTIPARTFORMPARSER_H