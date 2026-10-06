
//***************************************************************************
// ScopeExit.h : interface for the CScopeExit class.
//
//***************************************************************************

#ifndef UC_SCOPEEXIT_H
#define UC_SCOPEEXIT_H

#include <utility>

//***************************************************************************
// @class CScopeExit
// @brief 스코프를 벗어날 때(모든 return 경로, 예외 포함) 등록한 함수를 1회 실행하는 가드.
// @details
// 사용 예:
//      auto release = CScopeExit([this]() { Release(); });
//
// Dismiss()를 호출하면 소멸 시 함수를 실행하지 않습니다(성공 경로에서 정리를 취소할 때).
// 등록한 함수는 예외를 던지면 안 됩니다 — 소멸자에서 실행되므로 던지면 std::terminate로 이어집니다.
//***************************************************************************
template<typename TFunc>
class CScopeExit
{
public:
	explicit CScopeExit(TFunc func) : _func(std::move(func)) {}
	~CScopeExit()
	{
		if( _active )
			_func();
	}

	CScopeExit(const CScopeExit&) = delete;
	CScopeExit& operator=(const CScopeExit&) = delete;

	void Dismiss() noexcept { _active = false; }

private:
	TFunc	_func;
	bool	_active = true;
};

#endif // ndef UC_SCOPEEXIT_H