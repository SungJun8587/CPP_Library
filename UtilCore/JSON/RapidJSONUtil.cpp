
//***************************************************************************
// RapidJSONUtil.cpp: implementation of the CRapidJSONUtil class.
//
//***************************************************************************

#include "pch.h"
#include "RapidJSONUtil.h"
#include <locale>     // std::locale, std::locale::classic() (SaveToFile/LoadFromFile에서 직접 사용)
#include <stdexcept>  // std::runtime_error (로케일 생성 실패 시 catch에서 직접 사용)

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief `CRapidJSONUtil` 클래스의 기본 인스턴스를 생성합니다.
// @param 없음
// @return 없음
// @detail 내부 문서 객체 할당자를 초기화하고, 기본 문서를 빈 JSON 객체(`kObjectType`) 상태로 설정합니다.
//***************************************************************************
CRapidJSONUtil::CRapidJSONUtil() : _bIsDebugPrint(false)
{
	_document.SetObject();
}

//***************************************************************************
// @brief 다른 `CRapidJSONUtil` 객체의 데이터를 복사하여 새로운 인스턴스를 생성합니다. (복사 생성자)
// @param other 복사할 원본 `CRapidJSONUtil` 객체
// @return 없음
// @detail 원본 객체가 가진 내부 JSON 문서의 전체 내용을 할당자를 통해 깊은 복사(`CopyFrom`)합니다.
//***************************************************************************
CRapidJSONUtil::CRapidJSONUtil(const CRapidJSONUtil& other) : _bIsDebugPrint(other._bIsDebugPrint)
{
	_document.CopyFrom(other._document, _document.GetAllocator());		// 객체 복사
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
// @brief 지정한 키의 멤버가 존재하면 재귀적으로 정리한 뒤 제거합니다.
// @param key 검사/제거할 멤버의 키 이름
// @return 없음
// @detail FindMember를 한 번만 호출해 얻은 이터레이터를 그대로 재사용하여
//         존재 검사와 삭제를 모두 처리합니다(HasMember + operator[] 이중 탐색 방지).
//         제거에는 RemoveMember가 아닌 EraseMember를 사용합니다. RemoveMember는 삭제 위치에
//         마지막 멤버를 옮겨 채우는 O(1) 스왑 방식이라 멤버 순서가 바뀌는 반면, EraseMember는
//         뒤 요소들을 한 칸씩 시프트하여 O(n)이지만 나머지 멤버들의 상대 순서를 보존합니다.
//         Add* 계열 함수들이 "추가"가 아닌 "설정" 의미를 갖도록 호출 전에 사용되므로,
//         같은 키에 값을 반복 설정해도 그 필드가 맨 뒤로 밀려나지 않고 원래 위치를 유지합니다.
//***************************************************************************
void CRapidJSONUtil::RemoveMemberIfExists(const _tstring& key)
{
	if( !_document.IsObject() )
	{
		return;
	}

	auto itr = _document.FindMember(key.c_str());
	if( itr != _document.MemberEnd() )
	{
		RecursiveRemove(itr->value);
		_document.EraseMember(itr);
	}
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
		_document.CopyFrom(other._document, _document.GetAllocator());
	}
	return *this;  // 자신을 리턴하여 연속적인 연산 가능
}

