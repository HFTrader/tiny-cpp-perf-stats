#include <cstdint>
#include "emmintrin.h"

template< uint32_t N, uint32_t M >
struct Round {
    static constexpr unsigned Value = N>0 ? ((N-1)/M+1)*M : M;
};


template< typename T >
class SelectiveCacheVector
{
public:

    static constexpr unsigned ElSize = Round<sizeof(T),16>::Value;

    SelectiveCacheVector() {}
    SelectiveCacheVector( uint32_t size ) {}
    ~SelectiveCacheVector() {}
};
