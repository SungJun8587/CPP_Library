
//***************************************************************************
// HttpPacketBuilder.h : interface for the CHttpRequestBuilder & CHttpResponseBuilder classes.
//
//***************************************************************************

#ifndef UC_HTTPPACKETBUILDER_H
#define UC_HTTPPACKETBUILDER_H

#include <BaseRedefineDataType.h>
#include <Network/NetworkRedefineDataType.h>

#include <string_view>
#include <vector>
#include <utility>
#include <charconv>
#include <cstring>

//***************************************************************************
// 내부 유틸리티 (대소문자 비교)
//***************************************************************************
namespace http
{
	//***************************************************************************
	// @brief 대소문자 무시 비교 (헤더 이름은 RFC 7230 기준 대소문자 무관, ASCII 범위만 처리).
	// @param a 비교할 문자열 A
	// @param b 비교할 문자열 B
	// @return bool 대소문자 무시 시 같으면 true
	//***************************************************************************
	inline bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept
	{
		if( a.size() != b.size() )
			return false;
		for( size_t i = 0; i < a.size(); ++i )
		{
			char ca = a[i], cb = b[i];
			if( ca >= 'A' && ca <= 'Z' ) ca += 32;
			if( cb >= 'A' && cb <= 'Z' ) cb += 32;
			if( ca != cb ) return false;
		}
		return true;
	}
}

//***************************************************************************
// @class CHttpBuilderBase
// @brief HTTP/1.1 요청·응답 패킷 빌더가 공유하는 버퍼 조립 로직 (경량, 제로카피 지향)
//
// @details
//      [char 네이티브인 이유] 이 클래스는 소켓으로 직접 나가는 와이어 바이트를
//      조립한다. HTTP 요청/응답 라인·헤더는 RFC 7230상 항상 ASCII이고, 조립된
//      결과는 CIocpSession/CRioSession의 Send(const void*, uint16_t)로 그대로
//      전달돼야 한다. TCHAR(UNICODE 빌드에서 wchar_t) 기반으로 조립하면, 결국
//      소켓에 나가기 직전에 다시 char로 재인코딩해야 하는 "낭비된 왕복"이 요청
//      하나마다(hot path) 발생한다 — 임시 버퍼 힙 할당 + wchar_t->char 순회
//      복사가 매 BeginRequest() 호출마다 생기는 셈. 이 비용을 근본적으로 없애기
//      위해 이 클래스는 처음부터 끝까지 char/std::string_view로만 동작한다
//      (int32/uint64 등 고정폭 정수 타입과 USING_SHARED_PTR 매크로만
//      BaseRedefineDataType.h에서 계속 사용).
//
//      로깅/UI 등 네트워크 밖에서 TCHAR 문자열이 필요하면, 이 클래스가 만든
//      char 버퍼를 그 경계에서만 변환해서 쓰는 쪽을 권장한다(반대로 매 요청마다
//      TCHAR로 조립했다가 다시 char로 되돌리는 것보다 훨씬 저렴함).
//
//      설계 원칙:
//      - 내부 버퍼 std::vector<char> 하나로 통일, reserve() 후 insert(memcpy)만
//        사용. ostringstream / sprintf / string 연결 없음.
//      - key/value/body는 std::string_view로 받는 제로카피 입력. Build() 호출
//        시점까지만 유효하면 됨(임시 std::string을 즉시 넘기면 댕글링 위험).
//      - 헤더 컨테이너는 map이 아닌 flat vector<pair>(헤더 개수 규모에서
//        선형 스캔이 더 빠르고 삽입 순서도 보존됨).
//      - Reset()으로 버퍼 용량 유지한 채 재사용, CopyTo()로 외부 송신 버퍼에
//        직접 복사(CSendBuffer 등과 연동).
//      - 정수->문자열 변환은 std::to_chars(char 전용, 할당 없음)를 그대로 사용.
//***************************************************************************
class CHttpBuilderBase
{
public:
	//***************************************************************************
	// @brief 완성된 패킷 데이터 뷰를 반환합니다 (복사 없음).
	//***************************************************************************
	std::pair<const char*, size_t> GetData() const noexcept
	{
		return { m_buffer.data(), m_buffer.size() };
	}

	//***************************************************************************
	// @brief 외부 소유 버퍼로 직접 복사합니다.
	// @param dst 복사 대상 버퍼
	// @param capacity dst의 용량 (바이트 기준)
	// @return size_t 복사한 바이트 수. capacity 부족 시 0.
	//***************************************************************************
	size_t CopyTo(char* dst, size_t capacity) const noexcept
	{
		if( capacity < m_buffer.size() )
			return 0;
		std::memcpy(dst, m_buffer.data(), m_buffer.size());
		return m_buffer.size();
	}

