#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "PerfUtils.h"

// https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events

struct PerfGroup {
    PerfGroup();
    ~PerfGroup();
    bool init(const std::vector<std::string> &events);
    void close();
    bool start();
    bool stop();

    size_t size() const;
    uint64_t operator[](size_t index) const;
    uint64_t operator[](const char *name) const;
    std::string name(size_t index) const;

    struct Descriptor {
        std::string name;
        int fd;
        uint64_t id;
        uint64_t value;
        size_t order;
    };

private:
    void read();
    std::vector<int> _leaders;
    std::vector<Descriptor> _ids;
    std::vector<size_t> _order;
    std::unordered_map<std::string, size_t> _names;
};
