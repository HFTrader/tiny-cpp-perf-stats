#include "MicroStats.h"
#include <iostream>
#include <random>

#if defined(__GNUC__) && defined(__x86_64__)
#include <x86intrin.h>
uint64_t now() {
    return __rdtsc();
}
#else
#include <chrono>
uint64_t now() {
    using std::chrono;
    return duration_cast<nanoseconds>(
        system_clock::now().time_since_epoch()).count();
}
#endif

template< uint32_t N >
void summary( const MicroStats<N>& ms )
{
    constexpr double sd2 = 0.022750131948178987;
    constexpr double sd1 = 0.15865525393145702;
    std::cout << "10pct:" << ms.percentile(10)
              << " -2SD:" << ms.percentile(100*sd2)
              << " -1SD:" << ms.percentile(100*sd1)
              << " 50pct:" << ms.percentile(50)
              << " +1SD:" << ms.percentile(100-100*sd1)
              << " +2SD:" << ms.percentile(100-200*sd2)
              << " 90pct:" << ms.percentile(90)
              << " 99pct:" << ms.percentile(99)
              << " Stdev:" << (ms.percentile(100-100*sd1) - ms.percentile(100*sd1))/2
              << " Stdev:" << (ms.percentile(100-100*sd2) - ms.percentile(100*sd2))/4
              << std::endl;
}

int main( int argc, char* argv[] )
{
    MicroStats<3> ms;

    // Single point distribution
    for ( uint32_t j=0; j<1000; ++j ) {
        ms.add( 1000 );
    }
    summary( ms );
    ms.clear();

    // Uniform distribution over [0,10000]
    for ( uint32_t j=0; j<10000; ++j ) {
        ms.add( j );
    }
    summary( ms );
    ms.clear();

    // Random distribution
    MicroStats<4> tms;
    std::default_random_engine generator;
    std::normal_distribution<double> distribution(50000.0,2500.0);
    for ( uint32_t j=0; j<10000000; ++j ) {
        double number = distribution(generator);
        if ( number<0 ) number=0;
        uint64_t unumber = ::llrint( number );
        uint64_t t0 = now();
        if ( t0>0 ) {
            ms.add(  unumber );
            uint64_t t1 = now();
            if ( t1>t0 ) tms.add( t1-t0 );
        }
    }
    summary( ms );
    ms.clear();

    std::cout << "Cost of MicroStats (measure+bin)" << std::endl;
    summary( tms );
    tms.clear();


    for ( uint32_t j=0; j<10000000; ++j ) {
        uint64_t t0 = now();
        if ( t0>0 ) {
            //asm __volatile__( "nop" );
            uint64_t t1 = now();
            if ( t1>t0 ) tms.add( t1-t0 );
        }
    }
    std::cout << "Cost of measuring only" << std::endl;
    summary( tms );
}
