
//***************************************************************************
// RawAllocator.h
//
// 역할 : 프로젝트 내 모든 Allocator 클래스가 공유하는 raw 메모리 할당 계층.
//        컴파일 타임에 정의된 매크로를 기반으로 실제 메모리 할당자
//        라이브러리(mimalloc / jemalloc / tcmalloc / malloc)를 선택하여
//        raw 메모리 할당 및 해제를 수행합니다.
//
// 사용 대상
//   - 메모리 할당이 필요한 모든 Allocator 클래스에서 포함하여 사용합니다.
//   - 직접 호출보다는 각 Allocator 클래스의 인터페이스를 통해 사용하는 것을 권장합니다.
//
// 지원 Allocator (우선순위 순)
//   1. mimalloc  (MIMALLOC_H 정의 시)
//   2. jemalloc  (JEMALLOC_H_ 정의 시)
//   3. tcmalloc  (TCMALLOC_TCMALLOC_H_ 정의 시)
//   4. malloc    (기본값, 위 매크로가 모두 미정의 시)
//
// 주의
//   - allocator 라이브러리 헤더는 pch.h 또는 각 라이브러리 설정 헤더에서
//     먼저 포함되어 있어야 합니다.
//***************************************************************************

#ifndef __RAWALLOCATOR_H__
#define __RAWALLOCATOR_H__

#pragma once

#include <cstdlib> // ::malloc, ::free (기본값 fallback)

namespace RawAllocator
{
    //*************************************************************************
    // Alloc : raw 메모리 할당
    //         size == 0 이면 nullptr 반환 (구현 정의 동작 회피)
    //         할당 실패  : nullptr 반환 (각 라이브러리의 기본 동작을 그대로 노출)
    //*************************************************************************
    inline void* Alloc(size_t size)
    {
#if defined(MIMALLOC_H)
        // Microsoft mimalloc : 성능·보안 균형이 뛰어난 컴팩트 할당자
        return ::mi_malloc(size);

#elif defined(JEMALLOC_H_)
        // jemalloc : 대용량·멀티스레드 워크로드에 최적화
        return ::je_malloc(size);

#elif defined(TCMALLOC_TCMALLOC_H_)
        // TCMalloc : Google, 스레드 캐시 기반 고성능 할당자
        // tc_new(operator new 래퍼)가 아닌 tc_malloc(raw 할당)을 사용
        return ::tc_malloc(size);

#else
        // 표준 C 할당자 (fallback)
        return ::malloc(size);
#endif
    }

    //*************************************************************************
    // Free : raw 메모리 해제
    //        nullptr 전달은 no-op (표준 free 동작과 동일하게 유지)
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

} // namespace RawAllocator

#endif // ndef __RAWALLOCATOR_H__