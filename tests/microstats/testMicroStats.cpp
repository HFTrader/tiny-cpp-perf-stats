#include "MicroStats.h"
#include <iostream>
#include <random>

#if defined(__GNUC__) && defined(__x86_64__)
#include <x86intrin.h>
uint64_t nowts() {
    return __rdtsc();
}
#else
#include <chrono>
uint64_t nowts() {
    using std::chrono;
    return duration_cast<nanoseconds>(system_clock::nowts().time_since_epoch()).count();
}
#endif

template <uint32_t N>
void summary(const std::string& title, const MicroStats<N>& ms) {
    constexpr double sd2 = 47.71;
    constexpr double sd1 = 34.13;
    std::cout << title << "\n\tPercentiles (1/10/50/90/99): " << ms.percentile(1) << " "
              << ms.percentile(10) << " " << ms.percentile(50) << " " << ms.percentile(90)
              << " " << ms.percentile(99) << " "
              << "\n\tSD(-2/-1/+1/+2):" << ms.percentile(50 - sd2) << " "
              << ms.percentile(50 - sd1) << " " << ms.percentile(50 + sd1) << " "
              << ms.percentile(50 + sd2)
              << "\n\tSd:" << (ms.percentile(50 + sd1) - ms.percentile(50 - sd1)) / 2
              << "\n\t2Sd:" << (ms.percentile(50 + sd2) - ms.percentile(50 - sd2)) / 2
              << '\n';
}

int main(int argc, char* argv[]) {
    using Histogram = MicroStats<6>;
    int bin = Histogram::calcbin(1000);
    auto range = Histogram::calcrange(bin);
    std::cout << "Total number of bins: " << Histogram::NUMBINS << "\n";
    std::cout << "Bin for value 1000: " << bin << "\n";
    std::cout << "Range for bin " << bin << ": " << range.from << "-" << range.to << "\n";

    // Single point distribution
    Histogram ms;
    for (uint32_t j = 0; j < 1000; ++j) {
        ms.add(1000);
    }
    summary("Single Point Distribution at 1,000", ms);
    ms.clear();

    // Uniform distribution over [0,10000]
    for (uint32_t j = 0; j < 10000; ++j) {
        ms.add(j);
    }
    summary("Uniform Distribution Over [0,10,000]", ms);
    ms.clear();

    // Random distribution
    Histogram tms;

    bin = Histogram::calcbin(50000);
    range = Histogram::calcrange(bin);
    std::cout << "Bin for value 50000: " << bin;
    std::cout << "  range: " << range.from << "-" << range.to << "\n";
    bin = Histogram::calcbin(50000 - 2500);
    range = Histogram::calcrange(bin);
    std::cout << "Bin for value 47500: " << bin;
    std::cout << "  range: " << range.from << "-" << range.to << "\n";
    bin = Histogram::calcbin(50000 + 2500);
    range = Histogram::calcrange(bin);
    std::cout << "Bin for value 52500: " << bin;
    std::cout << "  range: " << range.from << "-" << range.to << "\n";

    std::default_random_engine generator;
    std::normal_distribution<double> distribution(50000.0, 2500.0);
    for (uint32_t j = 0; j < 10000000; ++j) {
        double number = distribution(generator);
        if (number < 0) number = 0;
        uint64_t unumber = ::llrint(number);
        uint64_t t0 = nowts();
        if (t0 > 0) {
            ms.add(unumber);
            uint64_t t1 = nowts();
            if (t1 > t0) tms.add(t1 - t0);
        }
    }
    summary("Normal Distribution (u=50,000, s=2,500)", ms);
    ms.clear();

    std::cout << "Cost of MicroStats (measure+bin)" << '\n';
    summary("Microstats Cost", tms);
    tms.clear();

    for (uint32_t j = 0; j < 10000000; ++j) {
        uint64_t t0 = nowts();
        if (t0 > 0) {
            // asm __volatile__( "nop" );
            uint64_t t1 = nowts();
            if (t1 > t0) tms.add(t1 - t0);
        }
    }
    std::cout << "Cost of measuring only" << '\n';
    summary("Measuring Cost Only", tms);
}
