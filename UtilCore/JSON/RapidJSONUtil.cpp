
//***************************************************************************
// RapidJSONUtil.cpp: implementation of the CRapidJSONUtil class.
//
//***************************************************************************

#include "pch.h"
#include "RapidJSONUtil.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief `CRapidJSONUtil` 클래스의 기본 인스턴스를 생성합니다.
// @param 없음
// @return 없음
// @detail 내부 문서 객체 할당자를 초기화하고, 기본 문서를 빈 JSON 객체(`kObjectType`) 상태로 설정합니다.
//***************************************************************************
CRapidJSONUtil::CRapidJSONUtil() : _allocator(_document.GetAllocator())
{
	_document.SetObject();
}

//***************************************************************************
// @brief 다른 `CRapidJSONUtil` 객체의 데이터를 복사하여 새로운 인스턴스를 생성합니다. (복사 생성자)
// @param other 복사할 원본 `CRapidJSONUtil` 객체
// @return 없음
// @detail 원본 객체가 가진 내부 JSON 문서의 전체 내용을 할당자를 통해 깊은 복사(`CopyFrom`)합니다.
//***************************************************************************
CRapidJSONUtil::CRapidJSONUtil(const CRapidJSONUtil& other) : _allocator(_document.GetAllocator())
{
	_document.CopyFrom(other._document, _allocator);		// 객체 복사
}

//***************************************************************************
// @brief 디버깅 활성화 상태인 경우 포맷팅된 디버그 메시지를 표준 출력에 기록합니다.
// @param ptszFormat 출력할 문자열 포맷 (가변 인자 지원)
// @return 없음
// @detail 디버그 출력 플래그(`_bIsDebugPrint`)가 참일 때 접두사(`CRapidJSONUtil::`)와 함께 가변 인자 메시지를 `stdout`에 출력합니다.
//***************************************************************************
void CRapidJSONUtil::Print_DebugInfo(const TCHAR* ptszFormat, ...)
{
	if( !_bIsDebugPrint ) return;

	va_list args;

	_ftprintf_s(stdout, _T("CRapidJSONUtil::"));

	va_start(args, ptszFormat);
	_vtprintf_s(ptszFormat, args);
	va_end(args);

	return;
}

//***************************************************************************
// @brief 대입 연산자 오버로딩을 통해 다른 `CRapidJSONUtil` 객체의 JSON 문서를 복사합니다.
// @param other 대입할 원본 `CRapidJSONUtil` 객체
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 자기 자신과의 대입인지 검사한 후, 원본 문서의 내용을 내부 문서에 깊은 복사(`CopyFrom`)하고 자신을 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(const CRapidJSONUtil& other)
{
	if( this != &other )
	{
		_document.CopyFrom(other._document, _allocator);
	}
	return *this;  // 자신을 리턴하여 연속적인 연산 가능
}

//***************************************************************************
// @brief 문자열 포인터 값을 내부 문서의 문자열 데이터로 설정합니다.
// @param ptszValue 설정할 문자열 포인터 (`TCHAR*`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서를 문자열 타입으로 전환하고 지정한 문자열을 할당한 뒤 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(const TCHAR* ptszValue)
{
	_document.SetString(ptszValue, _allocator);
	return *this;
}

//***************************************************************************
// @brief `_tstring` 문자열 값을 내부 문서의 문자열 데이터로 설정합니다.
// @param strValue 설정할 문자열 (`_tstring`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 문자열의 길이와 내용을 바탕으로 내부 문서를 문자열 타입으로 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(const _tstring& strValue)
{
	_document.SetString(strValue.c_str(), (rapidjson::SizeType)strValue.length(), _allocator);
	return *this;
}

//***************************************************************************
// @brief 32비트 부호 있는 정수 값을 내부 문서의 정수 데이터로 설정합니다.
// @param iValue 설정할 32비트 정수 값
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 32비트 정수 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(int32 iValue)
{
	_document.SetInt(iValue);
	return *this;
}

//***************************************************************************
// @brief 64비트 부호 있는 정수 값을 내부 문서의 정수 데이터로 설정합니다.
// @param i64Value 설정할 64비트 정수 값
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 64비트 정수 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(int64 i64Value)
{
	_document.SetInt64(i64Value);
	return *this;
}

//***************************************************************************
// @brief 32비트 부호 없는 정수 값을 내부 문서의 부호 없는 정수 데이터로 설정합니다.
// @param uValue 설정할 32비트 부호 없는 정수 값
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 32비트 부호 없는 정수 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(uint32 uValue)
{
	_document.SetUint(uValue);
	return *this;
}

