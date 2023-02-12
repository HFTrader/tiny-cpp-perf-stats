#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <chrono>

#include <boost/container/flat_map.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "Snapshot.h"

static double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double secs = ts.tv_sec + double(ts.tv_nsec) * 1E-9;
    // printf("\n>> %ld %ld %10.2f\n", ts.tv_nsec, ts.tv_sec, secs);
    // std::cout << ts.tv_nsec << " " << ts.tv_sec << " " << secs << std::endl;
    return secs;
}

struct OrderBook {
    uint32_t count = 0;
    uint8_t dummy[16 * 1024];
};

struct Ticker {
    char name[8];
    bool operator<(const Ticker& rhs) const {
        return std::memcmp(name, rhs.name, sizeof(name)) < 0;
    }
    bool operator==(const Ticker& rhs) const {
        return std::memcmp(name, rhs.name, sizeof(name)) == 0;
    }
    Ticker& operator=(const std::string& rhs) {
        memset(name, 0, sizeof(name));
        strncpy(name, rhs.c_str(), sizeof(name));
        return *this;
    }
    Ticker& operator=(const Ticker& rhs) {
        if (&rhs != this) {
            memset(name, 0, sizeof(name));
            std::memcpy(name, rhs.name, sizeof(name));
        }
        return *this;
    }
    friend std::ostream& operator<<(std::ostream& out, const Ticker& ticker) {
        return out << std::string(ticker.name);
    }
};

template <>
struct std::hash<Ticker> {
    std::size_t operator()(Ticker const& s) const noexcept {
        const uint64_t* ptr = reinterpret_cast<const uint64_t*>(s.name);
        return h(*ptr);
    }
    std::hash<uint64_t> h;
};

struct TickerInfo {
    uint32_t index;
    Ticker ticker;
    uint64_t volume;
};

struct Event {
    Ticker ticker;
    uint64_t time;
    uint32_t count;
};

using EventVec = std::vector<Event>;
using TickerVec = std::vector<TickerInfo>;

template <template <typename Key, typename... Value> class MapType>
void testme(const std::string& key, Snapshot& snap, const TickerVec& tickers,
            uint32_t numevents, uint32_t numtickers, double runsecs) {
    // Initialize the entire map
    MapType<Ticker, OrderBook> bookmap{};
    for (uint32_t j = 0; j < numtickers; ++j) {
        bookmap[tickers[j].ticker].count = 0;
    }

    // Run searching
    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<> chance(0, numtickers - 1);
    snap.start();
    uint64_t counter = 0;
    double start = now();
    do {
        for (uint32_t j = 0; j < numevents; ++j) {
            uint32_t idx = chance(rng);
            OrderBook& book(bookmap[tickers[idx].ticker]);
            book.count += 1;
            counter++;
        }
    } while (now() < start + runsecs);
    snap.stop(key, numtickers, counter);

    // Check the counter
    for (auto& ev : bookmap) {
        counter -= ev.second.count;
    }
    assert(counter == 0);
}

bool split(const std::string& str, char separator, std::string& first,
           std::string& second) {
    std::string::size_type p1 = str.find_first_of(separator);
    if (p1 == std::string::npos) {
        return false;
    }
    std::string::size_type p2 = str.find_first_of(separator, p1 + 1);
    if (p2 == std::string::npos) {
        first = str.substr(0, p1);
        second = str.substr(p1 + 1);
        return true;
    }
    first = str.substr(0, p1);
    second = str.substr(p1 + 1, p2 - p1 - 1);
    return true;
}

template <typename Fn>
void getTickers(const std::string& filename, Fn fn) {
    std::cout << "Opening file " << filename << std::endl;
    std::ifstream ifs(filename);
    while (ifs.good()) {
        try {
            std::string line;
            std::getline(ifs, line, '\n');
            // std::cout << line << std::endl;
            std::string ticker, svolume;
            if (split(line, ',', ticker, svolume)) {
                // std::cout << "    " << ticker << ", " << svolume << std::endl;
                uint64_t volume = std::stol(svolume);
                fn(ticker, volume);
            }
        } catch (...) {
        }
    }
}

int main(int argc, char* argv[]) {
    double totalvolume = 0;
    TickerVec tickers;
    auto tickproc = [&tickers, &totalvolume](const std::string& ticker, uint64_t volume) {
        uint32_t index = tickers.size();
        TickerInfo tk;
        tk.index = index;
        tk.ticker = ticker;
        tk.volume = volume;
        totalvolume += volume;
        tickers.push_back(tk);
    };
    getTickers("Bats_Volume_2016-05-03.csv", tickproc);
    if (tickers.empty()) {
        std::cerr << "Could not read tickers file" << std::endl;
        return 1;
    }

    // Sort tickers by volume order. This guarantees that the volume will
    // be pretty much constant across all tests
    std::sort(tickers.begin(), tickers.end(),
              [](const TickerInfo& lhs, const TickerInfo& rhs) {
                  return lhs.volume > rhs.volume;
              });

    const size_t numevents = 2000;
    const double runsecs = 0.5;
    Snapshot snap(1);
    for (uint32_t numtickers = 400; numtickers < 6500; numtickers += 200) {
        std::cout << "Tickers:" << numtickers << std::endl;
        testme<boost::container::flat_map>("boost::flat_map", snap, tickers, numevents,
                                           numtickers, runsecs);
        testme<std::map>("std::map", snap, tickers, numevents, numtickers, runsecs);
        testme<std::unordered_map>("std::unordered_map", snap, tickers, numevents,
                                   numtickers, runsecs);
    }
    snap.summary("Map");

    return 0;
}
