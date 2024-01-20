#pragma once
#include <cstdint>

template <typename IntType>
struct Counter {
    IntType value = 0;
    operator uint64_t() const {
        return value;
    }
    IntType operator++() {
        value++;
        return value;
    }
    IntType operator++(int) {
        IntType tmp = value;
        value++;
        return tmp;
    }
};