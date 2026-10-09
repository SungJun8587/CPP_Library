
//***************************************************************************
// HttpParseUtil.h : shared utility functions and types for HTTP parser classes.
//
//***************************************************************************

#ifndef UC_HTTPPARSEUTIL_H
#define UC_HTTPPARSEUTIL_H

#include <Util/PercentCodec.h>

#include <charconv>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

//***************************************************************************
// @namespace HTTP
// @brief CHttpResponseParser/CHttpRequestParser가 공유하는 파싱 상태/상한/유틸리티.
//        두 파서는 start-line만 다를 뿐 헤더/본문 프레이밍(RFC 7230 §3.3)은 같으므로,
//        줄 읽기·헤더 추가·청크 처리·본문 길이 판정을 여기서 한 번만 구현한다.
//***************************************************************************
namespace HTTP
{
	//***************************************************************************
	// @enum EParseState
	// @brief CHttpRequestParser/CHttpResponseParser 공용 파싱 진행 상태
	// @details StartLine은 파서 종류에 따라 요청 라인("METHOD URI VERSION") 또는
	//          상태 라인("VERSION CODE REASON")을 뜻한다(RFC 7230 §3.1의
	//          start-line = request-line / status-line). 어느 쪽인지는 이 enum이
	//          아니라 호출하는 파서 클래스가 결정한다.
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

	// 파서 공용 상한 (비정상 메시지/공격으로 인한 메모리 고갈 방지)
	inline constexpr size_t kMaxLineLen = 8192;                  // 줄 하나(시작 줄/헤더/청크 크기 줄)의 최대 길이
	inline constexpr size_t kMaxHeaderCount = 100;               // 메시지 하나에 허용하는 헤더(트레일러 포함) 개수
	inline constexpr size_t kMaxBodyLen = 64 * 1024 * 1024;      // 본문 누적 크기 상한(64MB)
	inline constexpr size_t kMaxBodyReserve = 1024 * 1024;       // Content-Length만 믿고 미리 잡는 버퍼의 상한(1MB) — 나머지는 append로 자란다

	using HeaderList = std::vector<std::pair<std::string, std::string>>;

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
	// @brief 공백(0x20)을 포함한 제어 문자(0x00~0x20, 0x7F)가 하나라도 있는지 확인합니다.
	// @details URL/메서드/헤더 이름처럼 공백이 허용되지 않는 값에 CR/LF가 섞여 요청 라인이나 헤더를
	//          끊고 임의의 헤더를 끼워 넣는 것(요청 분할)을 막는 입력 검사용이다.
	//***************************************************************************
	inline bool ContainsControlOrSpace(std::string_view sv) noexcept
	{
		for( unsigned char c : sv )
			if( c <= 0x20 || c == 0x7F )
				return true;
		return false;
	}

	//***************************************************************************
	// @brief 헤더 값에 허용되지 않는 제어 문자(탭을 제외한 0x00~0x1F, 0x7F)가 있는지 확인합니다.
	//***************************************************************************
	inline bool ContainsControlChar(std::string_view sv) noexcept
	{
		for( unsigned char c : sv )
			if( (c < 0x20 && c != '\t') || c == 0x7F )
				return true;
		return false;
	}

	//***************************************************************************
	// @brief 쉼표로 구분된 토큰 목록("keep-alive, Upgrade")에 token이 있는지 대소문자 무시로 확인합니다.
	//***************************************************************************
	inline bool ContainsToken(std::string_view list, std::string_view token) noexcept
	{
		while( !list.empty() )
		{
			const size_t comma = list.find(',');
			const std::string_view item = Trim(comma == std::string_view::npos ? list : list.substr(0, comma));
			if( EqualsIgnoreCaseAscii(item, token) )
				return true;
			if( comma == std::string_view::npos )
				break;
			list.remove_prefix(comma + 1);
		}
		return false;
	}

	//***************************************************************************
	// @brief 헤더 목록에서 이름이 같은 첫 헤더의 값을 반환합니다(없으면 빈 뷰).
	//***************************************************************************
	inline std::string_view FindHeaderValue(const HeaderList& headers, std::string_view name) noexcept
	{
		for( const auto& [k, v] : headers )
			if( EqualsIgnoreCaseAscii(k, name) )
				return v;
		return {};
	}

	//***************************************************************************
	// @brief 이름이 같은 모든 헤더(같은 헤더가 여러 줄로 온 경우 포함)의 값에 token이 있는지 확인합니다.
	//***************************************************************************
	inline bool HeaderHasToken(const HeaderList& headers, std::string_view name, std::string_view token) noexcept
	{
		for( const auto& [k, v] : headers )
			if( EqualsIgnoreCaseAscii(k, name) && ContainsToken(v, token) )
				return true;
		return false;
	}

