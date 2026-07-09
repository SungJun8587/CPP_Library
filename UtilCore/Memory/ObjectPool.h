
//***************************************************************************
// ObjectPool.h : Generic Object Pool Template for High-Performance Allocation
//
//***************************************************************************

#ifndef __OBJECTPOOL_H__
#define __OBJECTPOOL_H__

#pragma once

#ifndef	__MEMORYPOOL_H__
#include <Memory/MemoryPool.h>
#endif

template<typename Type>
class CObjectPool
{
public:
	template<typename... Args>
	static Type* Pop(Args&&... args)
	{
#ifdef _STOMP
		MemoryHeader* ptr = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(s_allocSize));
		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(ptr, s_allocSize));
#else
		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(s_pool.Pop(), s_allocSize));
#endif		
		new(memory)Type(forward<Args>(args)...); // placement new
		return memory;
	}

	static void Push(Type* obj)
	{
		obj->~Type();
#ifdef _STOMP
		StompAllocator::Release(MemoryHeader::DetachHeader(obj));
#else
		s_pool.Push(MemoryHeader::DetachHeader(obj));
#endif
	}

	template<typename... Args>
	static shared_ptr<Type> MakeShared(Args&&... args)
	{
		shared_ptr<Type> ptr = { Pop(forward<Args>(args)...), Push };
		return ptr;
	}

private:
	static int32		s_allocSize;
	static CMemoryPool	s_pool;
};

template<typename Type>
int32 CObjectPool<Type>::s_allocSize = sizeof(Type) + sizeof(MemoryHeader);

template<typename Type>
CMemoryPool CObjectPool<Type>::s_pool{ s_allocSize };

#endif // ndef __OBJECTPOOL_H__