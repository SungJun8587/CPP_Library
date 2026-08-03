
//***************************************************************************
// SpinLockQueue.h : interface for the CSpinLockQueue class.
//
//***************************************************************************

#ifndef __SPINLOCKQUEUE_H__
#define __SPINLOCKQUEUE_H__

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __SPINLOCK_H__
#include <Thread/SpinLock.h>
#endif

#include <atomic>

template<typename T>
class CSpinLockQueue
{
public:
	void Push(T item)
	{
		SPIN_LOCK;

		// 인자로 받은 item을 rvalue로 전환하여 큐에 효율적으로 삽입
		_items.push(std::move(item));
		_size.fetch_add(1, std::memory_order_relaxed);
	}
	
	T Pop()
	{
		SPIN_LOCK;

		if( _items.empty() )
			return T();
		T ret = std::move(_items.front());
		_items.pop();
		_size.fetch_sub(1, std::memory_order_relaxed);
		return ret;
	}

	// Containers.h에 정의된 커스텀 할당기 기반 CVector를 사용하도록 OUT 인자 타입을 변경합니다.
	void PopAll(OUT CVector<T>& items)
	{
		SPIN_LOCK; // 단 한 번만 락을 잡고 내부에서 루프를 돌려 쏟아냅니다.

		// 메모리 재할당 비용을 줄이기 위해 컨테이너 크기 미리 확보
		items.reserve(items.size() + _items.size());
		while( !_items.empty() )
		{
			items.push_back(std::move(_items.front()));
			_items.pop();
		}
		_size.store(0, std::memory_order_relaxed);
	}

	void Clear()
	{
		SPIN_LOCK;

		// Containers.h의 CQueue<T>를 사용하여 스왑 처리를 수행합니다.
		CQueue<T> emptyQueue;
		std::swap(_items, emptyQueue);
		_size.store(0, std::memory_order_relaxed);
	}
	
	// 원자적 카운터로 조회하므로 락 진입 자체가 없습니다.
	bool Empty() const
	{
		return _size.load(std::memory_order_relaxed) == 0;
	}

	size_t Size() const
	{
		return static_cast<size_t>(_size.load(std::memory_order_relaxed));
	}

private:
	SPIN_USE_LOCK; // 내부적으로 _lock 객체가 자동 생성됩니다. (LightWeight 프리셋, 배타 스핀락)
	// 일반 Queue 대신 Containers.h에 선언된 커스텀 CQueue를 사용하여 메모리 오버라이딩을 적용합니다.
	CQueue<T> _items;
	std::atomic<int64_t> _size{ 0 }; // Empty()/Size()를 락 없이 조회하기 위한 카운터
};

#endif // ndef __SPINLOCKQUEUE_H__