	//***************************************************************************
	// @enum ELineResult
	// @brief ReadLine()의 결과
	//***************************************************************************
	enum class ELineResult
	{
		NeedMore, // 줄 끝('\n')을 아직 못 만났다 — data는 전부 lineBuffer에 누적됨
		Line,     // 한 줄이 완성됐다 — lineBuffer에 개행(CRLF/LF)이 제거된 줄이 들어있다
		TooLong   // 줄이 kMaxLineLen을 넘었다 — 프로토콜 오류로 처리
	};

	//***************************************************************************
	// @brief data에서 개행('\n')까지를 lineBuffer에 이어 붙여 한 줄을 완성합니다.
	// @param lineBuffer 이전 호출에서 누적된 부분 줄 (Line을 반환하면 호출부가 처리한 뒤 비워야 함)
	// @param data 입력 바이트
	// @param len data의 길이
	// @param consumed [OUT] 소비한 바이트 수 (NeedMore/TooLong이면 len 전체 또는 줄 끝까지)
	// @details 줄 끝의 '\r'은 제거한다. 요청/응답 파서의 시작 줄/헤더/청크 크기 줄이 모두 이 함수를 쓴다.
	//***************************************************************************
	inline ELineResult ReadLine(std::string& lineBuffer, const char* data, size_t len, size_t& consumed)
	{
		const char* nl = static_cast<const char*>(std::memchr(data, '\n', len));
		if( nl == nullptr )
		{
			lineBuffer.append(data, len);
			consumed = len;
			return lineBuffer.size() > kMaxLineLen ? ELineResult::TooLong : ELineResult::NeedMore;
		}

		consumed = static_cast<size_t>(nl - data) + 1;
		lineBuffer.append(data, consumed - 1); // '\n' 직전까지
		if( !lineBuffer.empty() && lineBuffer.back() == '\r' )
			lineBuffer.pop_back();

		return lineBuffer.size() > kMaxLineLen ? ELineResult::TooLong : ELineResult::Line;
	}

	//***************************************************************************
	// @brief 헤더 한 줄("Key: Value")을 파싱해 headers에 추가합니다.
	// @return false면 메시지를 거부해야 하는 줄이다 — 헤더 개수 초과, 빈 이름, 이름 앞/뒤의 공백
	//         ("Transfer-Encoding : chunked"처럼 이름이 달리 해석돼 본문 경계 판정이 어긋나는
	//         요청 스머글링 수법, RFC 7230 §3.2.4). 콜론이 없는 줄은 무시하고 true를 반환한다.
	//***************************************************************************
	inline bool AddHeaderLine(HeaderList& headers, std::string_view line)
	{
		const size_t colon = line.find(':');
		if( colon == std::string_view::npos )
			return true;
		if( colon == 0 || line[0] == ' ' || line[0] == '\t' || line[colon - 1] == ' ' || line[colon - 1] == '\t' )
			return false;
		if( headers.size() >= kMaxHeaderCount )
			return false;

		headers.emplace_back(std::string(line.substr(0, colon)), std::string(Trim(line.substr(colon + 1))));
		return true;
	}

