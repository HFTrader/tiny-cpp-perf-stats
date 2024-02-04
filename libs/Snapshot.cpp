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

void Snapshot::stop(const char *event_name, uint64_t numitems, uint64_t numiterations) {
    counters.stop();
    last_iterations = numiterations;
    if (numiterations > 0) {
        Event &event(events[event_name]);
        if (event.metrics.empty()) {
            event.name = event_name;
            event.metrics.resize(counters.size());
            for (size_t j = 0; j < counters.size(); ++j) {
                event.metrics[j].name = counters.name(j);
            }
        }
        event.N.push_back(numitems);
        for (size_t j = 0; j < counters.size(); ++j) {
            double average = double(counters[j]) / numiterations;
            event.metrics[j].values.push_back(average);
        }
    }
}

const Snapshot::EventMap &Snapshot::getEvents() const {
    return events;
}

double Snapshot::operator[](std::size_t index) const {
    if (last_iterations == 0) return std::numeric_limits<double>::quiet_NaN();
    return double(counters[index]) / last_iterations;
}

double Snapshot::operator[](const char *key) const {
    if (last_iterations == 0) return std::numeric_limits<double>::quiet_NaN();
    return double(counters[key]) / last_iterations;
}
