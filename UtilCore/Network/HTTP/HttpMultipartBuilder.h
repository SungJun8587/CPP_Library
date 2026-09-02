
//***************************************************************************
// HttpMultipartBuilder.h : interface for the CMultipartFormBuilder class.
//
//***************************************************************************

#ifndef UC_HTTPMULTIPARTBUILDER_H
#define UC_HTTPMULTIPARTBUILDER_H

#include <string>
#include <string_view>
#include <vector>
#include <random>
#include <cstdio>

//***************************************************************************
// @class CMultipartFormBuilder
// @brief multipart/form-data 요청 본문 빌더 (파일 업로드 + 일반 폼 필드 혼합)
//
// @details
//      CHttpBuilderBase(HttpPacketBuilder.h)와 같은 설계 원칙을 따른다 —
//      내부 버퍼 std::vector<char> 하나로 통일, AddField/AddFile은 그 자리에서
//      바로 버퍼에 append(제로카피 뷰 입력 — 호출부가 Build() 시점까지 데이터를
//      살려둬야 함). 특히 AddFile()의 fileData는 그대로 복사 없이 참조만 하다가
//      Build() 때 한 번에 버퍼로 옮겨지므로, 큰 파일이라도 이중 복사가 없다.
//
//      [경계(boundary) 문자열] 생성자에서 무작위 16바이트를 hex로 인코딩해
//      만든다 — 요청 본문 안에 우연히 경계 문자열과 같은 바이트열이 등장할
//      확률을 실질적으로 0으로 만들기 위함(RFC 7578 권장 관례).
//
//      [필드/파일 이름 이스케이프] Content-Disposition 헤더의 name/filename
//      값에서 큰따옴표(")만 백슬래시로 이스케이프한다(RFC 7578 기준 최소한의
//      처리). 파일명에 non-ASCII 문자가 들어가는 경우의 RFC 2231/5987 확장
//      인코딩은 지원하지 않는다 — 필요해지면 별도로 추가해야 한다.
//
//      [사용 예]
//      CMultipartFormBuilder form;
//      form.AddField("user_id", "42")
//          .AddFile("avatar", "photo.jpg", "image/jpeg", fileBytes);
//      auto [data, len] = form.Build();
//
//      CHttpRequestBuilder req;
//      req.SetMethod("POST").SetPath("/upload")
//         .AddHeader("Content-Type", form.GetContentTypeHeaderValue())
//         .SetBody(std::string_view(data, len));
//      auto [reqData, reqLen] = req.Build();
//      // form과 req 둘 다 이 Build() 호출이 끝날 때까지 살아있어야 함
//      // (form의 버퍼를 req.SetBody()가 뷰로만 참조하기 때문)
//***************************************************************************
class CMultipartFormBuilder
{
public:
	//***************************************************************************
	// @brief CMultipartFormBuilder 생성자. 무작위 경계 문자열을 생성합니다.
	// @param reserveSize 내부 버퍼의 초기 예약 크기 (기본 4096바이트)
	//***************************************************************************
	explicit CMultipartFormBuilder(size_t reserveSize = 4096)
	{
		_buffer.reserve(reserveSize);
		_boundary = GenerateBoundary();
		_contentTypeHeaderValue = "multipart/form-data; boundary=" + _boundary;
	}

	//***************************************************************************
	// @brief 일반 폼 필드를 하나 추가합니다.
	// @param name 필드 이름
	// @param value 필드 값
	//***************************************************************************
	CMultipartFormBuilder& AddField(std::string_view name, std::string_view value)
	{
		AppendBoundaryLine();
		AppendRaw("Content-Disposition: form-data; name=\"");
		AppendEscaped(name);
		AppendRaw("\"");
		AppendCRLF();
		AppendCRLF(); // 헤더 종료 빈 줄
		AppendRaw(value);
		AppendCRLF();
		return *this;
	}

	//***************************************************************************
	// @brief 파일 하나를 추가합니다.
	// @param fieldName 폼 필드 이름
	// @param filename 파일 이름 (Content-Disposition의 filename= 값)
	// @param contentType 파일의 MIME 타입 (예: "image/jpeg", "application/octet-stream")
	// @param fileData 파일 바이트 (제로카피 뷰 — Build() 호출 시점까지 유효해야 함)
	//***************************************************************************
	CMultipartFormBuilder& AddFile(std::string_view fieldName, std::string_view filename,
		std::string_view contentType, std::string_view fileData)
	{
		AppendBoundaryLine();
		AppendRaw("Content-Disposition: form-data; name=\"");
		AppendEscaped(fieldName);
		AppendRaw("\"; filename=\"");
		AppendEscaped(filename);
		AppendRaw("\"");
		AppendCRLF();
		AppendRaw("Content-Type: ");
		AppendRaw(contentType);
		AppendCRLF();
		AppendCRLF(); // 헤더 종료 빈 줄
		AppendRaw(fileData);
		AppendCRLF();
		return *this;
	}

