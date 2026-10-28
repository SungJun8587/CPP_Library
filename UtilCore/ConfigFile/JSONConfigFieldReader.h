
//***************************************************************************
// JSONConfigFieldReader.h : interface for the CJSONConfigFieldReader class.
//
//***************************************************************************

#ifndef UC_JSONCONFIGFIELDREADER_H
#define UC_JSONCONFIGFIELDREADER_H

#include <ConfigFile/ConfigFieldCommon.h>
#include <JSON/RapidJSONUtil.h>

#include <initializer_list>
#include <set>
#include <type_traits>
#include <vector>

//***************************************************************************
// @class CJSONConfigFieldReader
// @brief CRapidJSONUtil로 로드한 설정 JSON에서 필드를 타입/범위 검증하며 읽는 도우미.
// @detail
// CRapidJSONUtil의 operator[]는 키가 없거나 타입이 달라도 0/빈 문자열을 돌려줘서
// "값이 0"과 "키 없음/오타/타입 오류"를 구분할 수 없다. 이 클래스는 문서를 직접 확인해
//     - 키가 없으면 대상 변수를 건드리지 않고(= 기본값 유지),
//     - 키는 있는데 타입이 맞지 않거나 범위를 벗어나면 오류로 기록한다.
// 모든 필드를 읽은 뒤 Validate()를 호출해 오류를 로그로 남기고 성공 여부를 받는다.
// 읽히지 않은 키(오타 가능성)는 경고만 남긴다.
//
// 읽기 함수는 대상 변수의 타입으로 오버로드를 고른다.
//     reader.Read(_T("Port"), port, 1, 65535);		// 모든 정수형(8~64비트, 부호 유무 무관)과 float/double
//     reader.Read(_T("Name"), name);				// TCHAR 배열(길이 초과는 오류), _tstring
//     reader.Read(_T("Verbose"), verbose);			// bool
//     reader.Read(_T("Ports"), ports, 1, 65535);	// 위 타입의 vector 계열(원소마다 검증, 범위 지정 가능)
//     reader.ReadObjects(_T("Nodes"), nodes, {...});	// FromJSON을 가진 사용자 타입의 배열
// 컨테이너는 value_type/push_back/clear를 가진 것이면 되고(std::vector, CVector 등) 읽기에 성공하면 내용을 통째로 교체한다.
//***************************************************************************
class CJSONConfigFieldReader : public CConfigErrorLog
{
	template <typename T> using Identity = ConfigField::Identity<T>;	// 템플릿 인자 추론에서 제외하기 위한 도구
	template <typename T> static constexpr bool IsNumeric = ConfigField::IsNumeric<T>;	// bool을 제외한 산술 타입인지
	template <typename C> static constexpr bool IsScalarList = ConfigField::IsScalarList<C>;	// 숫자/bool/문자열을 담는 컨테이너인지

public:
	//***************************************************************************
	// @brief 생성자. 이미 로드된 JSON 문서에서 읽도록 연결합니다.
	// @param json 설정이 로드된 CRapidJSONUtil. 이 Reader보다 오래 살아 있어야 합니다
	//***************************************************************************
	explicit CJSONConfigFieldReader(CRapidJSONUtil& json) : _json(json) {}

	//***************************************************************************
	// @brief 정수/실수 필드를 [minV, maxV] 범위 검증과 함께 읽습니다.
	// @detail 범위를 생략하면 대상 타입이 표현할 수 있는 전체 범위다. 실수에 정수 리터럴(1000)이 적혀 있어도 받아들인다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @param minV 허용하는 최소값 (생략하면 대상 타입의 최소값)
	// @param maxV 허용하는 최대값 (생략하면 대상 타입의 최대값)
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	template <typename T, typename = std::enable_if_t<IsNumeric<T>>>
	bool Read(const TCHAR* key, T& out,
		typename Identity<T>::type minV = (std::numeric_limits<T>::lowest)(),
		typename Identity<T>::type maxV = (std::numeric_limits<T>::max)())
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;

		T value{};
		TCHAR why[128];
		if( !Convert(*v, value, minV, maxV, why, _countof(why)) )
			return Fail(key, why);

