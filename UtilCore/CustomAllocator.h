
//***************************************************************************
// CustomAllocator.h : Interface for custom containers where stl container memory allocator(malloc/free) 
//				has been replaced by thread caching memory allocator(tcmalloc).
//
//***************************************************************************

#ifndef __CUSTOMALLOCATOR_H__
#define __CUSTOMALLOCATOR_H__

#pragma once

#ifdef _DEBUG
#pragma comment(lib, "libtcmalloc_minimal64D.lib")  
#else
#pragma comment(lib, "libtcmalloc_minimal64.lib")
#endif

#include <gperftools/tcmalloc.h>

class BaseAllocator
{
public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);
};

class StompAllocator
{
	enum
	{
		PAGE_SIZE = 0x1000
	};

public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);
};

class PoolAllocator
{
public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);
};

template<class _TMain>
class CCustomAllocator
{
	// generic allocator for objects of class _Ty
public:
	//typedef CCustomAllocator<_TMain> other;

	//typedef _TMain value_type;
	using value_type = _TMain;

	CCustomAllocator()
	{
		// construct default allocator (do nothing)
	}

	CCustomAllocator(const CCustomAllocator<_TMain>&)
	{
		// construct by copying (do nothing)
	}

	template<class _TOther>
	CCustomAllocator(const CCustomAllocator<_TOther>&)
	{
		// construct from a related allocator (do nothing)
	}

	template<class _TOther>
	CCustomAllocator<_TMain>& operator=(const CCustomAllocator<_TOther>&)
	{
		// assign from a related allocator (do nothing)
		return (*this);
	}

	_TMain* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(_TMain));
		return (_TMain * )tc_new(size);
	}

	void deallocate(_TMain* ptr, size_t count)
	{
		tc_delete(ptr);
	}
};

#endif // ndef __CUSTOMALLOCATOR_H__