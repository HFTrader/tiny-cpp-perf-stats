#pragma once

#include <cstdint>
#include <vector>
#include <iostream>
#include <unordered_map>
#include "PerfCounter.h"


class Snapshot
{
public:
    Snapshot();
    ~Snapshot();
    void start();
    void stop( const std::string& evname,
               uint64_t numitems,
               uint64_t numrep );
    void summary( const std::string& header, FILE* fout = stdout );
private:
    PerfCounter cycles, instructions, cachemisses, branchmisses;
    struct Sample {
        uint64_t numitems;
        uint64_t cycles;
        uint64_t instructions;
        uint64_t cachemisses;
        uint64_t branchmisses;
    };
    using SampleVec = std::vector< Sample >;
    using SampleMap = std::unordered_map< std::string, SampleVec >;
    SampleMap samples;
};
