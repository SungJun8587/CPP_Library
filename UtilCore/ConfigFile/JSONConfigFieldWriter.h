
//***************************************************************************
// JSONConfigFieldWriter.h : interface for the CJSONConfigFieldWriter class.
//
//***************************************************************************

#ifndef UC_JSONCONFIGFIELDWRITER_H
#define UC_JSONCONFIGFIELDWRITER_H

#include <ConfigFile/ConfigFieldCommon.h>
#include <JSON/RapidJSONUtil.h>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>

//***************************************************************************
// @class CJSONConfigFieldWriter
// @brief CRapidJSONUtil 문서에 설정 필드를 쓰고 파일로 저장하는 도우미. CJSONConfigFieldReader의 짝.
// @detail
// 읽기 쪽과 같은 규칙으로 대상 변수의 타입에 따라 오버로드를 고른다.
//     writer.Write(_T("Port"), port);					// 모든 정수형(8~64비트, 부호 유무 무관), float/double
//     writer.Write(_T("Name"), name);					// _tstring, const TCHAR*, TCHAR 배열
//     writer.Write(_T("Verbose"), verbose);			// bool
//     writer.Write(_T("Ports"), ports);				// 위 타입의 vector 계열
//     writer.WriteObjects(_T("Nodes"), nodes);			// ToJSON을 가진 사용자 타입의 배열
//     writer.Save(_T("Config\\server.json"));			// 임시 파일에 쓴 뒤 교체
//
// CRapidJSONUtil::AddValue 대신 이 클래스를 쓰는 이유:
//     - float이 double로 넓혀지며 0.1f가 0.10000000149011612로 저장되는 문제를 막는다
//       (float로 되돌려 읽었을 때 같은 값이 되는 가장 짧은 십진수로 기록).
//     - int8/uint8/long 등 CRapidJSONUtil이 변환하지 못하는 정수형도 쓸 수 있다.
//     - NaN/Inf(JSON으로 표현할 수 없음)는 문서를 망가뜨리기 전에 오류로 기록한다.
//     - 같은 키를 다시 쓰면 멤버 순서를 유지한 채 값만 교체한다.
//     - 저장은 임시 파일에 쓴 뒤 교체하므로, 저장 도중 실패해도 기존 파일이 잘리지 않는다.
// 키는 문서 최상위에 평면으로 기록한다(CRapidJSONUtil과 같은 방식).
//***************************************************************************
class CJSONConfigFieldWriter : public CConfigErrorLog
{
	template <typename T> using Identity = ConfigField::Identity<T>;	// 템플릿 인자 추론에서 제외하기 위한 도구
	template <typename T> static constexpr bool IsNumeric = ConfigField::IsNumeric<T>;	// bool을 제외한 산술 타입인지
	template <typename C> static constexpr bool IsScalarList = ConfigField::IsScalarList<C>;	// 숫자/bool/문자열을 담는 컨테이너인지

public:
	//***************************************************************************
	// @brief 생성자. 문서가 객체가 아니면 빈 객체로 초기화합니다.
	// @param json 값을 기록할 CRapidJSONUtil. 이 Writer보다 오래 살아 있어야 합니다
	//***************************************************************************
	explicit CJSONConfigFieldWriter(CRapidJSONUtil& json) : _json(json)
	{
		if( !_json.GetDocument().IsObject() )
			_json.GetDocument().SetObject();
	}

	//***************************************************************************
	// @brief 정수/실수 값을 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 정수/실수 값
	// @return 기록에 성공하면 true, NaN/Inf면 false(오류 기록)
	//***************************************************************************
	template <typename T, std::enable_if_t<IsNumeric<T>, int> = 0>
	bool Write(const TCHAR* key, const T& value)
	{
		_tValue v;
		if( !MakeNumber(value, v) )
			return Fail(key, _T("number is not finite (NaN/Inf cannot be written as JSON)"));

		Set(key, v);
		return true;
	}

	//***************************************************************************
	// @brief bool 값을 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 값
	// @return 기록에 성공하면 true
	//***************************************************************************
	bool Write(const TCHAR* key, bool value)
	{
		_tValue v(value);
		Set(key, v);
		return true;
	}

	//***************************************************************************
	// @brief 문자열을 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 문자열
	// @return 기록에 성공하면 true
	//***************************************************************************
	bool Write(const TCHAR* key, const _tstring& value)
	{
		_tValue v(value.c_str(), static_cast<rapidjson::SizeType>(value.length()), Allocator());
		Set(key, v);
		return true;
	}

