
//***************************************************************************
// ObjectPool.h : interface and implementation for the CObjectPool class.
//
//***************************************************************************

#ifndef __OBJECTPOOL_H__
#define __OBJECTPOOL_H__

#pragma once

class CPoolObj
{
public:
	CPoolObj(void) {}
	virtual ~CPoolObj(void) {}

	void* operator new(size_t size)
	{
#ifdef TCMALLOC_TCMALLOC_H_
		return tc_new(size);
#else
	#ifdef JEMALLOC_H_
		return je_malloc(size);
	#endif
#endif
	}

	void* operator new[](size_t size)
	{
#ifdef TCMALLOC_TCMALLOC_H_
		return tc_new(size);
#else
	#ifdef JEMALLOC_H_
		return je_malloc(size);
	#endif
#endif
	}

	void operator delete(void* ptr)
	{
#ifdef TCMALLOC_TCMALLOC_H_
		tc_delete(ptr);
#else	
	#ifdef JEMALLOC_H_
		je_free(ptr);
	#endif
#endif
	}

	void operator delete[](void* ptr)
	{
#ifdef TCMALLOC_TCMALLOC_H_
		tc_delete(ptr);
#else	
	#ifdef JEMALLOC_H_
		je_free(ptr);
	#endif
#endif
	}
};

#endif // ndef __OBJECTPOOL_H__