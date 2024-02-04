#include "Snapshot.h"
#include <iostream>
#include <cmath>

Snapshot::Snapshot(const std::vector<std::string> &pmc) {
    if (!counters.init(pmc)) {
        std::cerr << "Unable to initialize performance counters group" << '\n';
    }
}

Snapshot::Snapshot() {
    std::vector<std::string> counter_names{"cycles", "instructions", "cache-misses",
                                           "branch-misses"};
    if (!counters.init(counter_names)) {
        std::cerr << "Unable to initialize performance counters group" << '\n';
    }
}

Snapshot::~Snapshot() {
}

void Snapshot::start() {
    counters.start();
}

Snapshot::Sample Snapshot::stop(const std::string &event_name, uint64_t numitems,
                                uint64_t numiterations) {
    counters.stop();
    Sample samp;
    if (numiterations > 0) {
        samp.push_back({"Constant", (double)1, 0, false});
        samp.push_back({"N", (double)numitems, 1, false});
        samp.push_back({"N2", double(numitems) * double(numitems), 1, false});
        samp.push_back({"log(N)", (double)log(numitems) / log(10), 1, false});
        for (size_t j = 0; j < counters.size(); ++j) {
            double value = double(counters[j]) / numiterations;
            samp.push_back({counters.name(j), value, int(2 + j), false});
        }
        samples[event_name].push_back(samp);
    }
    return samp;
}

const Snapshot::SampleMap &Snapshot::getSamples() const {
    return samples;
}
