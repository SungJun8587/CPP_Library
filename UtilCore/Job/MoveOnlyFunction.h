
//***************************************************************************
// MoveOnlyFunction.h : interface for the CMoveOnlyFunction class.
//
//***************************************************************************

#ifndef UC_MOVEONLYFUNCTION_H
#define UC_MOVEONLYFUNCTION_H

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

template <typename TSignature, size_t kCapacity = 48>
class CMoveOnlyFunction;

//***************************************************************************
// @class CMoveOnlyFunction
// @brief 이동 전용(move-only) 함수 객체. 캡처가 kCapacity 이하이면 힙 할당 없이 내부 버퍼에 저장한다.
// @details std::function과 달리 복사 가능성을 요구하지 않으므로 unique_ptr 등 이동 전용 캡처를 담을 수 있고,
//          작은 람다(패킷 한 건 처리 등)는 할당 없이 큐에 들어간다.
//          kCapacity를 넘거나, 이동 생성자가 noexcept가 아니거나, 정렬 요구가 크면 힙에 저장한다.
//          스레드 안전하지 않다. 하나의 객체는 한 스레드가 소유한다(큐를 통해 소유권을 넘긴다).
// @tparam R         반환 타입
// @tparam Args      인자 타입들
// @tparam kCapacity 내부 버퍼 크기(바이트)
//***************************************************************************
template <typename R, typename... Args, size_t kCapacity>
class CMoveOnlyFunction<R(Args...), kCapacity>
{
	static_assert(kCapacity >= sizeof(void*), "kCapacity must hold a pointer");

public:
	CMoveOnlyFunction() noexcept = default;
	CMoveOnlyFunction(std::nullptr_t) noexcept {}

	template <typename F,
		typename D = std::decay_t<F>,
		typename = std::enable_if_t<!std::is_same<D, CMoveOnlyFunction>::value&&
		std::is_invocable_r<R, D&, Args...>::value>>
		CMoveOnlyFunction(F&& fn)
	{
		Emplace<D>(std::forward<F>(fn));
	}

	CMoveOnlyFunction(const CMoveOnlyFunction&) = delete;
	CMoveOnlyFunction& operator=(const CMoveOnlyFunction&) = delete;

	CMoveOnlyFunction(CMoveOnlyFunction&& other) noexcept
	{
		MoveFrom(other);
	}

	CMoveOnlyFunction& operator=(CMoveOnlyFunction&& other) noexcept
	{
		if( this != &other )
		{
			Reset();
			MoveFrom(other);
		}
		return *this;
	}

	CMoveOnlyFunction& operator=(std::nullptr_t) noexcept
	{
		Reset();
		return *this;
	}

	~CMoveOnlyFunction()
	{
		Reset();
	}

	//***************************************************************************
	// @brief 호출 가능한 대상이 들어 있는지 반환합니다.
	// @return 비어 있지 않으면 true
	//***************************************************************************
	explicit operator bool() const noexcept { return _ops != nullptr; }

	//***************************************************************************
	// @brief 저장된 대상이 힙 할당 없이 내부 버퍼에 들어 있는지 반환합니다. (진단/테스트용)
	// @return 인라인 저장이면 true, 비어 있거나 힙에 저장되었으면 false
	//***************************************************************************
	bool IsStoredInline() const noexcept { return _ops != nullptr && _ops->isInline; }

	//***************************************************************************
	// @brief 저장된 대상을 호출합니다. 비어 있으면 안 됩니다.
	//***************************************************************************
	R operator()(Args... args)
	{
		return _ops->invoke(Storage(), std::forward<Args>(args)...);
	}

	//***************************************************************************
	// @brief 저장된 대상을 파괴하고 비웁니다.
	//***************************************************************************
	void Reset() noexcept
	{
		if( _ops != nullptr )
		{
			_ops->destroy(Storage());
			_ops = nullptr;
		}
	}

private:
	struct SOps
	{
		R(*invoke)(void* storage, Args&&... args);
		void(*relocate)(void* dst, void* src) noexcept;		// src의 대상을 dst로 옮기고 src는 비운다
		void(*destroy)(void* storage) noexcept;
		bool isInline;										// 내부 버퍼에 직접 저장되었는지 여부
	};

	template <typename D>
	static constexpr bool IsInline()
	{
		return sizeof(D) <= kCapacity
			&& alignof(D) <= alignof(std::max_align_t)
			&& std::is_nothrow_move_constructible<D>::value;
	}

	template <typename D, bool bInline = IsInline<D>()>
	struct SOpsFor;

	// 내부 버퍼에 직접 저장
	template <typename D>
	struct SOpsFor<D, true>
	{
		static R Invoke(void* storage, Args&&... args)
		{
			return (*static_cast<D*>(storage))(std::forward<Args>(args)...);
		}

		static void Relocate(void* dst, void* src) noexcept
		{
			::new (dst) D(std::move(*static_cast<D*>(src)));
			static_cast<D*>(src)->~D();
		}

		static void Destroy(void* storage) noexcept
		{
			static_cast<D*>(storage)->~D();
		}

		static constexpr SOps kOps = { &Invoke, &Relocate, &Destroy, true };
	};

	// 힙에 저장하고 버퍼에는 포인터만 둔다
	template <typename D>
	struct SOpsFor<D, false>
	{
		static D*& Ptr(void* storage) { return *static_cast<D**>(storage); }

		static R Invoke(void* storage, Args&&... args)
		{
			return (*Ptr(storage))(std::forward<Args>(args)...);
		}

		static void Relocate(void* dst, void* src) noexcept
		{
			::new (dst) D* (Ptr(src));
			Ptr(src) = nullptr;
		}

		static void Destroy(void* storage) noexcept
		{
			delete Ptr(storage);
		}

		static constexpr SOps kOps = { &Invoke, &Relocate, &Destroy, false };
	};

	template <typename D, typename F>
	void Emplace(F&& fn)
	{
		if constexpr( IsInline<D>() )
			::new (Storage()) D(std::forward<F>(fn));
		else
			::new (Storage()) D* (new D(std::forward<F>(fn)));

		_ops = &SOpsFor<D>::kOps;
	}

	void MoveFrom(CMoveOnlyFunction& other) noexcept
	{
		if( other._ops != nullptr )
		{
			other._ops->relocate(Storage(), other.Storage());
			_ops = other._ops;
			other._ops = nullptr;
		}
	}

	void* Storage() noexcept { return _buffer; }

	alignas(std::max_align_t) unsigned char _buffer[kCapacity];		// 인라인 저장소 (또는 힙 포인터)
	const SOps* _ops = nullptr;										// 현재 저장된 타입의 동작 테이블 (비었으면 nullptr)
};

#endif // ndef UC_MOVEONLYFUNCTION_H