	//***************************************************************************
	// @brief 버퍼 용량은 유지하고 내용만 초기화합니다 (인스턴스 재사용용).
	//***************************************************************************
	void Reset()
	{
		m_buffer.clear();
		m_headers.clear();
		m_body = {};
		m_hasContentLength = false;
	}

protected:
	//***************************************************************************
	// @brief CHttpBuilderBase 생성자
	// @param reserveSize 내부 버퍼/헤더 목록의 초기 예약 크기
	//***************************************************************************
	explicit CHttpBuilderBase(size_t reserveSize)
	{
		m_buffer.reserve(reserveSize);
		m_headers.reserve(8);
	}

	//***************************************************************************
	// @brief 문자열을 내부 버퍼 끝에 그대로 추가합니다 (memcpy 기반, 복사 1회).
	//***************************************************************************
	void AppendRaw(std::string_view sv)
	{
		m_buffer.insert(m_buffer.end(), sv.begin(), sv.end());
	}

	//***************************************************************************
	// @brief CRLF(\r\n)를 내부 버퍼 끝에 추가합니다.
	//***************************************************************************
	void AppendCRLF()
	{
		m_buffer.push_back('\r');
		m_buffer.push_back('\n');
	}

	//***************************************************************************
	// @brief 헤더 한 줄("Key: Value\r\n")을 내부 버퍼 끝에 추가합니다.
	//***************************************************************************
	void AppendHeaderLine(std::string_view key, std::string_view value)
	{
		AppendRaw(key);
		AppendRaw(": ");
		AppendRaw(value);
		AppendCRLF();
	}

	//***************************************************************************
	// @brief m_headers에 쌓인 헤더 전부를 삽입 순서대로 내부 버퍼에 추가합니다.
	//***************************************************************************
	void AppendAllHeaders()
	{
		for( const auto& [k, v] : m_headers )
			AppendHeaderLine(k, v);
	}

	//***************************************************************************
	// @brief body가 있고 사용자가 Content-Length를 직접 넣지 않았다면 자동 계산해 추가합니다.
	//***************************************************************************
	void AppendContentLengthIfNeeded()
	{
		if( !m_body.empty() && !m_hasContentLength )
		{
			char numBuf[20];
			auto res = std::to_chars(numBuf, numBuf + sizeof(numBuf), m_body.size());
			AppendHeaderLine("Content-Length", std::string_view(numBuf, res.ptr - numBuf));
		}
	}

	//***************************************************************************
	// @brief 헤더를 m_headers에 추가합니다. key가 "Content-Length"면 사용자가
	//        직접 지정했다는 플래그(m_hasContentLength)를 세워 자동 계산을 막습니다.
	//***************************************************************************
	void AddHeaderImpl(std::string_view key, std::string_view value)
	{
		if( http::EqualsIgnoreCase(key, "Content-Length") )
			m_hasContentLength = true;
		m_headers.emplace_back(key, value);
	}

protected:
	std::vector<char> m_buffer;                                        // 조립 중인 패킷 버퍼 (reserve() 후 append만, 재할당 최소화)
	std::vector<std::pair<std::string_view, std::string_view>> m_headers; // 헤더 목록 (flat vector, 삽입 순서 보존)
	std::string_view m_body;                                           // 요청/응답 본문 (제로카피 뷰, Build() 시점까지만 유효해야 함)
	bool m_hasContentLength = false;                                   // 사용자가 Content-Length 헤더를 직접 추가했는지 여부
};

//***************************************************************************
// @class CHttpRequestBuilder
// @brief HTTP/1.1 요청 패킷 빌더 (요청 라인 + 헤더 + body)
//***************************************************************************
class CHttpRequestBuilder final : public CHttpBuilderBase
{
public:
	//***************************************************************************
	// @brief CHttpRequestBuilder 생성자
	// @param reserveSize 내부 버퍼의 초기 예약 크기 (기본 512바이트)
	//***************************************************************************
	explicit CHttpRequestBuilder(size_t reserveSize = 512)
		: CHttpBuilderBase(reserveSize) {
	}

	//***************************************************************************
	// @brief HTTP 메서드를 설정합니다 (예: "GET", "POST"). 기본값 "GET".
	//***************************************************************************
	CHttpRequestBuilder& SetMethod(std::string_view method) { m_method = method; return *this; }

	//***************************************************************************
	// @brief 요청 경로를 설정합니다 (예: "/api/users"). 기본값 "/".
	//***************************************************************************
	CHttpRequestBuilder& SetPath(std::string_view path) { m_path = path; return *this; }

	//***************************************************************************
	// @brief HTTP 버전 문자열을 설정합니다. 기본값 "HTTP/1.1".
	//***************************************************************************
	CHttpRequestBuilder& SetVersion(std::string_view version) { m_version = version; return *this; }