	//***************************************************************************
	// @brief 10진수만으로 이뤄진 문자열을 size_t로 엄격하게 파싱합니다(부호/공백/꼬리 문자 불허, 오버플로 실패).
	//***************************************************************************
	inline bool ParseUnsigned(std::string_view sv, size_t& out) noexcept
	{
		if( sv.empty() )
			return false;
		const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), out, 10);
		return res.ec == std::errc() && res.ptr == sv.data() + sv.size();
	}

	//***************************************************************************
	// @brief chunked 인코딩의 청크 크기 줄("1A;ext=1")에서 크기(16진수)를 파싱합니다.
	// @details chunk-extension(';' 이후)은 무시한다. 16진수 이외의 문자나 오버플로는 실패.
	//***************************************************************************
	inline bool ParseChunkSizeLine(std::string_view line, size_t& outSize) noexcept
	{
		const size_t semi = line.find(';');
		if( semi != std::string_view::npos )
			line = line.substr(0, semi);
		line = Trim(line);
		if( line.empty() )
			return false;

		const auto res = std::from_chars(line.data(), line.data() + line.size(), outSize, 16);
		return res.ec == std::errc() && res.ptr == line.data() + line.size();
	}

	//***************************************************************************
	// @brief 청크 데이터 뒤의 CRLF 두 바이트를 검증하며 소비합니다(부분 수신 대비 1바이트씩).
	// @param progress 지금까지 소비한 바이트 수(0~2) — 2가 되면 호출부가 0으로 되돌리고 다음 상태로 넘어간다
	// @param error [OUT] 기대한 '\r'/'\n'이 아니면 true (청크 경계가 어긋난 메시지)
	// @return 소비한 바이트 수
	//***************************************************************************
	inline size_t ConsumeChunkCrlf(const char* data, size_t len, int& progress, bool& error) noexcept
	{
		size_t consumed = 0;
		while( consumed < len && progress < 2 )
		{
			const char expected = (progress == 0) ? '\r' : '\n';
			if( data[consumed] != expected )
			{
				error = true;
				return consumed;
			}
			++consumed;
			++progress;
		}
		return consumed;
	}

	//***************************************************************************
	// @enum EBodyFraming
	// @brief 헤더 섹션만으로 판정한 본문 길이 규칙 (RFC 7230 §3.3.3)
	//***************************************************************************
	enum class EBodyFraming
	{
		None,           // 본문 길이를 알리는 헤더가 없다
		ContentLength,  // Content-Length로 길이가 정해진다
		Chunked,        // Transfer-Encoding의 마지막 코딩이 chunked
		UnknownCoding,  // Transfer-Encoding이 있지만 마지막 코딩이 chunked가 아니다 (요청이면 거부, 응답이면 연결 종료까지)
		Invalid         // 잘못된 Content-Length, 서로 다른 Content-Length 중복, (옵션) TE+CL 동시 존재
	};

	//***************************************************************************
	// @struct SBodyFraming
	// @brief ResolveBodyFraming()의 결과
	//***************************************************************************
	struct SBodyFraming
	{
		EBodyFraming kind = EBodyFraming::None;
		size_t contentLength = 0; // kind==ContentLength일 때만 의미 있음
	};

	//***************************************************************************
	// @brief 헤더 목록에서 Transfer-Encoding/Content-Length를 엄격하게 해석해 본문 길이 규칙을 정합니다.
	// @param headers 파싱된 헤더 목록
	// @param rejectBothTeAndCl true면 두 헤더가 함께 오는 메시지를 Invalid로 본다(요청 — RFC 7230 §3.3.3은
	//        이를 오류로 다루길 권한다). false면 Transfer-Encoding이 우선한다(응답).
	// @details 잘못된 Content-Length("abc", "5abc", "-1")를 "헤더 없음"으로 간주하면 본문이 다음 메시지로
	//          해석돼 메시지 경계가 어긋나므로 반드시 Invalid로 돌려 연결을 폐기하게 한다. 같은 헤더가 여러
	//          줄이거나 쉼표 목록("5, 5")이어도 값이 모두 같을 때만 유효하다.
	//***************************************************************************
	inline SBodyFraming ResolveBodyFraming(const HeaderList& headers, bool rejectBothTeAndCl) noexcept
	{
		bool hasTe = false;
		std::string_view lastTe;
		bool hasCl = false;
		size_t contentLength = 0;

		for( const auto& [k, v] : headers )
		{
			if( EqualsIgnoreCaseAscii(k, "Transfer-Encoding") )
			{
				hasTe = true;
				lastTe = v;
			}
			else if( EqualsIgnoreCaseAscii(k, "Content-Length") )
			{
				std::string_view list = v;
				for( ;; )
				{
					const size_t comma = list.find(',');
					const std::string_view token = Trim(comma == std::string_view::npos ? list : list.substr(0, comma));

					size_t n = 0;
					if( !ParseUnsigned(token, n) )
						return { EBodyFraming::Invalid, 0 };
					if( hasCl && n != contentLength )
						return { EBodyFraming::Invalid, 0 };
					hasCl = true;
					contentLength = n;

					if( comma == std::string_view::npos )
						break;
					list.remove_prefix(comma + 1);
				}
			}
		}

		if( hasTe )
		{
			if( hasCl && rejectBothTeAndCl )
				return { EBodyFraming::Invalid, 0 };

			const size_t lastComma = lastTe.rfind(',');
			const std::string_view finalCoding = Trim(lastComma == std::string_view::npos ? lastTe : lastTe.substr(lastComma + 1));
			return { EqualsIgnoreCaseAscii(finalCoding, "chunked") ? EBodyFraming::Chunked : EBodyFraming::UnknownCoding, 0 };
		}

		if( hasCl )
			return { EBodyFraming::ContentLength, contentLength };
		return { EBodyFraming::None, 0 };
	}

	//***************************************************************************
	// @brief RFC 3986 퍼센트 인코딩("%XX")을 원래 바이트로 디코딩합니다.
	// @details MultipartFormParser.h(RFC 5987 filename*=), FormUrlEncodedParser.h
	//          (application/x-www-form-urlencoded), HttpFormUtil.h(UrlDecode)가
	//          공통으로 쓴다. 결과는 원본 바이트 그대로이며(문자셋 변환 없음), "%" 뒤에
	//          유효한 16진수 두 글자가 없으면 그 "%"는 원본 그대로 둔다(관대한 처리).
	//          실제 규칙은 WebUtil(UrlDecode 등)과 공유하는 Util/PercentCodec.h에 있다 —
	//          두 곳의 디코딩 규칙이 서로 어긋나지 않게 하려는 것이다.
	//***************************************************************************
	inline std::string PercentDecode(std::string_view encoded)
	{
		return PercentCodec::Decode(encoded, false);
	}
}

#endif // ndef UC_HTTPPARSEUTIL_H