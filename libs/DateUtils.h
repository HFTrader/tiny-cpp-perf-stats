#pragma once

#include <ctime>

// Returns time in nanoseconds from epoch
static inline double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double secs = ts.tv_sec + double(ts.tv_nsec) * 1E-9;
    return secs;
}
