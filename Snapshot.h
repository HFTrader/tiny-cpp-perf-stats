#pragma once

#include <cstdint>
#include <vector>
#include <iostream>
#include <unordered_map>
#include "PerfCounter.h"

class Snapshot
{
public:
    struct Sample
    {
        uint64_t numitems = 0;
        double cycles = 0;
        double instructions = 0;
        double cachemisses = 0;
        double branchmisses = 0;
        double tlbmisses = 0;
    };

    Snapshot(int debug_level = 0);
    ~Snapshot();
    void start();
    Sample stop(const std::string &evname,
                uint64_t numitems,
                uint64_t numrep);
    void summary(const std::string &header, FILE *fout = stdout);

private:
    PerfCounter cycles, instructions, cachemisses, branchmisses, tlbmisses;
    using SampleVec = std::vector<Sample>;
    using SampleMap = std::unordered_map<std::string, SampleVec>;
    SampleMap samples;
    int debug;
};
