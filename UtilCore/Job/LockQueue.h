
//***************************************************************************
// LockQueue.h : interface for the CLockQueue class.
//
//***************************************************************************

#ifndef __LOCKQUEUE_H__
#define __LOCKQUEUE_H__

#pragma once

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

template<typename T>
class CLockQueue
{
public:
	void Push(T item)
	{
		WRITE_LOCK;
		// 인자로 받은 item을 rvalue로 전환하여 큐에 효율적으로 삽입
		_items.push(std::move(item));
	}

	T Pop()
	{
		WRITE_LOCK;
		if( _items.empty() )
			return T();

		T ret = std::move(_items.front());
		_items.pop();
		return ret;
	}

	// Containers.h에 정의된 커스텀 할당기 기반 CVector를 사용하도록 OUT 인자 타입을 변경합니다.
	void PopAll(OUT CVector<T>& items)
	{
		WRITE_LOCK; // 단 한 번만 락을 잡고 내부에서 루프를 돌려 쏟아냅니다.

		// 메모리 재할당 비용을 줄이기 위해 컨테이너 크기 미리 확보
		items.reserve(items.size() + _items.size());

		while( !_items.empty() )
		{
			items.push_back(std::move(_items.front()));
			_items.pop();
		}
	}

	void Clear()
	{
		WRITE_LOCK;
		// Containers.h의 CQueue<T>를 사용하여 스왑 처리를 수행합니다.
		CQueue<T> emptyQueue;
		std::swap(_items, emptyQueue);
	}

	// 💡 팁: 단순 크기나 비어있는지 확인하는 것은 ReadLock으로 처리하여 
	// 멀티스레드 조회 성능을 극대화할 수 있습니다.
	bool Empty() const
	{
		READ_LOCK;
		return _items.empty();
	}

	size_t Size() const
	{
		READ_LOCK;
		return _items.size();
	}

private:
	USE_LOCK; // 내부적으로 _lock 객체가 자동 생성됩니다.

	// 일반 Queue 대신 Containers.h에 선언된 커스텀 CQueue를 사용하여 메모리 오버라이딩을 적용합니다.
	CQueue<T> _items;
};

#endif // ndef __LOCKQUEUE_H__