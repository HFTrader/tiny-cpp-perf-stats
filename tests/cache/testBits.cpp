#include "BitUtils.h"
#include <cstdint>
#include <cstdio>

int main() {
    for (size_t num = 0; num < 64; ++num) {
        printf("%3ld:  ", num);
        printf("%3ld %3ld/%-3ld   ", ilog2<0>(num), irange2<0>(ilog2<0>(num)).base,
               irange2<0>(ilog2<0>(num)).range);
        printf("%3ld %3ld/%-3ld   ", ilog2<1>(num), irange2<1>(ilog2<1>(num)).base,
               irange2<1>(ilog2<1>(num)).range);
        printf("%3ld %3ld/%-3ld   ", ilog2<2>(num), irange2<2>(ilog2<2>(num)).base,
               irange2<2>(ilog2<2>(num)).range);
        printf("%3ld %3ld/%-3ld   ", ilog2<3>(num), irange2<3>(ilog2<3>(num)).base,
               irange2<3>(ilog2<3>(num)).range);
        printf(" FP: %3ld %3ld %3ld %3ld\n", flog2<0>(num), flog2<1>(num), flog2<2>(num),
               flog2<3>(num));
    }
}
