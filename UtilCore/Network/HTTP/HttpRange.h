
//***************************************************************************
// HttpRange.h : "Range: bytes=..." 요청 헤더 파싱 유틸리티.
//
// [지원 범위] RFC 7233의 byte-range-spec 중 가장 흔한 두 가지 형태만
// 지원한다:
//     Range: bytes=0-499        (시작-끝, 둘 다 명시)
//     Range: bytes=500-         (시작만 명시, 끝은 "파일 끝까지")
//     Range: bytes=-500         (suffix — "파일 끝에서부터 500바이트")
// 여러 range를 콤마로 나열하는 multipart range("bytes=0-99,200-299")는
// 지원하지 않는다 — 이 경우 통째로 무시하고 전체 파일을 200 OK로
// 돌려준다(RFC 7233 권장 동작 중 하나 — "이해 못 하는 Range는 무시해도
// 됨"). 비디오 탐색(seek)/다운로드 재개가 목적인 이 서버 규모에서
// multipart range까지 지원할 실익이 낮다고 판단했다.
//***************************************************************************

#ifndef UC_HTTPRANGE_H
#define UC_HTTPRANGE_H

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <charconv>

namespace HTTP
{
	//***************************************************************************
	// @struct SByteRange
	// @brief 파싱 및 검증이 끝난, 실제 파일 크기 기준으로 확정된 바이트 범위.
	// @details start/end는 둘 다 inclusive(포함) — HTTP Range/Content-Range의
	//          관례를 그대로 따른다(예: 0-499는 500바이트).
	//***************************************************************************
	struct SByteRange
	{
		int64_t start = 0;
		int64_t end = 0;	// inclusive

		int64_t Length() const { return end - start + 1; }
	};

	//***************************************************************************
	// @brief "Range: bytes=..." 헤더 값(예: "bytes=0-499")을 파싱해서,
	//        totalSize 기준으로 유효한 실제 범위로 확정한다.
	// @param rangeHeaderValue Range 헤더의 값 부분(헤더 이름 제외). 빈
	//        문자열이면(=Range 헤더 자체가 없음) std::nullopt를 돌려준다 —
	//        호출부는 이 경우 "일반 200 OK로 전체를 보내라"로 해석하면 된다.
	// @param totalSize 실제 파일의 전체 바이트 수 — 이 값을 넘는 범위나
	//        음수/역전된 범위는 전부 유효하지 않은 것으로 처리한다.
	// @return 파싱 및 검증에 성공하면 확정된 SByteRange. 다음 중 하나라도
	//         해당하면 std::nullopt를 돌려준다 — 헤더가 없거나(길이 0),
	//         "bytes=" 접두사가 없거나, 형식이 잘못됐거나, multipart(콤마
	//         포함)거나, totalSize==0이거나, 범위가 [0, totalSize-1]을
	//         벗어나거나 역전됐거나(start > end)의 경우. 호출부(FileDownloadHandler)는
	//         nullopt를 "이 Range 요청은 처리 못 하니 무시하고 전체 200
	//         OK로 응답"으로 다루면 되고, 별도로 416을 내려주고 싶다면
	//         "Range 헤더는 있었는데 nullopt였는지"를 별도로 체크해야 한다
	//         (이 함수 반환값만으로는 "헤더 없음"과 "헤더는 있는데 무효"를
	//         구분 못 한다 — 필요하면 호출부에서 rangeHeaderValue.empty()로
	//         먼저 구분할 것).
	//***************************************************************************
	inline std::optional<SByteRange> ParseRange(std::string_view rangeHeaderValue, int64_t totalSize)
	{
		if( rangeHeaderValue.empty() || totalSize <= 0 )
			return std::nullopt;

		constexpr std::string_view kPrefix = "bytes=";
		if( rangeHeaderValue.size() <= kPrefix.size()
			|| rangeHeaderValue.compare(0, kPrefix.size(), kPrefix) != 0 )
		{
			return std::nullopt;
		}

		std::string_view spec = rangeHeaderValue.substr(kPrefix.size());

		// multipart range("0-99,200-299") — 콤마가 있으면 지원 범위 밖.
		if( spec.find(',') != std::string_view::npos )
			return std::nullopt;

		const size_t dashPos = spec.find('-');
		if( dashPos == std::string_view::npos )
			return std::nullopt;

		std::string_view startPart = spec.substr(0, dashPos);
		std::string_view endPart = spec.substr(dashPos + 1);

		int64_t start = 0;
		int64_t end = 0;

		if( startPart.empty() )
		{
			// suffix range: "bytes=-500" → 파일 끝에서부터 500바이트.
			if( endPart.empty() )
				return std::nullopt; // "bytes=-" 같은 완전히 빈 형태

			int64_t suffixLen = 0;
			auto result = std::from_chars(endPart.data(), endPart.data() + endPart.size(), suffixLen);
			if( result.ec != std::errc() || suffixLen <= 0 )
				return std::nullopt;

			// 요청한 suffix 길이가 파일 전체보다 크면 파일 전체로 잘라낸다
			// (RFC 7233 권장 동작).
			start = (suffixLen >= totalSize) ? 0 : (totalSize - suffixLen);
			end = totalSize - 1;
		}
		else
		{
			auto startResult = std::from_chars(startPart.data(), startPart.data() + startPart.size(), start);
			if( startResult.ec != std::errc() || start < 0 )
				return std::nullopt;

			if( endPart.empty() )
			{
				// "bytes=500-" → 500부터 파일 끝까지.
				end = totalSize - 1;
			}
			else
			{
				auto endResult = std::from_chars(endPart.data(), endPart.data() + endPart.size(), end);
				if( endResult.ec != std::errc() )
					return std::nullopt;

				// 요청한 end가 파일 크기를 넘으면 파일 끝으로 잘라낸다
				// (RFC 7233 권장 동작 — 416 대신 클램프).
				if( end >= totalSize )
					end = totalSize - 1;
			}
		}

		if( start > end || start < 0 || end >= totalSize )
			return std::nullopt;

		return SByteRange{ start, end };
	}
}

#endif // ndef UC_HTTPRANGE_H