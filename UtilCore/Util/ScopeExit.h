
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
	//***************************************************************************
	// @brief CScopeExit 생성자
	// @param func 스코프 종료 시 실행할 콜백 함수 (rvalue/lvalue 모두 이동 생성)
	//***************************************************************************
	explicit CScopeExit(TFunc func) : _func(std::move(func)) {}

	//***************************************************************************
	// @brief CScopeExit 소멸자
	// @details 활성화 상태(_active가 true)일 때만 등록된 콜백 함수를 실행합니다.
	//***************************************************************************
	~CScopeExit()
	{
		if( _active )
			_func();
	}

	CScopeExit(const CScopeExit&) = delete;
	CScopeExit& operator=(const CScopeExit&) = delete;

	//***************************************************************************
	// @brief 스코프 종료 시 콜백 실행을 취소
	// @details 정상적으로 작업이 완료되어 정리 작업이 필요 없을 때 호출합니다.
	//***************************************************************************
	void Dismiss() noexcept { _active = false; }

private:
	TFunc	_func;		// 스코프 종료 시 실행할 콜백 함수 객체
	bool	_active = true;	// 콜백 실행 여부 플래그 (Dismiss() 호출 시 false)
};

#endif // ndef UC_SCOPEEXIT_H