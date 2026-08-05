
#ifndef __QUEUECOMMON_H__
#define __QUEUECOMMON_H__

#include <atomic>
#include <cstddef>
#include <utility>
#include <new>
#include <mutex>
#include <condition_variable>
#include <chrono>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    #include <immintrin.h>
    #define LFQ_CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define LFQ_CPU_PAUSE() __asm__ __volatile__("yield")
#else
    #define LFQ_CPU_PAUSE() ((void)0)
#endif

// LFQ_CACHE_LINE_SIZE: CPU 캐시 라인 크기(64바이트).
// Lock-Free Queue에서 false sharing 방지 및 성능 최적화를 위해
// 원자 변수와 Cell 구조체를 캐시 라인 단위로 정렬하는 데 사용됨.
constexpr std::size_t LFQ_CACHE_LINE_SIZE = 64;


#endif // __QUEUECOMMON_H__