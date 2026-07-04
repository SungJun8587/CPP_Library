
//***************************************************************************
// CustomAllocator.cpp : implementation of custom containers where stl container memory allocator(malloc/free) 
//				has been replaced by thread caching memory allocator(tcmalloc).
//
//***************************************************************************

#include "pch.h"
#include "CustomAllocator.h"

//*************************************************************************
// Alloc  : size 바이트의 raw 메모리를 할당하여 반환합니다.
//          size == 0 : nullptr 반환
//          할당 실패  : nullptr 반환
//*************************************************************************
void* BaseAllocator::Alloc(int32 size)
{
	if (size == 0)
		return nullptr;

	return RawAllocator::Alloc(size);
}

//*************************************************************************
// Release : Alloc으로 할당된 메모리를 해제합니다.
//           ptr == nullptr : no-op
//*************************************************************************
void BaseAllocator::Release(void* ptr)
{
	if (ptr == nullptr)
		return;

	RawAllocator::Free(ptr);
}

//***************************************************************************
//
void* StompAllocator::Alloc(int32 size)
{
	const int64 pageCount = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	const int64 dataOffset = pageCount * PAGE_SIZE - size;
	void* baseAddress = ::VirtualAlloc(NULL, pageCount * PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return static_cast<void*>(static_cast<int8*>(baseAddress) + dataOffset);
}

//***************************************************************************
//
void StompAllocator::Release(void* ptr)
{
	const int64 address = reinterpret_cast<int64>(ptr);
	const int64 baseAddress = address - (address % PAGE_SIZE);
	::VirtualFree(reinterpret_cast<void*>(baseAddress), 0, MEM_RELEASE);
}

//***************************************************************************
//
void* PoolAllocator::Alloc(int32 size)
{
#ifdef __MEMORY_H__
	return gpMemory->Allocate(size);
#else
	return nullptr;
#endif
}

//***************************************************************************
//
void PoolAllocator::Release(void* ptr)
{
#ifdef __MEMORY_H__
	gpMemory->Release(ptr);
#endif
}
