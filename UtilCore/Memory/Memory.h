
//***************************************************************************
// Memory.h : interface for the CMemory class.
//
//***************************************************************************

#ifndef __MEMORY_H__
#define __MEMORY_H__

#pragma once

#ifndef	__CUSTOMALLOCATOR_H__
#include <Memory/CustomAllocator.h>
#endif

#ifndef	__MEMORYPOOL_H__
#include <Memory/MemoryPool.h>
#endif

class CMemoryPool;

class CMemory
{
	enum
	{
		// ~1024까지 32단위, ~2048까지 128단위, ~4096까지 256단위
		POOL_COUNT = (1024 / 32) + (1024 / 128) + (2048 / 256),
		MAX_ALLOC_SIZE = 4096
	};

public:
	CMemory();
	~CMemory();

	void*	Allocate(int32 size);
	void	Release(void* ptr);

private:
	vector<CMemoryPool*> _pools;

	// 메모리 크기 <-> 메모리 풀
	// O(1) 빠르게 찾기 위한 테이블
	CMemoryPool* _poolTable[MAX_ALLOC_SIZE + 1];
};


template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	new(memory)Type(forward<Args>(args)...); // placement new
	return memory;
}

template<typename Type>
void xdelete(Type* obj)
{
	obj->~Type();
	PoolAllocator::Release(obj);
}

template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
	return shared_ptr<Type>{ xnew<Type>(forward<Args>(args)...), xdelete<Type> };
}

#endif // ndef __MEMORY_H__