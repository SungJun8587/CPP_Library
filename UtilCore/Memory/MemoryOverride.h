
//***************************************************************************
// MemoryOverride.h : Interface for overriding global new/delete operators.
//
//***************************************************************************

#ifndef __MEMORYOVERRIDE_H__
#define __MEMORYOVERRIDE_H__

#pragma once

#ifndef __RAWALLOCATOR_H__
#include <Memory/RawAllocator.h>
#endif

class CMemoryOverride
{
public:
    // virtual 키워드를 제거하여 가상 함수 테이블(vptr) 8바이트 오버헤드를 없앴습니다.
    CMemoryOverride(void) {}
    ~CMemoryOverride(void) {}

    void* operator new(size_t size)
    {
        return RawAllocator::Alloc(size);
    }

    void* operator new[](size_t size)
    {
        return RawAllocator::Alloc(size);
    }

    void operator delete(void* ptr)
    {
        RawAllocator::Free(ptr);
    }

    void operator delete[](void* ptr)
    {
        RawAllocator::Free(ptr);
    }
};

#endif // ndef __MEMORYOVERRIDE_H__