//***************************************************************************
// @brief 64비트 부호 없는 정수 값을 내부 문서의 부호 없는 정수 데이터로 설정합니다.
// @param u64Value 설정할 64비트 부호 없는 정수 값
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 64비트 부호 없는 정수 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(uint64 u64Value)
{
	_document.SetUint64(u64Value);
	return *this;
}

//***************************************************************************
// @brief 부동소수점 실수 값을 내부 문서의 실수 데이터로 설정합니다.
// @param dValue 설정할 실수 값 (`double`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 실수 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(double dValue)
{
	_document.SetDouble(dValue);
	return *this;
}

//***************************************************************************
// @brief 부울(참/거짓) 값을 내부 문서의 부울 데이터로 설정합니다.
// @param bValue 설정할 부울 값 (`bool`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서에 부울 값을 설정하고 객체 참조를 반환합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(bool bValue)
{
	_document.SetBool(bValue);
	return *this;
}

//***************************************************************************
// @brief 키-값 쌍(문자열 포인터 키)을 객체에 추가하여 확장합니다.
// @param keyValue 추가할 키와 `CRapidJSONUtil` 객체가 담긴 페어(`std::pair`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 객체 타입이 아닐 경우 객체로 초기화한 뒤, 전달된 서브 문서 객체를 복사하여 새로운 멤버로 추가합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator+(const std::pair<const TCHAR*, CRapidJSONUtil>& keyValue)
{
	if( !_document.IsObject() )
	{
		_document.SetObject();
	}

	_tValue key(keyValue.first, _allocator);	// Key를 rapidjson::Value로 변환
	_tValue value;

	// Value도 복사하여 추가해야 함
	value.CopyFrom(keyValue.second._document, _allocator);

	_document.AddMember(key, value, _allocator);
	return *this;
}

//***************************************************************************
// @brief 키-값 쌍(`_tstring` 키)을 객체에 추가하여 확장합니다.
// @param keyValue 추가할 키와 `CRapidJSONUtil` 객체가 담긴 페어(`std::pair`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 객체 타입이 아닐 경우 객체로 초기화한 뒤, 전달된 서브 문서 객체를 복사하여 새로운 멤버로 추가합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator+(const std::pair<_tstring, CRapidJSONUtil>& keyValue)
{
	if( !_document.IsObject() )
	{
		_document.SetObject();
	}

	_tValue key(keyValue.first.c_str(), _allocator);	// Key를 rapidjson::Value로 변환
	_tValue value;

	// Value도 복사하여 추가해야 함
	value.CopyFrom(keyValue.second._document, _allocator);

	_document.AddMember(key, value, _allocator);
	return *this;
}

//***************************************************************************
// @brief 다른 `CRapidJSONUtil` 객체를 배열 요소로 추가하여 확장합니다.
// @param other 배열에 추가할 원본 `CRapidJSONUtil` 객체
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 배열 타입이 아닐 경우 배열로 초기화한 뒤, 전달된 객체의 내용을 복사하여 배열 끝에 추가합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator+(const CRapidJSONUtil& other)
{
	if( !_document.IsArray() )
	{
		_document.SetArray();
	}
	_tValue value;
	value.CopyFrom(other._document, _allocator);	// JSON 객체를 복사하여 추가
	_document.PushBack(value, _allocator);
	return *this;
}

//***************************************************************************
// @brief 문자열 포인터에 해당하는 객체 속성을 삭제합니다.
// @param ptszKey 삭제할 JSON 멤버의 키 이름 (`TCHAR*`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 객체이고 해당 키가 존재하면, 하위 요소를 재귀적으로 정리한 뒤 문서에서 멤버를 제거합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator-(const TCHAR* ptszKey)
{
	if( _document.IsObject() && _document.HasMember(ptszKey) )
	{
		RecursiveRemove(_document[ptszKey]);
		_document.RemoveMember(ptszKey);
	}
	return *this;
}

//***************************************************************************
// @brief `_tstring` 키에 해당하는 객체 속성을 삭제합니다.
// @param key 삭제할 JSON 멤버의 키 이름 (`_tstring`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부적으로 `operator-(const TCHAR*)`를 호출하여 해당 키의 멤버를 제거합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator-(const _tstring& key)
{
	return operator-(key.c_str());
}

