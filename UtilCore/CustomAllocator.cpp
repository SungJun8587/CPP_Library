
//***************************************************************************
// CustomAllocator.cpp : implementation of custom containers where stl container memory allocator(malloc/free) 
//				has been replaced by thread caching memory allocator(tcmalloc).
//
//***************************************************************************

#include "pch.h"
#include "CustomAllocator.h"

//***************************************************************************
//
void* BaseAllocator::Alloc(int32 size)
{
	return tc_new(size);
}

//***************************************************************************
//
void BaseAllocator::Release(void* ptr)
{
	tc_delete(ptr);
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
	return gpMemory->Allocate(size);
}

//***************************************************************************
//
void PoolAllocator::Release(void* ptr)
{
	gpMemory->Release(ptr);
}