	//***************************************************************************
	// @brief 헤더를 추가합니다. "Content-Length"를 직접 추가하면 자동 계산을 막습니다.
	//***************************************************************************
	CHttpRequestBuilder& AddHeader(std::string_view key, std::string_view value)
	{
		AddHeaderImpl(key, value);
		return *this;
	}

	//***************************************************************************
	// @brief 요청 본문을 설정합니다 (제로카피 뷰, Build() 호출 시점까지 유효해야 함).
	//***************************************************************************
	CHttpRequestBuilder& SetBody(std::string_view body)
	{
		m_body = body;
		return *this;
	}

	//***************************************************************************
	// @brief 요청 라인 + 헤더 + 빈 줄 + body 순으로 조립합니다.
	// @return std::pair<const char*, size_t> 내부 버퍼 뷰(GetData()와 동일)
	//***************************************************************************
	std::pair<const char*, size_t> Build()
	{
		m_buffer.clear();
		AppendRaw(m_method);
		AppendRaw(" ");
		AppendRaw(m_path);
		AppendRaw(" ");
		AppendRaw(m_version);
		AppendCRLF();

		AppendAllHeaders();
		AppendContentLengthIfNeeded();
		AppendCRLF(); // 헤더 종료 빈 줄

		if( !m_body.empty() )
			AppendRaw(m_body);

		return GetData();
	}

private:
	std::string_view m_method{ "GET" };        // HTTP 메서드 (기본값 GET)
	std::string_view m_path{ "/" };            // 요청 경로 (기본값 "/")
	std::string_view m_version{ "HTTP/1.1" };  // HTTP 버전 문자열
};

//***************************************************************************
// @class CHttpResponseBuilder
// @brief HTTP/1.1 응답 패킷 빌더 (상태 라인 + 헤더 + body)
//***************************************************************************
class CHttpResponseBuilder final : public CHttpBuilderBase
{
public:
	//***************************************************************************
	// @brief CHttpResponseBuilder 생성자
	// @param reserveSize 내부 버퍼의 초기 예약 크기 (기본 512바이트)
	//***************************************************************************
	explicit CHttpResponseBuilder(size_t reserveSize = 512)
		: CHttpBuilderBase(reserveSize) {
	}

	//***************************************************************************
	// @brief HTTP 버전 문자열을 설정합니다. 기본값 "HTTP/1.1".
	//***************************************************************************
	CHttpResponseBuilder& SetVersion(std::string_view version) { m_version = version; return *this; }

	//***************************************************************************
	// @brief 상태 코드와 상태 메시지(Reason-Phrase)를 설정합니다. 기본값 200/"OK".
	//***************************************************************************
	CHttpResponseBuilder& SetStatus(int32 code, std::string_view reason)
	{
		m_statusCode = code;
		m_reason = reason;
		return *this;
	}

	//***************************************************************************
	// @brief 헤더를 추가합니다. "Content-Length"를 직접 추가하면 자동 계산을 막습니다.
	//***************************************************************************
	CHttpResponseBuilder& AddHeader(std::string_view key, std::string_view value)
	{
		AddHeaderImpl(key, value);
		return *this;
	}

	//***************************************************************************
	// @brief 응답 본문을 설정합니다 (제로카피 뷰, Build() 호출 시점까지 유효해야 함).
	//***************************************************************************
	CHttpResponseBuilder& SetBody(std::string_view body)
	{
		m_body = body;
		return *this;
	}

	//***************************************************************************
	// @brief 상태 라인 + 헤더 + 빈 줄 + body 순으로 조립합니다.
	// @return std::pair<const char*, size_t> 내부 버퍼 뷰(GetData()와 동일)
	//***************************************************************************
	std::pair<const char*, size_t> Build()
	{
		m_buffer.clear();
		AppendRaw(m_version);
		AppendRaw(" ");

		char numBuf[8];
		auto res = std::to_chars(numBuf, numBuf + sizeof(numBuf), m_statusCode);
		AppendRaw(std::string_view(numBuf, res.ptr - numBuf));

		AppendRaw(" ");
		AppendRaw(m_reason);
		AppendCRLF();

		AppendAllHeaders();
		AppendContentLengthIfNeeded();
		AppendCRLF();

		if( !m_body.empty() )
			AppendRaw(m_body);

		return GetData();
	}

private:
	std::string_view m_version{ "HTTP/1.1" }; // HTTP 버전 문자열
	int32 m_statusCode = 200;               // HTTP 상태 코드 (기본값 200)
	std::string_view m_reason{ "OK" };        // 상태 메시지 (Reason-Phrase)
};

#endif // ndef UC_HTTPPACKETBUILDER_H