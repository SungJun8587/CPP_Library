
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
		// ~1024���� 32����, ~2048���� 128����, ~4096���� 256����
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

	// �޸� ũ�� <-> �޸� Ǯ
	// O(1) ������ ã�� ���� ���̺�
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