//***************************************************************************
// @brief 인덱스 위치에 해당하는 배열 요소를 삭제합니다.
// @param index 삭제할 배열 요소의 인덱스 위치 (`uint32`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 배열이고 유효한 인덱스 범위 내에 있다면, 해당 요소를 재귀 정리한 후 배열에서 지웁니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator-(const uint32 index)
{
	if( _document.IsArray() && index < _document.Size() )
	{
		RecursiveRemove(_document[index]);
		_document.Erase(_document.Begin() + index);
	}
	return *this;
}

//***************************************************************************
// @brief 지정한 키가 문서 내에 존재하는지 확인합니다.
// @param ptszKey 확인할 키 이름 (`TCHAR*`)
// @return 존재할 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서의 `HasMember`를 호출하여 키의 존재 여부를 반환합니다.
//***************************************************************************
bool CRapidJSONUtil::IsExists(const TCHAR* ptszKey) const
{
	return _document.HasMember(ptszKey);
}

//***************************************************************************
// @brief 지정한 키(`_tstring`)가 문서 내에 존재하는지 확인합니다.
// @param key 확인할 키 이름 (`_tstring`)
// @return 존재할 경우 true, 그렇지 않으면 false
// @detail `_tstring`의 c_str 포인터를 이용해 멤버 존재 여부를 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsExists(const _tstring& key) const
{
	return IsExists(key.c_str());
}

//***************************************************************************
// @brief 현재 문서가 JSON 객체 타입인지 확인합니다.
// @param 없음
// @return 객체 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서 타입이 `kObjectType`인지 판별합니다.
//***************************************************************************
bool CRapidJSONUtil::IsObject() const
{
	return _document.GetType() == kObjectType ? true : false;
}

//***************************************************************************
// @brief 현재 문서가 JSON 배열 타입인지 확인합니다.
// @param 없음
// @return 배열 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서 타입이 `kArrayType`인지 판별합니다.
//***************************************************************************
bool CRapidJSONUtil::IsArray() const
{
	return _document.GetType() == kArrayType ? true : false;
}

//***************************************************************************
// @brief 현재 문서가 JSON 문자열 타입인지 확인합니다.
// @param 없음
// @return 문자열 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 문자열 형식을 만족하는지 검사합니다.
//***************************************************************************
bool CRapidJSONUtil::IsString() const
{
	return _document.IsString();
}

//***************************************************************************
// @brief 현재 문서가 JSON 숫자 타입인지 확인합니다.
// @param 없음
// @return 숫자 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 숫자 형식을 만족하는지 검사합니다.
//***************************************************************************
bool CRapidJSONUtil::IsNumber() const
{
	return _document.IsNumber();
}

