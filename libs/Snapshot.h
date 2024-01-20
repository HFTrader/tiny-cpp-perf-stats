#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <limits>
#include <unordered_map>
#include "PerfGroup.h"

class Snapshot {
public:
    struct Metric {
        std::string name;  // the name of the metric
        double value;      // the value of the metric in this sample
        int group;         // to create alternates like O(1) or O(logN) or O(N)
        bool global;       // does this apply to all tests globally?
    };
    struct Sample : std::vector<Metric> {
        double operator[](const std::string &name) const {
            return get(name);
        }
        double get(const std::string &name) const {
            for (const Metric &s : *this) {
                if (s.name == name) return s.value;
            }
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    Snapshot();
    Snapshot(const std::vector<std::string> &pmc);
    ~Snapshot();
    void start();
    Sample stop(const std::string &evname, uint64_t numitems, uint64_t numrep);
    void summary(const std::string &header, FILE *fout = stdout);

private:
    std::unordered_map<std::string, int> events;
    PerfGroup counters;
    using SampleVector = std::vector<Sample>;
    using SampleMap = std::unordered_map<std::string, SampleVector>;
    SampleMap samples;
    int debug;
};
