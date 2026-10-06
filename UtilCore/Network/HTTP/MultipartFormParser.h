
//***************************************************************************
// MultipartFormParser.h : multipart/form-data 본문 파싱 유틸리티.
//
// [설계] CHttpRequestParser는 body를 그대로(GetBody()) 돌려줄 뿐 그 내용을
// 해석하지 않는다 — Content-Type이 뭐든 상관없이 body는 body이기 때문이다.
// multipart/form-data는 그 body의 여러 인코딩 중 하나일 뿐이라, 별도 파일로
// 분리해서 여기서만 다룬다. HttpParseUtil.h(HTTP::EqualsIgnoreCaseAscii/
// HTTP::Trim/HTTP::PercentDecode)를 그대로 재사용해 파서들과 동일한 "char 전용"
// 원칙을 따른다. 완성된 body를 한 번에 스캔하므로 짧은 필드에 적합하고,
// 대용량 업로드는 MultipartStreamParser.h의 스트리밍 파서를 쓴다.
//***************************************************************************

#ifndef UC_MULTIPARTFORMPARSER_H
#define UC_MULTIPARTFORMPARSER_H

#include <Network/HTTP/HttpParseUtil.h>

#include <string>
#include <string_view>
#include <vector>

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

	namespace detail
	{
		//***************************************************************************
		// @brief 헤더 파라미터 값의 바깥 큰따옴표를 벗기고 `\"`를 `"`로 되돌립니다.
		// @details 백슬래시를 나머지 문자 앞에서도 풀어 버리면 오래된 브라우저가 보내던
		//          "C:\dir\file.txt" 같은 값이 깨지므로 `\"`만 처리한다.
		//***************************************************************************
		inline std::string UnquoteParamValue(std::string_view v)
		{
			if( v.size() < 2 || v.front() != '"' || v.back() != '"' )
				return std::string(v);

			v = v.substr(1, v.size() - 2);

			std::string result;
			result.reserve(v.size());
			for( size_t i = 0; i < v.size(); ++i )
			{
				if( v[i] == '\\' && i + 1 < v.size() && v[i + 1] == '"' )
					++i;
				result.push_back(v[i]);
			}
			return result;
		}
	}

	//***************************************************************************
	// @brief Content-Disposition 헤더 값에서 name="..."/filename="..."을 추출합니다.
	// @details 헤더 값을 큰따옴표 밖의 ';'로만 나눠 파라미터를 하나씩 이름으로 비교한다 —
	//          그래서 파라미터 순서(filename이 name보다 먼저 오는 .NET HttpClient의 전송 방식
	//          등)나 값 안의 ';'(filename="a;b.txt")에 영향받지 않고, "filename" 안에 부분
	//          문자열로 들어 있는 "name"을 name으로 오인하지도 않는다.
	//
	//          RFC 5987/6266 §4.3의 filename*=<charset>'<language>'<percent-encoded> 형식은
	//          비-ASCII(한글 등) 파일명을 실어 보낼 때 쓰며, 있으면 일반 filename보다 우선한다
	//          (순서와 무관). charset 변환은 하지 않고 UTF-8 바이트를 가정해 퍼센트 디코딩만 한다.
	//***************************************************************************
	inline void ParseContentDisposition(std::string_view value, std::string& outName, std::string& outFilename)
	{
		bool haveExtendedFilename = false;

		auto handleParam = [&](std::string_view param)
			{
				param = HTTP::Trim(param);
				const size_t eq = param.find('=');
				if( eq == std::string_view::npos )
					return;

				const std::string_view key = HTTP::Trim(param.substr(0, eq));
				const std::string_view val = HTTP::Trim(param.substr(eq + 1));

				if( HTTP::EqualsIgnoreCaseAscii(key, "name") )
				{
					outName = detail::UnquoteParamValue(val);
				}
				else if( HTTP::EqualsIgnoreCaseAscii(key, "filename") )
				{
					if( !haveExtendedFilename )
						outFilename = detail::UnquoteParamValue(val);
				}
				else if( HTTP::EqualsIgnoreCaseAscii(key, "filename*") )
				{
					const std::string ext = detail::UnquoteParamValue(val);
					const size_t firstQuote = ext.find('\'');
					const size_t secondQuote = (firstQuote == std::string::npos) ? std::string::npos : ext.find('\'', firstQuote + 1);
					if( secondQuote != std::string::npos )
					{
						outFilename = PercentDecode(std::string_view(ext).substr(secondQuote + 1));
						haveExtendedFilename = true;
					}
				}
			};

		size_t start = 0;
		bool inQuote = false;
		for( size_t i = 0; i <= value.size(); ++i )
		{
			if( i == value.size() || (!inQuote && value[i] == ';') )
			{
				handleParam(value.substr(start, i - start));
				start = i + 1;
				continue;
			}

			if( inQuote && value[i] == '\\' && i + 1 < value.size() )
				++i; // 따옴표 안의 이스케이프(\")는 따옴표 상태를 바꾸지 않는다
			else if( value[i] == '"' )
				inQuote = !inQuote;
		}
	}

	//***************************************************************************
	// @brief 파트 헤더 섹션(빈 줄 이전까지)에서 Content-Disposition/Content-Type을 뽑아냅니다.
	// @details MultipartStreamParser.h도 이 함수를 쓴다.
	//***************************************************************************
	inline void ParseMultipartPartHeaders(std::string_view headerSection, std::string& outName, std::string& outFilename, std::string& outContentType)
	{
		size_t lineStart = 0;
		while( lineStart < headerSection.size() )
		{
			size_t lineEnd = headerSection.find("\r\n", lineStart);
			if( lineEnd == std::string_view::npos )
				lineEnd = headerSection.size();

			const std::string_view line = headerSection.substr(lineStart, lineEnd - lineStart);
			const size_t colon = line.find(':');
			if( colon != std::string_view::npos )
			{
				const std::string_view headerName = HTTP::Trim(line.substr(0, colon));
				const std::string_view headerValue = HTTP::Trim(line.substr(colon + 1));

				if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Disposition") )
					ParseContentDisposition(headerValue, outName, outFilename);
				else if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Type") )
					outContentType = std::string(headerValue);
			}

			lineStart = (lineEnd < headerSection.size()) ? lineEnd + 2 : lineEnd;
		}
	}

	//***************************************************************************
	// @brief multipart/form-data 본문을 boundary로 나눠 각 파트를 파싱합니다.
	// @param body CHttpRequestParser::GetBody()가 돌려준 원본 body(char 그대로,
	//        어떤 바이트가 섞여도 안전 — std::string_view라 임베디드 NUL도 처리).
	// @param boundary ExtractBoundary()로 뽑아낸 값(앞의 "--"는 이 함수가 붙임).
	// @param outFields [out] 파싱된 필드 목록(순서 보존).
	// @return 형식이 심하게 깨져 있으면 false(호출부는 400 Bad Request로 응답).
	// @details 파트 데이터의 끝은 "\r\n--boundary"로 판정한다(줄 시작에 오는 경계선만 경계로
	//          인정) — 데이터 한가운데에 "--boundary" 문자열이 우연히 들어 있어도 파트가
	//          잘리지 않는다.
	//***************************************************************************
	inline bool ParseMultipartFormData(std::string_view body, const std::string& boundary, std::vector<SMultipartField>& outFields)
	{
		const std::string delimiter = "--" + boundary;
		const std::string bodyDelimiter = "\r\n" + delimiter;

		size_t pos = body.find(delimiter);
		if( pos == std::string_view::npos )
			return false;
		pos += delimiter.size();

		for( ;; )
		{
			// 경계 직후: "--"(종료) 또는 CRLF(다음 파트)
			if( body.compare(pos, 2, "--") == 0 )
				return true;
			if( body.compare(pos, 2, "\r\n") != 0 )
				return false;
			pos += 2;

			// 파트 헤더 섹션: 빈 줄("\r\n\r\n")까지. 헤더가 하나도 없으면 바로 빈 줄이 온다.
			std::string_view headerSection;
			size_t valueStart;
			if( body.compare(pos, 2, "\r\n") == 0 )
			{
				valueStart = pos + 2;
			}
			else
			{
				const size_t headerEnd = body.find("\r\n\r\n", pos);
				if( headerEnd == std::string_view::npos )
					return false;
				headerSection = body.substr(pos, headerEnd - pos);
				valueStart = headerEnd + 4;
			}

			const size_t valueEnd = body.find(bodyDelimiter, valueStart);
			if( valueEnd == std::string_view::npos )
				return false; // 닫는 boundary를 못 찾음 — 잘린/깨진 본문

			SMultipartField field;
			ParseMultipartPartHeaders(headerSection, field.name, field.filename, field.contentType);
			field.data.assign(body.data() + valueStart, valueEnd - valueStart);
			outFields.push_back(std::move(field));

			pos = valueEnd + bodyDelimiter.size();
		}
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