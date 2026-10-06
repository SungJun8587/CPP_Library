
//***************************************************************************
// SyncValue.h : interface for the CSyncValue class.
//
//***************************************************************************

#ifndef UC_SYNCVALUE_H
#define UC_SYNCVALUE_H

#include <mutex>
#include <utility>

//***************************************************************************
// @class CSyncValue
// @brief 뮤텍스로 보호되는 값 하나. 설정 스레드와 읽는 스레드가 다른 설정값용입니다.
// @details
// Get()은 복사본을 반환하므로 호출 측이 락 없이 안전하게 사용할 수 있습니다.
// 값이 없을 수 있는 설정은 CSyncValue<std::optional<T>>로 표현합니다.
// 값 하나를 읽고 쓰는 용도 전용이며, 여러 값을 원자적으로 함께 바꿔야 하면 별도의 락이 필요합니다.
//***************************************************************************
template<typename T>
class CSyncValue
{
public:
	CSyncValue() = default;
	explicit CSyncValue(T value) : _value(std::move(value)) {}

	CSyncValue(const CSyncValue&) = delete;
	CSyncValue& operator=(const CSyncValue&) = delete;

	void Set(const T& value)
	{
		std::lock_guard<std::mutex> guard(_lock);
		_value = value;
	}

	T Get() const
	{
		std::lock_guard<std::mutex> guard(_lock);
		return _value;
	}

private:
	mutable std::mutex	_lock;
	T					_value{};
};

#endif // ndef UC_SYNCVALUE_H