	//***************************************************************************
	// @brief 문자열을 씁니다. TCHAR 배열과 문자열 리터럴도 이 오버로드로 들어오며, 널 문자까지만 기록한다.
	// @param key 기록할 키 이름
	// @param value 기록할 널 종료 문자열. nullptr이면 빈 문자열
	// @return 기록에 성공하면 true
	//***************************************************************************
	bool Write(const TCHAR* key, const TCHAR* value)
	{
		_tValue v;
		if( value == nullptr )
			v.SetString(_T(""), 0, Allocator());
		else
			v.SetString(value, static_cast<rapidjson::SizeType>(_tcslen(value)), Allocator());
		Set(key, v);
		return true;
	}

	//***************************************************************************
	// @brief 숫자/bool/문자열 컨테이너(std::vector, CVector 등)를 JSON 배열로 씁니다.
	// @param key 기록할 키 이름
	// @param values 기록할 컨테이너
	// @return 기록에 성공하면 true, 원소에 NaN/Inf가 있으면 false(오류 기록, 문서는 변경하지 않음)
	//***************************************************************************
	template <typename C, std::enable_if_t<IsScalarList<C>, int> = 0>
	bool Write(const TCHAR* key, const C& values)
	{
		using E = typename C::value_type;

		_tValue array(rapidjson::kArrayType);
		rapidjson::SizeType index = 0;
		for( const auto& element : values )
		{
			_tValue item;
			if constexpr( std::is_same_v<E, _tstring> )
			{
				item.SetString(element.c_str(), static_cast<rapidjson::SizeType>(element.length()), Allocator());
			}
			else if constexpr( std::is_same_v<E, bool> )
			{
				item.SetBool(static_cast<bool>(element));
			}
			else if( !MakeNumber(static_cast<E>(element), item) )
			{
				TCHAR name[128];
				_sntprintf_s(name, _countof(name), _TRUNCATE, _T("%s[%u]"), key, static_cast<uint32>(index));
				return Fail(name, _T("number is not finite (NaN/Inf cannot be written as JSON)"));
			}
			array.PushBack(item, Allocator());
			++index;
		}

		Set(key, array);
		return true;
	}

	//***************************************************************************
	// @brief ToJSON을 가진 사용자 타입 객체 하나를 씁니다.
	// @param key 기록할 키 이름
	// @param object 기록할 객체. ToJSON 멤버가 필요
	// @return 기록에 성공하면 true
	//***************************************************************************
	template <typename T>
	bool WriteObject(const TCHAR* key, const T& object)
	{
		static_assert(HasToJSON<T>::value, "WriteObject: T needs 'void ToJSON(_tValue&, _tDocument::AllocatorType&) const'");

		_tValue v(rapidjson::kObjectType);
		object.ToJSON(v, Allocator());
		Set(key, v);
		return true;
	}

	//***************************************************************************
	// @brief ToJSON을 가진 사용자 타입 컨테이너를 JSON 객체 배열로 씁니다.
	// @param key 기록할 키 이름
	// @param objects 기록할 객체 컨테이너. 원소에 ToJSON 멤버가 필요
	// @return 기록에 성공하면 true
	//***************************************************************************
	template <typename C>
	bool WriteObjects(const TCHAR* key, const C& objects)
	{
		static_assert(HasToJSON<typename C::value_type>::value, "WriteObjects: element needs 'void ToJSON(_tValue&, _tDocument::AllocatorType&) const'");

		_tValue array(rapidjson::kArrayType);
		for( const auto& object : objects )
		{
			_tValue item(rapidjson::kObjectType);
			object.ToJSON(item, Allocator());
			array.PushBack(item, Allocator());
		}

		Set(key, array);
		return true;
	}

