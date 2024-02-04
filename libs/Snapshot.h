#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <limits>
#include <unordered_map>
#include <iostream>
#include "PerfGroup.h"

class Snapshot {
public:
    using MetricName = std::string;
    using EventName = std::string;
    struct Metric {
        std::string name;
        std::vector<double> values;
    };
    struct Event {
        std::string name;
        std::vector<size_t> N;
        std::vector<Metric> metrics;
    };
    using EventMap = std::map<EventName, Event>;

    Snapshot();
    Snapshot(const std::vector<std::string> &pmc);
    ~Snapshot();
    void start();
    void stop(const char *event, uint64_t numitems, uint64_t numrep);
    const EventMap &getEvents() const;
    double operator[](std::size_t index) const;
    double operator[](const char *key) const;

private:
    PerfGroup counters;
    EventMap events;
    std::size_t last_iterations = 0;
};
