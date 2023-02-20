#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

#if (defined(__cpp_lib_int_pow2) && __cpp_lib_int_pow2 >= 202002L) || \
    (defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L)
#include <bit>
#else
namespace std {
template <typename IntType>
IntType bit_width(IntType value) {
    IntType l = 0;
    while ((value >> l) > 1) ++l;
    return l + 1;
}

#ifdef __GNUC__
template <>
uint32_t bit_width(uint32_t value) {
    return 32 - __builtin_clz(value);
}

template <>
uint64_t bit_width(uint64_t value) {
    return 64 - __builtin_clzl(value);
}
#endif
}  // namespace std
#endif

template <int BITS, typename ValueType = size_t>
ValueType ilog2(ValueType num) {
    static constexpr ValueType MAXNUM = (ValueType(1) << BITS);
    static constexpr ValueType MASK = MAXNUM - 1;
    if (num < MAXNUM) return num;
    size_t bits = std::bit_width(num);
    size_t off = (num >> (bits - BITS - 1)) & MASK;
    size_t main = (bits - BITS) << BITS;
    size_t res = off + main;
    return res;
}

template <typename ValueType>
struct Range {
    ValueType base;
    ValueType range;
};

template <int BITS, typename ValueType = size_t>
Range<ValueType> irange2(ValueType bin) {
    static constexpr ValueType MAXNUM = (ValueType(1) << BITS);
    static constexpr ValueType MASK = MAXNUM - 1;
    if (bin < MAXNUM) return {bin, 1};
    uint32_t bits = ((bin + (ValueType(BITS - 1) << BITS)) >> BITS);
    uint64_t offset = bin & MASK;
    ValueType base = (MAXNUM + offset) << (bits - BITS);
    ValueType range = (1ULL << (bits - BITS));
    return {base, range};
}

static uint64_t asfp64(uint64_t num) {
    double a = num;
    uint64_t r;
    memcpy(&r, &a, sizeof r);
    return r;
}

template <int BITS>
uint64_t flog2(uint64_t num) {
    constexpr size_t MAXNUM = (size_t(1) << BITS);
    constexpr size_t FP64_MANT_BITS = 52;
    constexpr size_t FP64_EXPO_BIAS = 1023;
    // this will cut one branch off
    if (num >> 55 != 0) __builtin_unreachable();
    if (num <= MAXNUM) return num;
    size_t u64 = asfp64(num);
    return (u64 >> (FP64_MANT_BITS - BITS)) - ((FP64_EXPO_BIAS - 1 + BITS) << BITS);
}

/** Computes the partial log2 of an integer using unions.
 * This method is typically slower but fancier to the eyes.
 */
template <int BITS>
uint64_t uflog2(uint64_t num) {
    constexpr uint64_t MAXNUM = (uint64_t(1) << BITS);
    if (num < MAXNUM) return num;
    union FP {
        double dbl;
        struct {
            uint64_t man : 52;
            uint32_t exp : 11;
            uint32_t sign : 1;
        };
        struct {
            uint64_t xman : 52 - BITS;
            uint32_t xexp : 11 + BITS;
            uint32_t xsgn : 1;
        };
    };
    FP fp;
    fp.dbl = num;
    fp.exp -= 1023 - 1 + BITS;
    return fp.xexp;
}