	//***************************************************************************
	// @brief 문서를 파일로 저장합니다. (들여쓰기 적용)
	// @detail path + ".tmp"에 먼저 쓰고 성공하면 path로 교체한다. 쓰기 오류가 기록돼 있으면 저장하지 않는다.
	// @param path 저장할 파일 경로
	// @return 저장에 성공하면 true, 쓰기 오류가 기록돼 있거나 저장에 실패하면 false(기존 파일은 그대로 남음)
	//***************************************************************************
	bool Save(const TCHAR* path)
	{
		if( !_errors.empty() )
		{
			ReportErrors();
			return false;
		}

		const _tstring temp = _tstring(path) + _T(".tmp");
		if( !_json.SaveToFile(temp, true) )
		{
			LOG_ERROR(_T("[Config] save failed. path=%s"), temp.c_str());
			return false;
		}

		if( !ConfigField::ReplaceFile(temp.c_str(), path) )
		{
			LOG_ERROR(_T("[Config] replace failed. path=%s"), path);
			ConfigField::RemoveFile(temp.c_str());
			return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief 기록된 쓰기 오류를 로그로 남깁니다.
	//***************************************************************************
	void ReportErrors() const
	{
		LogErrors(_T("write failed"));
	}

private:
	//***************************************************************************
	// @struct HasToJSON
	// @brief T가 ToJSON 멤버 함수를 가지고 있는지 판별하는 타입 트레이트.
	// @detail const T 객체에서 void ToJSON(_tValue&, _tDocument::AllocatorType&) const를 호출할 수 있으면 true. WriteObject/WriteObjects의 static_assert에 사용한다.
	//***************************************************************************
	template <typename T, typename = void>
	struct HasToJSON : std::false_type {};

	template <typename T>
	struct HasToJSON<T, std::void_t<decltype(std::declval<const T&>().ToJSON(std::declval<_tValue&>(), std::declval<_tDocument::AllocatorType&>()))>> : std::true_type {};

	//***************************************************************************
	// @brief 문서의 allocator를 돌려줍니다.
	// @return 문서의 allocator 참조
	//***************************************************************************
	_tDocument::AllocatorType& Allocator() { return _json.GetDocument().GetAllocator(); }

	//***************************************************************************
	// @brief 숫자를 JSON 값으로 만듭니다. 정수는 32/64비트로 정규화하고, float은 짧은 십진 표현을 사용합니다.
	// @param value 변환할 숫자
	// @param out [OUT] 만들어진 JSON 값
	// @return 변환에 성공하면 true, NaN/Inf면 false
	//***************************************************************************
	template <typename T>
	static bool MakeNumber(T value, _tValue& out)
	{
		if constexpr( std::is_floating_point_v<T> )
		{
			if( !std::isfinite(value) )
				return false;

			if constexpr( std::is_same_v<T, float> )
				out.SetDouble(ShortestDouble(value));
			else
				out.SetDouble(static_cast<double>(value));
		}
		else if constexpr( std::is_signed_v<T> )
		{
			if constexpr( sizeof(T) <= 4 )
				out.SetInt(static_cast<int32>(value));
			else
				out.SetInt64(static_cast<int64>(value));
		}
		else
		{
			if constexpr( sizeof(T) <= 4 )
				out.SetUint(static_cast<uint32>(value));
			else
				out.SetUint64(static_cast<uint64>(value));
		}
		return true;
	}

	//***************************************************************************
	// @brief float을 다시 float로 읽었을 때 같은 값이 되는 가장 짧은 십진 표현의 double로 바꿉니다.
	// @detail 그냥 double로 넓히면 0.1f가 0.10000000149011612로 기록된다. std::to_chars는 왕복되는 가장 짧은 표현을
	//          로케일과 무관하게 한 번에 만들어 주므로(0.1f -> "0.1"), 이를 double로 읽어 쓴다.
	// @param value 변환할 float 값
	// @return float로 되돌려 읽으면 같은 값이 되는 가장 짧은 십진 표현의 double
	//***************************************************************************
	static double ShortestDouble(float value)
	{
		char buffer[64];
		const auto r = std::to_chars(buffer, buffer + sizeof(buffer), value);
		double result = static_cast<double>(value);
		if( r.ec == std::errc() )
		{
			const auto p = std::from_chars(buffer, r.ptr, result);
			if( p.ec != std::errc() )
				result = static_cast<double>(value);
		}
		return result;
	}

	//***************************************************************************
	// @brief 키의 값을 설정합니다. 이미 있으면 멤버 순서를 유지한 채 값만 교체합니다.
	// @param key 기록할 키 이름
	// @param value 설정할 값 (이동됩니다)
	//***************************************************************************
	void Set(const TCHAR* key, _tValue& value)
	{
		_tDocument& doc = _json.GetDocument();

		auto it = doc.FindMember(key);
		if( it != doc.MemberEnd() )
		{
			it->value = value;		// rapidjson의 대입은 이동(원본은 null이 됨)
			return;
		}

		_tValue name(key, static_cast<rapidjson::SizeType>(_tcslen(key)), doc.GetAllocator());
		doc.AddMember(name, value, doc.GetAllocator());
	}

private:
	CRapidJSONUtil&				_json;	// 값을 기록할 JSON 문서 (외부 소유)
};

#endif // ndef UC_JSONCONFIGFIELDWRITER_H
