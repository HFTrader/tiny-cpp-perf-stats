#pragma once

#include <cstdint>

#if defined(__GNUC__) && defined(__x86_64__)

#include <x86intrin.h>

std::uint64_t tic() {
    return __rdtsc();
}

static inline void mfence()
{
    _mm_mfence();
}

static inline void disable_reorder()
{
    asm volatile("" ::: "memory");
}

#else

#include <chrono>
#include <ctime>

static inline void mfence()
{
}

static inline void disable_reorder()
{
}

std::uint64_t tic() {
    std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    return nsecs.count();
}

#endif

template< typename Fn, typename HistT >
void timeit( HistT& hist, const std::uint32_t loops, Fn&& fn )
{
    for ( std::uint32_t j=0; j<loops; ++j ) {
        std::uint64_t t0 = tic();
        mfence();
        disable_reorder();
        std::uint32_t count = fn();
        mfence();
        disable_reorder();
        std::uint64_t t1 = tic();
        hist.add( double(t1-t0)/count );
    }
}

template< typename Fn, typename HistT >
double timeit( HistT& hist, Fn&& fn )
{
    mfence();
    std::uint64_t t0 = tic();
    disable_reorder();
    std::uint32_t count = fn();
    disable_reorder();
    mfence();
    std::uint64_t t1 = tic();
    double elapsed = double(t1)-double(t0);
    hist.add( elapsed/count );
    return elapsed;
}