		out = value;
		return true;
	}

	//***************************************************************************
	// @brief bool 필드를 읽습니다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	bool Read(const TCHAR* key, bool& out)
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;
		if( !v->IsBool() )
			return Fail(key, _T("must be a boolean"));

		out = v->GetBool();
		return true;
	}

	//***************************************************************************
	// @brief 문자열 필드를 _tstring으로 읽습니다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	bool Read(const TCHAR* key, _tstring& out)
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;
		if( !v->IsString() )
			return Fail(key, _T("must be a string"));

		out.assign(v->GetString(), v->GetStringLength());
		return true;
	}

	//***************************************************************************
	// @brief 문자열 필드를 고정 크기 TCHAR 배열로 읽습니다. 버퍼(널 포함)에 들어가지 않으면 오류입니다.
	// @param key 읽을 키 이름
	// @param dst [OUT] 문자열을 받을 고정 크기 버퍼
	// @return 키가 없거나 정상적으로 읽었으면 true, 문자열이 아니거나 버퍼에 들어가지 않으면 false(오류 기록)
	//***************************************************************************
	template <size_t N>
	bool Read(const TCHAR* key, TCHAR (&dst)[N])
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;
		if( !v->IsString() )
			return Fail(key, _T("must be a string"));
		if( static_cast<size_t>(v->GetStringLength()) >= N )
			return Fail(key, _T("string is too long"));

		_tcsncpy_s(dst, N, v->GetString(), _TRUNCATE);
		return true;
	}

	//***************************************************************************
	// @brief 숫자/bool/문자열 배열을 컨테이너(std::vector, CVector 등)로 읽습니다.
	// @detail 한 원소라도 타입/범위가 맞지 않으면 오류로 기록하고 out은 건드리지 않는다.
	//          (CRapidJSONUtil::GetVector는 맞지 않는 원소를 조용히 건너뛰므로 설정 용도로는 위험하다.)
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 컨테이너. 성공하면 내용을 통째로 교체
	// @param minV 각 원소가 가질 수 있는 최소값 (숫자 원소에만 적용)
	// @param maxV 각 원소가 가질 수 있는 최대값 (숫자 원소에만 적용)
	// @return 키가 없거나 정상적으로 읽었으면 true, 배열이 아니거나 원소가 하나라도 맞지 않으면 false(오류 기록, out은 변경하지 않음)
	//***************************************************************************
	template <typename C, typename = std::enable_if_t<IsScalarList<C>>>
	bool Read(const TCHAR* key, C& out,
		typename Identity<typename C::value_type>::type minV = ConfigField::DefaultMin<typename C::value_type>(),
		typename Identity<typename C::value_type>::type maxV = ConfigField::DefaultMax<typename C::value_type>())
	{
		using E = typename C::value_type;

		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;
		if( !v->IsArray() )
			return Fail(key, _T("must be an array"));

		C result;
		for( rapidjson::SizeType i = 0; i < v->Size(); ++i )
		{
			E element{};
			TCHAR why[128];
			if( !ConvertElement((*v)[i], element, minV, maxV, why, _countof(why)) )
			{
				TCHAR name[128];
				_sntprintf_s(name, _countof(name), _TRUNCATE, _T("%s[%u]"), key, static_cast<uint32>(i));
				return Fail(name, why);
			}
			result.push_back(std::move(element));
		}

		out = std::move(result);
		return true;
	}

	//***************************************************************************
	// @brief FromJSON을 가진 사용자 타입의 객체 배열을 읽습니다.
	// @detail 사용자 타입의 FromJSON은 필드 존재/타입을 확인하지 않는 경우가 많아(없으면 정의되지 않은 동작),
	//          역직렬화 전에 모든 원소를 specs로 검증한다. 검증을 통과하면 CRapidJSONUtil::Deserialize로 채운다.
	//          원소 타입은 CRapidJSONUtil이 요구하는 대로 ToJSON/FromJSON 멤버를 가져야 한다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 객체를 받을 컨테이너
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @return 키가 없거나 정상적으로 읽었으면 true, 배열이 아니거나 원소가 specs를 만족하지 않으면 false(오류 기록)
	//***************************************************************************
	template <typename C>
	bool ReadObjects(const TCHAR* key, C& out, std::initializer_list<SFieldSpec> specs)
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;
		if( !v->IsArray() )
			return Fail(key, _T("must be an array"));

		for( rapidjson::SizeType i = 0; i < v->Size(); ++i )
		{
			TCHAR why[160];
			if( !CheckObject((*v)[i], specs, why, _countof(why)) )
			{
				TCHAR name[128];
				_sntprintf_s(name, _countof(name), _TRUNCATE, _T("%s[%u]"), key, static_cast<uint32>(i));
				return Fail(name, why);
			}
		}

		out = _json.Deserialize<C>(key);
		return true;
	}

	//***************************************************************************
	// @brief FromJSON을 가진 사용자 타입의 객체 하나를 읽습니다. (ReadObjects의 단일 객체 버전)
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 객체를 받을 변수
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @return 키가 없거나 정상적으로 읽었으면 true, 객체가 specs를 만족하지 않으면 false(오류 기록)
	//***************************************************************************
	template <typename T>
	bool ReadObject(const TCHAR* key, T& out, std::initializer_list<SFieldSpec> specs)
	{
		const _tValue* v = Find(key);
		if( v == nullptr )
			return true;

		TCHAR why[160];
		if( !CheckObject(*v, specs, why, _countof(why)) )
			return Fail(key, why);

		out = _json.Deserialize<T>(key);
		return true;
	}

	//***************************************************************************
	// @brief 이 클래스로 읽지 않지만 파일에 있어도 되는 키를 등록합니다. (미사용 키 경고 방지)
	// @param key 경고에서 제외할 키 이름
	//***************************************************************************
	void Ignore(const TCHAR* key) { _known.insert(key); }

	//***************************************************************************
	// @brief 읽기를 마치고 결과를 보고합니다.
	// @return 오류가 하나도 없으면 true
	//***************************************************************************
	bool Validate() const
	{
		for( const _tstring& key : _json.GetKeys() )
		{
			if( _known.find(key) == _known.end() )
				LOG_WARNING(_T("[Config] unknown key ignored. key=%s"), key.c_str());
		}

		LogErrors(_T("invalid"));
		return !HasErrors();
	}

