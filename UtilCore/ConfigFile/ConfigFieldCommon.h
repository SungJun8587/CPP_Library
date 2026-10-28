
//***************************************************************************
// ConfigFieldCommon.h : shared helpers of the config field readers/writers (JSON / XML).
//
//***************************************************************************

#ifndef UC_CONFIGFIELDCOMMON_H
#define UC_CONFIGFIELDCOMMON_H

#include <ConfigFile/ConfigFieldSpec.h>

#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace ConfigField
{
	// 템플릿 인자 추론에서 제외하기 위한 도구 (범위 인자가 대상 변수 타입으로만 결정되도록)
	template <typename T> struct Identity { using type = T; };

	template <typename T>
	inline constexpr bool IsNumeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

	// value_type이 숫자/bool/문자열인 컨테이너인지 (value_type이 없는 타입에서는 조용히 false)
	template <typename C, typename = void>
	struct ScalarListTrait : std::false_type {};

	template <typename C>
	struct ScalarListTrait<C, std::void_t<typename C::value_type>>
		: std::bool_constant<!std::is_same_v<C, _tstring> &&
			(IsNumeric<typename C::value_type> || std::is_same_v<typename C::value_type, bool> || std::is_same_v<typename C::value_type, _tstring>)> {};

	template <typename C>
	inline constexpr bool IsScalarList = ScalarListTrait<C>::value;

	// 범위를 생략했을 때 쓰는 기본 범위 (숫자는 타입 전체 범위, 그 외는 쓰이지 않는 값)
	template <typename E> E DefaultMin()
	{
		if constexpr( IsNumeric<E> ) return (std::numeric_limits<E>::lowest)();
		else return E{};
	}

	template <typename E> E DefaultMax()
	{
		if constexpr( IsNumeric<E> ) return (std::numeric_limits<E>::max)();
		else return E{};
	}

	// 오류 사유 작성. 항상 false를 돌려주므로 "return Describe(...)"로 쓴다.
	inline bool Describe(TCHAR* why, size_t whyCount, const TCHAR* text)
	{
		_tcsncpy_s(why, whyCount, text, _TRUNCATE);
		return false;
	}

	inline bool Describe(TCHAR* why, size_t whyCount, const TCHAR* text, double value)
	{
		_sntprintf_s(why, whyCount, _TRUNCATE, _T("%s: %g"), text, value);
		return false;
	}

	inline bool DescribeRange(TCHAR* why, size_t whyCount, int64 minV, int64 maxV, int64 value)
	{
		_sntprintf_s(why, whyCount, _TRUNCATE, _T("out of range (%lld..%lld): %lld"), static_cast<long long>(minV), static_cast<long long>(maxV), static_cast<long long>(value));
		return false;
	}

	inline bool DescribeRangeU(TCHAR* why, size_t whyCount, uint64 minV, uint64 maxV, uint64 value)
	{
		_sntprintf_s(why, whyCount, _TRUNCATE, _T("out of range (%llu..%llu): %llu"), static_cast<unsigned long long>(minV), static_cast<unsigned long long>(maxV), static_cast<unsigned long long>(value));
		return false;
	}

	// SFieldSpec(Int/UInt)의 범위 검사. UInt는 int64 최대값을 넘으면 범위 밖으로 본다.
	inline bool InSpecRange(const SFieldSpec& spec, int64 value)
	{
		return value >= spec.minV && value <= spec.maxV;
	}

	inline bool InSpecRangeU(const SFieldSpec& spec, uint64 value)
	{
		return value <= static_cast<uint64>((std::numeric_limits<int64>::max)()) && InSpecRange(spec, static_cast<int64>(value));
	}

	// from을 to로 교체한다. (Windows는 쓰기 완료를 기다린다)
	inline bool ReplaceFile(const TCHAR* from, const TCHAR* to)
	{
#ifdef _WIN32
		return ::MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
		return std::rename(from, to) == 0;
#endif
	}

	inline void RemoveFile(const TCHAR* path)
	{
#ifdef _WIN32
		::DeleteFile(path);
#else
		std::remove(path);
#endif
	}
}

//***************************************************************************
// @class CConfigErrorLog
// @brief Reader/Writer가 공유하는 오류 기록. "'키' 사유" 형태로 쌓고 한꺼번에 로그로 남긴다.
//***************************************************************************
class CConfigErrorLog
{
public:
	bool HasErrors() const { return !_errors.empty(); }

protected:
	// 오류를 기록하고 false를 돌려준다.
	bool Fail(const TCHAR* key, const TCHAR* reason)
	{
		_errors.push_back(_tstring(_T("'")) + key + _T("' ") + reason);
		return false;
	}

	void LogErrors(const TCHAR* title) const
	{
		for( const _tstring& error : _errors )
			LOG_ERROR(_T("[Config] %s. %s"), title, error.c_str());
	}

protected:
	std::vector<_tstring>		_errors;
};

#endif // ndef UC_CONFIGFIELDCOMMON_H
