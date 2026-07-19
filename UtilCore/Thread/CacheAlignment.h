
//***************************************************************************
//  CacheAlignment.h : 캐시라인 정렬 관련 공용 상수 및 유틸리티
//
//  락, 원자 변수 배열 등 여러 동시성 프리미티브가 공통으로 필요로 하는
//  "캐시라인 크기"와 "캐시라인 단위 패딩"을 한 곳에서 제공한다.
//  특정 락 구현에 종속되지 않으므로 SpinLock.h와 별도로 분리했다.
//***************************************************************************

#ifndef __CACHEALIGNMENT_H__
#define __CACHEALIGNMENT_H__

#pragma once

#include <atomic>
#include <cstddef>

//***************************************************************************
//  Platform: cache line size
//***************************************************************************
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
#include <new>
inline constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kCacheLineSize = 64;
#endif

//***************************************************************************
//  CachePaddedAtomic
//  서로 다른 원소를 서로 다른 스레드가 동시에 건드리는 atomic<T> 배열에서,
//  인접 원소끼리 캐시라인을 공유해 false sharing이 발생하는 것을 막는다
//  (예: 커넥션 풀의 슬롯별 참조 카운트 배열).
//***************************************************************************
template <typename T>
struct alignas(kCacheLineSize) CachePaddedAtomic
{
    static_assert(sizeof(std::atomic<T>) <= kCacheLineSize,
        "atomic<T> exceeds the configured cache line size");

    std::atomic<T> value{};
    char _padding[kCacheLineSize - sizeof(std::atomic<T>)]{};
};

template <typename T>
inline constexpr bool kCachePaddedAtomicSizeCheck =
sizeof(CachePaddedAtomic<T>) == kCacheLineSize;

#endif // ndef __CACHEALIGNMENT_H__