// Regression test for MicroStats bin/range arithmetic across the full 64-bit
// domain and for NDB = 0..6. It guards two corner cases that the demo in
// testMicroStats.cpp never exercises:
//
//   1) calcbin() must never return an index >= NUMBINS. A value with bit 63
//      set lands in the top octave; if NUMBINS is one octave short the index
//      runs past the bins[] array (out-of-bounds write in add()).
//
//   2) calcrange(calcbin(v)) must contain v. For high octaves the sub-bin
//      partition is shifted left by more than 31 bits, so holding it in a
//      32-bit value overflows and corrupts the reconstructed range.
//
// Uses explicit checks and a nonzero exit code rather than assert(), which is
// compiled out in Release builds.

#include "MicroStats.h"
#include <cstdint>
#include <cstdio>

template <uint32_t NDB>
static long check() {
    using M = MicroStats<NDB>;
    long oob = 0, inversion = 0, monotonic = 0;

    auto probe = [&](uint64_t v) {
        uint32_t b = M::calcbin(v);
        if (b >= M::NUMBINS) { ++oob; return; }
        auto r = M::calcrange(b);
        if (!(v >= r.from && v <= r.to)) ++inversion;
    };

    // Dense low range, also checking that the bin index is non-decreasing in v.
    uint32_t prev = 0;
    for (uint64_t v = 1; v < (1ull << 24); ++v) {
        uint32_t b = M::calcbin(v);
        if (b < prev) ++monotonic;
        prev = b;
        probe(v);
    }
    // Sparse sweep across every high octave, including the top one (bit 63),
    // landing on each sub-bin boundary.
    for (int e = 20; e < 64; ++e)
        for (uint64_t d = 0; d < (1u << NDB); ++d)
            probe((1ull << e) + (d << (e > (int)NDB ? e - (int)NDB : 0)));
    probe(~0ull);  // 2^64 - 1, the very top

    long total = oob + inversion + monotonic;
    std::printf("  NDB=%u NUMBINS=%-5u out-of-bounds=%ld inversion=%ld monotonicity=%ld %s\n",
                NDB, M::NUMBINS, oob, inversion, monotonic, total ? "FAIL" : "ok");
    return total;
}

int main() {
    long failures = check<0>() + check<1>() + check<2>() + check<3>() +
                    check<4>() + check<5>() + check<6>();
    if (failures) {
        std::printf("FAILED: %ld total violations\n", failures);
        return 1;
    }
    std::printf("PASSED: bin/range arithmetic valid across NDB 0..6 and the full 64-bit domain\n");
    return 0;
}