private:
	//***************************************************************************
	// @brief 문서 최상위에서 키의 값을 찾고, 읽기를 시도한 키로 기록합니다. null 값은 없는 것으로 봅니다.
	// @param key 읽을 키 이름
	// @return 값을 가리키는 포인터. 키가 없거나 null이거나 문서가 객체가 아니면 nullptr
	//***************************************************************************
	const _tValue* Find(const TCHAR* key)
	{
		_known.insert(key);

		const _tDocument& doc = _json.GetDocument();
		if( !doc.IsObject() )
			return nullptr;

		auto it = doc.FindMember(key);
		if( it == doc.MemberEnd() || it->value.IsNull() )
			return nullptr;
		return &it->value;
	}

	//***************************************************************************
	// @brief JSON 숫자를 T로 변환하며 타입/범위를 검사합니다. 실패 시 사유를 why에 적습니다.
	// @detail 정수형 T는 JSON이 정수일 때만(1.5는 거부), 부호 없는 T는 음수를 거부한다.
	//          비교는 64비트로 하므로 uint32(4294967296)나 int8(300) 같은 넘침도 범위 오류로 잡힌다.
	// @param v 변환할 JSON 값
	// @param out [OUT] 변환 결과
	// @param minV 허용하는 최소값
	// @param maxV 허용하는 최대값
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 변환에 성공하면 true, 타입/범위가 맞지 않으면 false(사유는 why에 기록)
	//***************************************************************************
	template <typename T>
	static bool Convert(const _tValue& v, T& out, T minV, T maxV, TCHAR* why, size_t whyCount)
	{
		if constexpr( std::is_floating_point_v<T> )
		{
			if( !v.IsNumber() )
				return ConfigField::Describe(why, whyCount, _T("must be a number"));

			const double d = v.GetDouble();
			if( d < static_cast<double>(minV) || d > static_cast<double>(maxV) )
				return ConfigField::Describe(why, whyCount, _T("out of range"), d);
			out = static_cast<T>(d);
			return true;
		}
		else if constexpr( std::is_signed_v<T> )
		{
			if( !v.IsInt64() )
				return ConfigField::Describe(why, whyCount, _T("must be an integer"));

			const int64 n = v.GetInt64();
			if( n < static_cast<int64>(minV) || n > static_cast<int64>(maxV) )
				return ConfigField::DescribeRange(why, whyCount, static_cast<int64>(minV), static_cast<int64>(maxV), n);
			out = static_cast<T>(n);
			return true;
		}
		else
		{
			if( !v.IsUint64() )
				return ConfigField::Describe(why, whyCount, _T("must be a non-negative integer"));

			const uint64 n = v.GetUint64();
			if( n < static_cast<uint64>(minV) || n > static_cast<uint64>(maxV) )
				return ConfigField::DescribeRangeU(why, whyCount, static_cast<uint64>(minV), static_cast<uint64>(maxV), n);
			out = static_cast<T>(n);
			return true;
		}
	}

	//***************************************************************************
	// @brief 배열 원소 하나를 E로 변환합니다. (숫자/bool/문자열)
	// @param v 변환할 배열 원소
	// @param out [OUT] 변환 결과
	// @param minV 허용하는 최소값 (숫자 원소에만 적용)
	// @param maxV 허용하는 최대값 (숫자 원소에만 적용)
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 변환에 성공하면 true, 타입/범위가 맞지 않으면 false(사유는 why에 기록)
	//***************************************************************************
	template <typename E>
	static bool ConvertElement(const _tValue& v, E& out, E minV, E maxV, TCHAR* why, size_t whyCount)
	{
		if constexpr( std::is_same_v<E, bool> )
		{
			if( !v.IsBool() )
				return ConfigField::Describe(why, whyCount, _T("must be a boolean"));
			out = v.GetBool();
			return true;
		}
		else if constexpr( std::is_same_v<E, _tstring> )
		{
			if( !v.IsString() )
				return ConfigField::Describe(why, whyCount, _T("must be a string"));
			out.assign(v.GetString(), v.GetStringLength());
			return true;
		}
		else
		{
			return Convert(v, out, minV, maxV, why, whyCount);
		}
	}

	//***************************************************************************
	// @brief 객체의 각 필드가 specs를 만족하는지 검사합니다.
	// @param obj 검사할 JSON 객체
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 모든 필드가 specs를 만족하면 true, 아니면 false(사유는 why에 기록)
	//***************************************************************************
	static bool CheckObject(const _tValue& obj, std::initializer_list<SFieldSpec> specs, TCHAR* why, size_t whyCount)
	{
		if( !obj.IsObject() )
			return ConfigField::Describe(why, whyCount, _T("must be an object"));

		for( const SFieldSpec& spec : specs )
		{
			auto it = obj.FindMember(spec.name);
			if( it == obj.MemberEnd() || it->value.IsNull() )
			{
				if( !spec.required )
					continue;
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' is required"), spec.name);
				return false;
			}

			const _tValue& f = it->value;
			bool ok = false;
			switch( spec.type )
			{
			case EFieldType::Int:
				ok = f.IsInt64() && ConfigField::InSpecRange(spec, f.GetInt64());
				break;
			case EFieldType::UInt:
				ok = f.IsUint64() && ConfigField::InSpecRangeU(spec, f.GetUint64());
				break;
			case EFieldType::Number:
				ok = f.IsNumber();
				break;
			case EFieldType::String:
				ok = f.IsString();
				break;
			case EFieldType::Bool:
				ok = f.IsBool();
				break;
			}

			if( !ok )
			{
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' has wrong type or is out of range"), spec.name);
				return false;
			}
		}
		return true;
	}

private:
	CRapidJSONUtil&				_json;	// 읽을 JSON 문서 (외부 소유)
	std::set<_tstring>			_known;		// 읽기를 시도한 키
};

#endif // ndef UC_JSONCONFIGFIELDREADER_H