//***************************************************************************
// @brief 현재 문서의 문자열 값이 숫자로 변환 가능한 형태인지 확인합니다.
// @param 없음
// @return 숫자 형태의 문자열일 경우 true, 그렇지 않으면 false
// @detail 문자열 내부를 순회하며 부호, 소수점 개수, 숫자 여부를 수동 파싱하여 숫자로 유효한지 판별합니다.
//***************************************************************************
bool CRapidJSONUtil::IsStringNumber() const
{
	if( false == IsString() )
		return false;

	const TCHAR* cch = _document.GetString();
	if( L'-' == *cch )
		++cch;

	int iDotCount = 0;
	for( ; *cch != 0; ++cch )
	{
		if( 0 == isdigit(*cch) )
		{
			if( '.' != *cch )
				return false;

			++iDotCount;
			if( 1 < iDotCount )
				return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 현재 문서가 32비트 정수 타입인지 확인합니다.
// @param 없음
// @return 32비트 정수일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 Int 타입인지 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsInt32() const
{
	return _document.IsInt();
}

//***************************************************************************
// @brief 현재 문서가 64비트 정수 타입인지 확인합니다.
// @param 없음
// @return 64비트 정수일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 Int64 타입인지 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsInt64() const
{
	return _document.IsInt64();
}

//***************************************************************************
// @brief 현재 문서가 32비트 부호 없는 정수 타입인지 확인합니다.
// @param 없음
// @return 32비트 부호 없는 정수일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 Uint 타입인지 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsUint32() const
{
	return _document.IsUint();
}

//***************************************************************************
// @brief 현재 문서가 64비트 부호 없는 정수 타입인지 확인합니다.
// @param 없음
// @return 64비트 부호 없는 정수일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 Uint64 타입인지 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsUint64() const
{
	return _document.IsUint64();
}

//***************************************************************************
// @brief 현재 문서가 실수(Double) 타입인지 확인합니다.
// @param 없음
// @return 실수 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서가 Double 타입인지 확인합니다.
//***************************************************************************
bool CRapidJSONUtil::IsDouble() const
{
	return _document.IsDouble();
}

//***************************************************************************
// @brief 현재 문서가 부울(참/거짓) 타입인지 확인합니다.
// @param 없음
// @return 부울 타입일 경우 true, 그렇지 않으면 false
// @detail RapidJSON 문서의 타입이 참(`kTrueType`) 또는 거짓(`kFalseType`)에 속하는지 검사합니다.
//***************************************************************************
bool CRapidJSONUtil::IsBool() const
{
	switch( _document.GetType() )
	{
	case kTrueType:
	case kFalseType:
		return true;
	}

	return _document.IsBool();
}

//***************************************************************************
// @brief JSON 문자열을 파싱하여 내부 문서를 구성합니다.
// @param jsonString 파싱할 JSON 형식의 문자열
// @return 파싱 성공 시 true, 실패 시 false
// @detail 문자열을 RapidJSON 파서에 통과시키고, 에러가 발생할 경우 에러 로그를 남깁니다.
//***************************************************************************
bool CRapidJSONUtil::Parse(const _tstring& jsonString)
{
	if( _document.Parse(jsonString.c_str()).HasParseError() )
	{
		_tcerr << _T("JSON parsing error!") << std::endl;
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 현재 내부 문서를 JSON 문자열로 변환합니다.
// @param pretty 가독성을 높이기 위한 예쁜 들여쓰기(포맷팅) 적용 여부
// @return 직렬화된 JSON 문자열
// @detail 유니코드 및 플랫폼 환경에 맞춰 적절한 라이터(`Writer` 또는 `PrettyWriter`)를 사용하여 문서를 문자열로 직렬화합니다.
//***************************************************************************
_tstring CRapidJSONUtil::ToString(const bool pretty) const
{
	_tStringBuffer buffer;

	if( pretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer> writer(buffer);
#endif	

		_document.Accept(writer);
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif

		_document.Accept(writer);
	}

	return buffer.GetString();
}

//***************************************************************************
// @brief 현재 내부 문서의 JSON 구조를 표준 출력 스트림에 디버깅용으로 출력합니다.
// @param pretty 들여쓰기 적용 여부
// @return 없음
// @detail `ToString`을 통해 문자열을 얻은 후 표준 출력(`_tcout`)으로 출력합니다.
//***************************************************************************
void CRapidJSONUtil::PrintJSON(const bool pretty) const
{
	_tcout << ToString(pretty) << std::endl;
}

//***************************************************************************
// @brief 지정한 문자열을 파싱하여 파일로 저장합니다.
// @param filename 저장할 파일 경로 및 이름
// @param jsonString 파일에 기록할 JSON 문자열
// @param pretty 들여쓰기 적용 여부
// @return 저장 및 파싱 성공 시 true, 실패 시 false
// @detail 전달받은 문자열을 먼저 파싱한 후, 성공하면 파일 저장 함수를 호출합니다.
//***************************************************************************
bool CRapidJSONUtil::SaveToFile(const _tstring& filename, _tstring& jsonString, const bool pretty)
{
	bool result = false;

	result = Parse(jsonString);
	if( result ) result = SaveToFile(filename, pretty);

	return result;
}

//***************************************************************************
// @brief 현재 내부 문서를 파일로 저장합니다.
// @param filename 저장할 파일 경로 및 이름
// @param pretty 들여쓰기 적용 여부
// @return 저장 성공 시 true, 실패 시 false
// @detail 라이터를 통해 문자열을 생성한 뒤, 인코딩 환경(`ko_KR.UTF-8` 등)에 맞추어 파일 스트림에 내용을 기록합니다.
//***************************************************************************
bool CRapidJSONUtil::SaveToFile(const _tstring& filename, const bool pretty)
{
	_tStringBuffer buffer;

	if( pretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer> writer(buffer);
#endif	

		if( _document.Accept(writer) == false )
		{
			Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, filename.c_str());
			return false;
		}
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif

		if( _document.Accept(writer) == false )
		{
			Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, filename.c_str());
			return false;
		}
	}

	_tstring temp = buffer.GetString();

	_tofstream out(filename, _tofstream::trunc);

#ifdef _UNICODE
	out.imbue(std::locale("ko_KR.UTF-8"));
	out << temp;
#else
	out << Iconv::CIconvUtil::ConvertEncoding(temp, "CP949", "UTF-8");
#endif
	out.close();

	Print_DebugInfo(_T("%s write to [%s] - [%s]\n"), __TFUNCTION__, filename.c_str(), temp.c_str());

	return true;
}

//***************************************************************************
// @brief 파일로부터 JSON 데이터를 읽어와 내부 문서로 로드합니다.
// @param filename 로드할 파일 경로 및 이름
// @return 로드 및 파싱 성공 시 true, 실패 시 false
// @detail 파일 스트림을 열어 전체 텍스트를 읽어온 뒤 `Parse` 함수를 통해 문서화합니다.
//***************************************************************************
bool CRapidJSONUtil::LoadFromFile(const _tstring& filename)
{
	_tifstream ifs(filename);
	if( !ifs.is_open() )
	{
		_tcerr << _T("Failed to open file: ") << filename << std::endl;
		return false;
	}
	_tstring content((std::istreambuf_iterator<TCHAR>(ifs)), std::istreambuf_iterator<TCHAR>());
	ifs.close();

	return Parse(content);
}

//***************************************************************************
// @brief 현재 JSON 객체에 포함된 모든 키 목록을 추출합니다.
// @param 없음
// @return 키 이름이 담긴 문자열 벡터 (`std::vector<_tstring>`)
// @detail 문서가 객체 형태일 경우, 내부 멤버들을 순회하며 각 멤버의 이름을 벡터에 수집하여 반환합니다.
//***************************************************************************
std::vector<_tstring> CRapidJSONUtil::GetKeys()
{
	std::vector<_tstring> keys;

	// JSON 객체 순회
	if( _document.IsObject() )
	{
		for( auto& member : _document.GetObject() )
		{
			keys.push_back(member.name.GetString());
		}
	}

	return keys;
}

//***************************************************************************
// @brief 지정한 키 포인터에 해당하는 객체 속성을 삭제합니다.
// @param ptszKey 삭제할 멤버의 키 이름 (`TCHAR*`)
// @return 없음
// @detail 문서가 객체이고 해당 키가 존재하면, 하위 요소를 재귀 정리한 후 멤버를 제거합니다.
//***************************************************************************
void CRapidJSONUtil::Remove(const TCHAR* ptszKey)
{
	if( _document.IsObject() && _document.HasMember(ptszKey) )
	{
		RecursiveRemove(_document[ptszKey]);
		_document.RemoveMember(ptszKey);
	}
}

//***************************************************************************
// @brief 지정한 `_tstring` 키에 해당하는 객체 속성을 삭제합니다.
// @param key 삭제할 멤버의 키 이름 (`_tstring`)
// @return 없음
// @detail 내부적으로 `Remove(const TCHAR*)`를 호출하여 멤버를 제거합니다.
//***************************************************************************
void CRapidJSONUtil::Remove(const _tstring& key)
{
	return Remove(key.c_str());
}

//***************************************************************************
// @brief 지정한 키의 배열에서 특정 인덱스의 요소를 삭제합니다.
// @param key 대상 배열이 위치한 키 이름
// @param index 삭제할 배열 요소의 인덱스 위치
// @return 없음
// @detail 해당 키가 유효한 배열인지 검사하고 범위 내에 있을 경우, 요소를 재귀 정리한 뒤 배열에서 지웁니다.
//***************************************************************************
void CRapidJSONUtil::Remove(const _tstring& key, uint32 index)
{
	if( _document.HasMember(key.c_str()) && _document[key.c_str()].IsArray() )
	{
		auto& arr = _document[key.c_str()];
		if( index < arr.Size() )
		{
			RecursiveRemove(arr);
			arr.Erase(arr.Begin() + index);
		}
	}
}

//***************************************************************************
// @brief JSON 값 내부의 객체나 배열 요소를 재귀적으로 순회하며 정리합니다.
// @param value 정리할 대상 RapidJSON 값 객체 (`_tValue`)
// @return 없음
// @detail 값이 객체인 경우 멤버들을 재귀 탐색하고 객체로 초기화하며, 배열인 경우 원소들을 재귀 탐색하고 배열로 초기화합니다.
//***************************************************************************
void CRapidJSONUtil::RecursiveRemove(_tValue& value)
{
	if( value.IsObject() )
	{
		for( auto itr = value.MemberBegin(); itr != value.MemberEnd(); ++itr )
		{
			RecursiveRemove(itr->value);
		}
		value.SetObject();
	}
	else if( value.IsArray() )
	{
		for( auto itr = value.Begin(); itr != value.End(); ++itr )
		{
			RecursiveRemove(*itr);
		}
		value.SetArray();
	}
}