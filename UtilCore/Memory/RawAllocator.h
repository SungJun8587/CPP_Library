
//***************************************************************************
// RawAllocator.h
//
// 설명 : 프로젝트 내 모든 Allocator 클래스에서 사용하는 raw 메모리 할당 정의.
//        컴파일 타임에 정의된 매크로를 기반으로 실제 메모리 할당기
//        라이브러리(mimalloc / jemalloc / tcmalloc / malloc)를 선택하여
//        raw 메모리 할당 및 정렬된 할당 기능을 제공합니다.
//
//        상위 계층(BaseAllocator, CMemoryPool, CMemory)은 이 네임스페이스를
//        통해서만 raw 메모리에 접근하므로, 사용할 malloc 구현체를 교체할 때
//        이 파일 하나만 신경 쓰면 됩니다. (분기는 런타임이 아닌 컴파일 타임에
//        매크로로 결정되므로 매 호출마다 오버헤드가 없습니다.)
//***************************************************************************

#ifndef __RAWALLOCATOR_H__
#define __RAWALLOCATOR_H__

#pragma once

#include <cassert>  // assert

// 1. 매크로 정의에 따른 헤더 포함 및 라이브러리 링크
#if defined(USE_MIMALLOC)
    #pragma comment(lib, LIB_NAME("mimalloc")) // 또는 사용하시는 LIB_NAME("mimalloc")
    #include <mimalloc.h>

#elif defined(USE_JEMALLOC)
    #pragma comment(lib, LIB_NAME("jemalloc"))
    #include <jemalloc/jemalloc.h>

#elif defined(USE_TCMALLOC)
    #pragma comment(lib, LIB_NAME("libtcmalloc_minimal"))
    #include <gperftools/tcmalloc.h>

#else
    #include <cstdlib>          // ::malloc, ::free, ::aligned_alloc (C11 fallback)

    #if defined(_WIN32)
        #include <malloc.h>     // ::_aligned_malloc, ::_aligned_free
    #endif
#endif

namespace RawAllocator
{
    //*************************************************************************
    // IsPowerOfTwo
    //
    // 설명 : alignment 값이 2의 거듭제곱인지 확인하는 내부 유틸리티 함수.
    //        정렬 할당 함수들의 전제 조건(alignment는 반드시 2의 거듭제곱)을
    //        검증하는 데 사용됩니다.
    //
    // 매개변수 : alignment - 검사할 정렬 값(바이트 단위)
    // 반환값   : 2의 거듭제곱이면 true, 아니면 false (0도 false)
    //*************************************************************************
    inline bool IsPowerOfTwo(size_t alignment)
    {
        return alignment != 0 && (alignment & (alignment - 1)) == 0;
    }

    //*************************************************************************
    // Alloc
    //
    // 설명 : 정렬 요구사항이 없는 일반 raw 메모리를 할당합니다.
    //        컴파일 타임에 정의된 매크로에 따라 mimalloc / jemalloc /
    //        tcmalloc / 표준 malloc 중 하나로 자동 분기됩니다.
    //
    // 매개변수 : size - 할당할 바이트 크기
    // 반환값   : 할당된 메모리 포인터 (실패 시 nullptr 가능 - 호출부에서 확인 필요)
    //*************************************************************************
    inline void* Alloc(size_t size)
    {
#if defined(MIMALLOC_H)
        return ::mi_malloc(size);

#elif defined(JEMALLOC_H_)
        return ::je_malloc(size);

#elif defined(TCMALLOC_TCMALLOC_H_)
        return ::tc_malloc(size);

#else
        return ::malloc(size);
#endif
    }