	//***************************************************************************
	// @brief 마지막 경계선을 마무리하고 완성된 본문 뷰를 반환합니다.
	// @return std::pair<const char*, size_t> 완성된 본문 데이터 뷰 (복사 없음)
	// @details 여러 번 호출해도 안전하도록 마무리 여부를 추적한다 — 이미
	//          마무리된 상태에서 다시 호출하면 그냥 현재 버퍼 뷰만 반환한다
	//          (닫는 경계선을 중복으로 붙이지 않음).
	//***************************************************************************
	std::pair<const char*, size_t> Build()
	{
		if( !_finalized )
		{
			AppendRaw("--");
			AppendRaw(_boundary);
			AppendRaw("--");
			AppendCRLF();
			_finalized = true;
		}
		return { _buffer.data(), _buffer.size() };
	}

	//***************************************************************************
	// @brief 이 본문에 맞는 Content-Type 헤더 값을 반환합니다 (boundary 포함).
	//***************************************************************************
	std::string_view GetContentTypeHeaderValue() const noexcept { return _contentTypeHeaderValue; }

	//***************************************************************************
	// @brief 버퍼 용량은 유지하고 내용만 초기화합니다. 경계 문자열도 새로 생성합니다
	//        (재사용 시 이전 요청의 경계와 겹치지 않도록).
	//***************************************************************************
	void Reset()
	{
		_buffer.clear();
		_finalized = false;
		_boundary = GenerateBoundary();
		_contentTypeHeaderValue = "multipart/form-data; boundary=" + _boundary;
	}

private:
	//***************************************************************************
	// @brief 무작위 16바이트를 hex로 인코딩해 경계 문자열을 생성합니다.
	// @details PRNG 엔진(std::mt19937)은 스레드마다 처음 호출될 때 한 번만
	//          std::random_device로 시드를 얻어 초기화하고, 그 뒤로는 계속
	//          재사용한다 — CMultipartFormBuilder를 요청마다 새로 만들거나
	//          Reset()을 자주 호출하는 핫패스에서, random_device 호출(대개
	//          OS 엔트로피 소스에 대한 시스템 콜)과 엔진 재시드 비용이 매번
	//          반복되는 걸 피하기 위함이다. thread_local이라 스레드 간 공유/락이
	//          필요 없다.
	//***************************************************************************
	static std::string GenerateBoundary()
	{
		thread_local std::mt19937 gen{ std::random_device{}() };
		std::uniform_int_distribution<int> dist(0, 255);

		std::string boundary = "----CppFormBoundary";
		char buf[3];
		for( int i = 0; i < 16; ++i )
		{
			std::snprintf(buf, sizeof(buf), "%02x", dist(gen));
			boundary.append(buf, 2);
		}
		return boundary;
	}

	void AppendRaw(std::string_view sv)
	{
		_buffer.insert(_buffer.end(), sv.begin(), sv.end());
	}

	void AppendCRLF()
	{
		_buffer.push_back('\r');
		_buffer.push_back('\n');
	}

	//***************************************************************************
	// @brief "--boundary\r\n" 한 줄을 버퍼에 추가합니다 (각 파트 시작 표시).
	//***************************************************************************
	void AppendBoundaryLine()
	{
		AppendRaw("--");
		AppendRaw(_boundary);
		AppendCRLF();
	}

	//***************************************************************************
	// @brief Content-Disposition의 name/filename 값에서 큰따옴표만 이스케이프해 추가합니다.
	//***************************************************************************
	void AppendEscaped(std::string_view sv)
	{
		for( char c : sv )
		{
			if( c == '"' )
				_buffer.push_back('\\');
			_buffer.push_back(c);
		}
	}

private:
	std::string _boundary;               // 무작위 경계 문자열 (each part를 구분)
	std::string _contentTypeHeaderValue; // "multipart/form-data; boundary=..." (요청 헤더에 그대로 씀)
	std::vector<char> _buffer;           // 조립 중인 본문 버퍼
	bool _finalized = false;             // Build()로 닫는 경계선까지 마무리됐는지 여부
};

#endif // ndef UC_HTTPMULTIPARTBUILDER_H