//***************************************************************************
// @brief 문자열 포인터 값을 내부 문서의 문자열 데이터로 설정합니다.
// @param ptszValue 설정할 문자열 포인터 (`TCHAR*`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 내부 문서를 문자열 타입으로 전환하고 지정한 문자열을 할당한 뒤 객체 참조를 반환합니다.
//         nullptr이 전달된 경우 빈 문자열로 안전하게 처리합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator=(const TCHAR* ptszValue)
{
	if( nullptr == ptszValue )
	{
		_document.SetString(_T(""), 0, _document.GetAllocator());
		return *this;
	}
	_document.SetString(ptszValue, _document.GetAllocator());
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
	_document.SetString(strValue.c_str(), (rapidjson::SizeType)strValue.length(), _document.GetAllocator());
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
// @detail 현재 문서가 객체 타입이 아닐 경우 객체로 초기화한 뒤, 기존 동일 키가 있으면 제거하고
//         전달된 서브 문서 객체를 복사하여 새로운 멤버로 추가합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator+(const std::pair<const TCHAR*, CRapidJSONUtil>& keyValue)
{
	if( nullptr == keyValue.first )
	{
		return *this; // 키가 없으면 추가할 수 없으므로 무시
	}

	if( !_document.IsObject() )
	{
		_document.SetObject();
	}

	RemoveMemberIfExists(keyValue.first);

	auto& allocator = _document.GetAllocator();
	_tValue key(keyValue.first, allocator);	// Key를 rapidjson::Value로 변환
	_tValue value;

	// Value도 복사하여 추가해야 함
	value.CopyFrom(keyValue.second._document, allocator);

	_document.AddMember(key, value, allocator);
	return *this;
}

//***************************************************************************
// @brief 키-값 쌍(`_tstring` 키)을 객체에 추가하여 확장합니다.
// @param keyValue 추가할 키와 `CRapidJSONUtil` 객체가 담긴 페어(`std::pair`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail 현재 문서가 객체 타입이 아닐 경우 객체로 초기화한 뒤, 기존 동일 키가 있으면 제거하고
//         전달된 서브 문서 객체를 복사하여 새로운 멤버로 추가합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator+(const std::pair<_tstring, CRapidJSONUtil>& keyValue)
{
	if( !_document.IsObject() )
	{
		_document.SetObject();
	}

	RemoveMemberIfExists(keyValue.first);

	auto& allocator = _document.GetAllocator();
	_tValue key(keyValue.first.c_str(), (rapidjson::SizeType)keyValue.first.length(), allocator);	// Key를 rapidjson::Value로 변환
	_tValue value;

	// Value도 복사하여 추가해야 함
	value.CopyFrom(keyValue.second._document, allocator);

	_document.AddMember(key, value, allocator);
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
	value.CopyFrom(other._document, _document.GetAllocator());	// JSON 객체를 복사하여 추가
	_document.PushBack(value, _document.GetAllocator());
	return *this;
}

//***************************************************************************
// @brief 문자열 포인터에 해당하는 객체 속성을 삭제합니다.
// @param ptszKey 삭제할 JSON 멤버의 키 이름 (`TCHAR*`)
// @return 자기 자신(`CRapidJSONUtil&`)에 대한 참조
// @detail RemoveMemberIfExists를 통해 존재 검사와 재귀 정리, 제거를 한 번의 탐색으로 처리합니다.
//***************************************************************************
CRapidJSONUtil& CRapidJSONUtil::operator-(const TCHAR* ptszKey)
{
	if( nullptr != ptszKey )
	{
		RemoveMemberIfExists(ptszKey);
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
	if( nullptr == ptszKey )
		return false;
	return _document.IsObject() && _document.HasMember(ptszKey);
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
// @detail 부호(-), 정수부/소수부(점 하나까지), 그리고 지수부(e/E, 부호, 자릿수)까지
//         지원하는 형태로 문자열을 파싱하여 숫자로 유효한지 판별합니다.
//         "1e10", "1.5e-3", "-2.5E+10" 등을 숫자로 인식합니다.
//         - 빈 문자열, "-" 단독, "." 단독, 지수부에 자릿수가 없는 경우("1e", "1e+")는 false.
//         - `_istdigit`을 사용하여 TCHAR가 char/wchar_t 어느 쪽이든 안전하게 판별합니다
//           (isdigit에 wchar_t를 직접 넘기는 것은 UB이므로 이를 회피합니다).
//***************************************************************************
bool CRapidJSONUtil::IsStringNumber() const
{
	if( false == IsString() )
		return false;

	const TCHAR* cch = _document.GetString();
	if( nullptr == cch || _T('\0') == *cch )
		return false;

	if( _T('-') == *cch )
		++cch;

	if( _T('\0') == *cch )
		return false; // "-" 단독은 숫자가 아님

	// 정수부/소수부: 점은 최대 한 번, 최소 하나의 자릿수가 필요.
	int  iDotCount = 0;
	bool bHasDigit = false;

	for( ; *cch != 0 && *cch != _T('e') && *cch != _T('E'); ++cch )
	{
		if( 0 != _istdigit(*cch) )
		{
			bHasDigit = true;
			continue;
		}

		if( _T('.') == *cch )
		{
			++iDotCount;
			if( 1 < iDotCount )
				return false;
			continue;
		}

		return false; // 그 외 문자는 숫자로 볼 수 없음
	}

	if( !bHasDigit )
		return false; // 정수부/소수부에 자릿수가 하나도 없음(".", "" 등)

	// 지수부(선택 사항): e/E [+/-] 자릿수+
	if( _T('e') == *cch || _T('E') == *cch )
	{
		++cch;

		if( _T('+') == *cch || _T('-') == *cch )
			++cch;

		bool bHasExpDigit = false;
		for( ; *cch != 0; ++cch )
		{
			if( 0 == _istdigit(*cch) )
				return false;
			bHasExpDigit = true;
		}

		if( !bHasExpDigit )
			return false; // "1e", "1e+" 등은 숫자가 아님
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
// @detail RapidJSON 문서의 IsBool()로 참/거짓 타입 여부를 검사합니다.
//***************************************************************************
bool CRapidJSONUtil::IsBool() const
{
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
// @detail 라이터를 통해 문자열을 생성한 뒤, 논유니코드 빌드에서는 내부 문자열(CP949)을
//         UTF-8로 변환하여 파일에 기록합니다. LoadFromFile은 이 변환을 역으로 되돌리므로
//         Save/Load 왕복 시 인코딩이 어긋나지 않습니다.
//         로케일 설정("ko_KR.UTF-8")이 시스템에 없을 수 있으므로 예외를 잡아
//         실패 시 기본 로케일로 안전하게 대체합니다.
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
	if( !out.is_open() )
	{
		Print_DebugInfo(_T("%s failed to open output file [%s]\n"), __TFUNCTION__, filename.c_str());
		return false;
	}

#ifdef _UNICODE
	try
	{
		out.imbue(std::locale("ko_KR.UTF-8"));
	}
	catch( const std::runtime_error& )
	{
		// 시스템에 해당 로케일이 설치되어 있지 않은 경우, 기본(classic) 로케일로 대체.
		Print_DebugInfo(_T("%s locale ko_KR.UTF-8 not available, falling back to classic locale\n"), __TFUNCTION__);
		out.imbue(std::locale::classic());
	}
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
//         - 유니코드 빌드: SaveToFile이 "ko_KR.UTF-8" 로케일로 imbue하여 내부 wchar_t를
//           UTF-8 바이트로 인코딩해 저장하므로, 읽을 때도 동일한 로케일을 imbue해야 합니다.
//           imbue하지 않으면 기본("C") 로케일이 UTF-8을 디코딩하지 못해 바이트를 그대로
//           wchar_t로 확장(1:1)해버려 비ASCII 문자가 깨집니다. (이전에는 이 imbue가 누락되어
//           SaveToFile→LoadFromFile 왕복 시 한글 등이 깨지는 버그가 있었습니다.)
//         - 논유니코드 빌드: SaveToFile의 CP949→UTF-8 변환을 역으로 되돌립니다.
//         두 경우 모두 SaveToFile과 대칭을 이루어 왕복 시 인코딩이 깨지지 않도록 합니다.
//***************************************************************************
bool CRapidJSONUtil::LoadFromFile(const _tstring& filename)
{
	_tifstream ifs(filename);
	if( !ifs.is_open() )
	{
		_tcerr << _T("Failed to open file: ") << filename << std::endl;
		return false;
	}

#ifdef _UNICODE
	try
	{
		ifs.imbue(std::locale("ko_KR.UTF-8"));
	}
	catch( const std::runtime_error& )
	{
		// SaveToFile과 동일하게, 로케일이 없으면 기본(classic) 로케일로 대체.
		Print_DebugInfo(_T("%s locale ko_KR.UTF-8 not available, falling back to classic locale\n"), __TFUNCTION__);
		ifs.imbue(std::locale::classic());
	}
#endif

	_tstring content((std::istreambuf_iterator<TCHAR>(ifs)), std::istreambuf_iterator<TCHAR>());
	ifs.close();

#ifndef _UNICODE
	content = Iconv::CIconvUtil::ConvertEncoding(content, "UTF-8", "CP949");
#endif

	return Parse(content);
}

//***************************************************************************
// @brief 현재 JSON 객체에 포함된 모든 키 목록을 추출합니다.
// @param 없음
// @return 키 이름이 담긴 문자열 벡터 (`std::vector<_tstring>`)
// @detail 문서가 객체 형태일 경우, 내부 멤버들을 순회하며 각 멤버의 이름을 벡터에 수집하여 반환합니다.
//         재할당 횟수를 줄이기 위해 미리 멤버 수만큼 reserve합니다.
//***************************************************************************
std::vector<_tstring> CRapidJSONUtil::GetKeys() const
{
	std::vector<_tstring> keys;

	// JSON 객체 순회
	if( _document.IsObject() )
	{
		keys.reserve(_document.MemberCount());
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
// @detail RemoveMemberIfExists를 통해 존재 검사, 재귀 정리, 제거를 한 번의 탐색으로 처리합니다.
//***************************************************************************
void CRapidJSONUtil::Remove(const TCHAR* ptszKey)
{
	if( nullptr == ptszKey )
		return;
	RemoveMemberIfExists(ptszKey);
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
// @detail FindMember로 한 번만 탐색한 이터레이터를 재사용하여 배열 여부와 범위를 검사하고,
//         범위 내에 있을 경우 해당 인덱스의 요소만 재귀 정리한 뒤 배열에서 지웁니다.
//         (기존 코드는 배열 전체를 RecursiveRemove하여 삭제 대상이 아닌 요소까지
//         초기화해버리는 버그가 있었습니다.)
//***************************************************************************
void CRapidJSONUtil::Remove(const _tstring& key, uint32 index)
{
	if( !_document.IsObject() )
	{
		return;
	}

	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() || !itr->value.IsArray() )
	{
		return;
	}

	auto& arr = itr->value;
	if( index < arr.Size() )
	{
		RecursiveRemove(arr[index]);
		arr.Erase(arr.Begin() + index);
	}
}

//***************************************************************************
// @brief JSON 값 내부의 객체나 배열 요소를 재귀적으로 순회하며 정리합니다.
// @param value 정리할 대상 RapidJSON 값 객체 (`_tValue`)
// @param depth 현재 재귀 깊이 (기본값 0). 신뢰할 수 없는 입력으로 인한
//              스택 오버플로우를 방지하기 위해 RAPIDJSONUTIL_MAX_RECURSION_DEPTH에서 중단합니다.
// @return 없음
// @detail 값이 객체인 경우 멤버들을 재귀 탐색하고 객체로 초기화하며, 배열인 경우 원소들을 재귀 탐색하고 배열로 초기화합니다.
//***************************************************************************
void CRapidJSONUtil::RecursiveRemove(_tValue& value, int depth)
{
	if( depth >= RAPIDJSONUTIL_MAX_RECURSION_DEPTH )
	{
		Print_DebugInfo(_T("%s max recursion depth reached, aborting cleanup early\n"), __TFUNCTION__);
		return;
	}

	if( value.IsObject() )
	{
		for( auto itr = value.MemberBegin(); itr != value.MemberEnd(); ++itr )
		{
			RecursiveRemove(itr->value, depth + 1);
		}
		value.SetObject();
	}
	else if( value.IsArray() )
	{
		for( auto itr = value.Begin(); itr != value.End(); ++itr )
		{
			RecursiveRemove(*itr, depth + 1);
		}
		value.SetArray();
	}
}