    //*************************************************************************
    // Free
    //
    // 설명 : Alloc()으로 할당한 raw 메모리를 해제합니다.
    //        반드시 Alloc()과 동일한 라이브러리 분기로 짝을 맞춰 호출되어야
    //        합니다(매크로가 컴파일 타임에 고정되므로 자동으로 짝이 맞음).
    //
    // 매개변수 : ptr - 해제할 메모리 포인터
    //*************************************************************************
    inline void Free(void* ptr)
    {
#if defined(MIMALLOC_H)
        ::mi_free(ptr);

#elif defined(JEMALLOC_H_)
        ::je_free(ptr);

#elif defined(TCMALLOC_TCMALLOC_H_)
        ::tc_free(ptr);

#else
        ::free(ptr);
#endif
    }

    //*************************************************************************
    // AllocAligned
    //
    // 설명 : 정렬된(aligned) raw 메모리를 할당합니다.
    //        SLIST(Interlocked SList) 등 특정 정렬 조건이 필요한 자료구조에
    //        사용됩니다. alignment는 반드시 2의 거듭제곱이어야 하며,
    //        위반 시 assert로 즉시 알립니다.
    //
    //        라이브러리마다 인자 순서가 달라 이 함수 내부에서 흡수합니다.
    //        (mimalloc: size, alignment / jemalloc·tcmalloc: alignment, size /
    //         Windows: size, alignment / C11: alignment, size)
    //
    // 매개변수 : size      - 할당할 바이트 크기
    //           alignment - 정렬 기준(바이트 단위, 2의 거듭제곱)
    // 반환값   : 할당된 메모리 포인터 (실패 시 nullptr 가능 - 호출부에서 확인 필요)
    //*************************************************************************
    inline void* AllocAligned(size_t size, size_t alignment)
    {
        assert(IsPowerOfTwo(alignment) && "alignment는 2의 거듭제곱이어야 합니다.");

#if defined(MIMALLOC_H)
        // mimalloc은 인자 순서가 (size, alignment)이며, size 제약이 없습니다.
        return ::mi_malloc_aligned(size, alignment);

#elif defined(JEMALLOC_H_)
        // jemalloc은 인자 순서가 (alignment, size)입니다.
        return ::je_aligned_alloc(alignment, size);

#elif defined(TCMALLOC_TCMALLOC_H_)
        // tcmalloc은 인자 순서가 (alignment, size)입니다.
        return ::tc_memalign(alignment, size);

#elif defined(_WIN32)
        // Windows: _aligned_malloc은 (size, alignment) 순서이며 size 제약이 없습니다.
        return ::_aligned_malloc(size, alignment);

#else
        // 표준 C11 fallback
        // aligned_alloc은 size가 alignment의 배수가 아니면 UB이므로 배수로 올림 보정합니다.
        size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
        return ::aligned_alloc(alignment, alignedSize);
#endif
    }

    //*************************************************************************
    // FreeAligned
    //
    // 설명 : AllocAligned()으로 할당한 정렬 메모리를 해제합니다.
    //        플랫폼/라이브러리에 따라 일반 Free와 구분되는 별도 해제 함수가
    //        필요할 수 있어 분리되어 있습니다(예: Windows의 _aligned_free).
    //
    // 매개변수 : ptr - 해제할 메모리 포인터
    //*************************************************************************
    inline void FreeAligned(void* ptr)
    {
#if defined(MIMALLOC_H)
        // mimalloc은 정렬 할당 건도 mi_free로 해제합니다.
        ::mi_free(ptr);

#elif defined(JEMALLOC_H_)
        // jemalloc은 정렬 할당 건도 je_free로 해제합니다.
        ::je_free(ptr);

#elif defined(TCMALLOC_TCMALLOC_H_)
        // tcmalloc은 정렬 할당 건도 tc_free로 해제합니다.
        ::tc_free(ptr);

#elif defined(_WIN32)
        // Windows: _aligned_malloc으로 할당한 메모리는 반드시 _aligned_free로 해제해야 합니다.
        ::_aligned_free(ptr);

#else
        // 표준 C11 fallback: aligned_alloc으로 할당한 메모리는 일반 free로 해제 가능합니다.
        ::free(ptr);
#endif
    }
}

#endif // __RAWALLOCATOR_H__
