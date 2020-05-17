
#include "PerfCounter.h"
#include <linux/perf_event.h>

int main( int argc, char* argv[] )
{
    PerfCounter pc( PERF_COUNT_HW_CACHE_MISSES );

}
