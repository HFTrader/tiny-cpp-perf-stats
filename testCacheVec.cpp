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
#include "Allocators.h"

// Returns time in nanoseconds from epoch
static double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double secs = ts.tv_sec + double(ts.tv_nsec) * 1E-9;
    return secs;
}

// A fake order book
struct OrderBook {
    uint32_t count = 0;
    uint8_t dummy[128];
};

// Represents a ticker
// It has fast comparison operators using the fact that
// the ticker value has 8 bytes
struct Ticker {
    union {
        uint64_t uval;
        char name[8];
    };
    bool operator<(const Ticker& rhs) const {
        return uval < rhs.uval;
    }
    bool operator==(const Ticker& rhs) const {
        return uval == rhs.uval;
    }
    Ticker& operator=(const std::string& rhs) {
        memset(name, 0, sizeof(name));
        strncpy(name, rhs.c_str(), sizeof(name));
        return *this;
    }
    Ticker& operator=(const Ticker& rhs) {
        if (&rhs != this) {
            uval = rhs.uval;
        }
        return *this;
    }
    friend std::ostream& operator<<(std::ostream& out, const Ticker& ticker) {
        return out << std::string(ticker.name);
    }
};

// Implements std::hash for the Ticker object so it can be used
// as key in STL containers
template <>
struct std::hash<Ticker> {
    std::size_t operator()(Ticker const& s) const noexcept {
        return h(s.uval);
    }
    std::hash<uint64_t> h;
};

// Ticker statistics as from file plus our internal index
struct TickerInfo {
    uint32_t index;
    Ticker ticker;
    uint64_t volume;
};

// The actual test run, templated by map type
template <class MapType>
void testme(const std::string& key, Snapshot& snap,
            const std::vector<TickerInfo>& tickers, uint32_t numevents,
            uint32_t numtickers, double runsecs) {
    // Initialize the entire map
    MapType bookmap;
    for (uint32_t j = 0; j < numtickers; ++j) {
        Ticker t = tickers[j].ticker;
        bookmap[t].count = 0;
    }

    // Create a vector of tickers to look up, simulating arriving packets
    struct Packet {
        Ticker ticker;
        char dummy[8];
    };
    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<> chance(0, numtickers - 1);
    std::vector<Packet> packets(numevents);
    for (uint32_t j = 0; j < numevents; ++j) {
        uint32_t idx = chance(rng);
        packets[j].ticker = tickers[idx].ticker;
    }

    // Loop measuring lookups
    double start = now();
    snap.start();
    uint64_t counter = 0;
    do {
        for (const Packet& packet : packets) {
            OrderBook& book(bookmap[packet.ticker]);
            book.count += 1;
            counter++;
        }
    } while (now() < start + runsecs);
    snap.stop(key, numtickers, counter);

    // The sum of all counters has to match
    for (auto& ev : bookmap) {
        counter -= ev.second.count;
    }
    assert(counter == 0);
}

// Dearly missing split function from the STL
// I should have it as string_view instead but it's not ubiquitous yet
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

// Functional style code to read the tickers
template <typename Fn>
void getTickers(const std::string& filename, Fn fn) {
    std::cout << "Opening file " << filename << std::endl;
    std::ifstream ifs(filename);
    while (ifs.good()) {
        try {
            std::string line;
            std::getline(ifs, line, '\n');
            std::string ticker, svolume;
            if (split(line, ',', ticker, svolume)) {
                uint64_t volume = std::stol(svolume);
                fn(ticker, volume);
            }
        } catch (...) {
        }
    }
}

// All our containers, templated by allocator type
template <class Allocator>
using BoostFlatMapType =
    boost::container::flat_map<Ticker, OrderBook, std::less<Ticker>, Allocator>;
template <class Allocator>
using StdMapType = std::map<Ticker, OrderBook, std::less<Ticker>, Allocator>;
template <class Allocator>
using StdHashMapType = std::unordered_map<Ticker, OrderBook, std::hash<Ticker>,
                                          std::equal_to<Ticker>, Allocator>;

// All ou allocators
using stdalloc = std::allocator<std::pair<Ticker, OrderBook>>;
using wrapped = retail_allocator<std::pair<Ticker, OrderBook>, standard_allocator>;
using mmaped = retail_allocator<std::pair<Ticker, OrderBook>, mmap_allocator>;
using transp = retail_allocator<std::pair<Ticker, OrderBook>, transparent_allocator>;
using hugepage = retail_allocator<std::pair<Ticker, OrderBook>, hugepage_allocator>;
using boostpmr = BoostAllocator<std::pair<const Ticker, OrderBook>>;

int main(int argc, char* argv[]) {
    // Print usage if necessary
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tickerfile>" << std::endl;
        std::cerr << "Example:\n\t" << argv[0] << " Bats_Volume_2016-05-03.csv"
                  << std::endl;
        return 0;
    }

    // Get file name from user
    std::string filename = argv[1];

    // Read all tickers from file into a vector
    double totalvolume = 0;
    std::vector<TickerInfo> tickers;
    auto tickproc = [&tickers, &totalvolume](const std::string& ticker, uint64_t volume) {
        uint32_t index = tickers.size();
        TickerInfo tk;
        tk.index = index;
        tk.ticker = ticker;
        tk.volume = volume;
        totalvolume += volume;
        tickers.push_back(tk);
    };
    getTickers(filename, tickproc);
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

    const size_t numevents = 500;
    const double runsecs = 0.5;

    Snapshot snap(1);
    for (uint32_t numtickers = 500; numtickers < 6500; numtickers += 500) {
        testme<StdMapType<stdalloc>>("std::map<std::alloc>", snap, tickers, numevents,
                                     numtickers, runsecs);
        testme<StdMapType<wrapped>>("std::map<wrap::std>", snap, tickers, numevents,
                                    numtickers, runsecs);
        testme<StdMapType<mmaped>>("std::map<wrap::mmap>", snap, tickers, numevents,
                                   numtickers, runsecs);
        testme<StdMapType<transp>>("std::map<wrap::thp>", snap, tickers, numevents,
                                   numtickers, runsecs);
        testme<StdMapType<hugepage>>("std::map<wrap::huge>", snap, tickers, numevents,
                                     numtickers, runsecs);
        testme<StdMapType<boostpmr>>("std::map<boostpmr>", snap, tickers, numevents,
                                     numtickers, runsecs);
        testme<boost::container::flat_map<Ticker, OrderBook>>(
            "boost::flat_map<std::alloc>", snap, tickers, numevents, numtickers, runsecs);

        testme<BoostFlatMapType<stdalloc>>("boost::flat_map<std::alloc>", snap, tickers,
                                           numevents, numtickers, runsecs);
        testme<BoostFlatMapType<wrapped>>("boost::flat_map<wrap::std>", snap, tickers,
                                          numevents, numtickers, runsecs);
        testme<BoostFlatMapType<mmaped>>("boost::flat_map<wrap::mmap>", snap, tickers,
                                         numevents, numtickers, runsecs);
        testme<BoostFlatMapType<transp>>("boost::flat_map<wrap::thp>", snap, tickers,
                                         numevents, numtickers, runsecs);
        testme<BoostFlatMapType<hugepage>>("boost::flat_map<wrap::huge>", snap, tickers,
                                           numevents, numtickers, runsecs);
        testme<BoostFlatMapType<boostpmr>>("boost::flat_map<boostpmr>", snap, tickers,
                                           numevents, numtickers, runsecs);

        testme<StdHashMapType<stdalloc>>("std::unordered_map<std::alloc>", snap, tickers,
                                         numevents, numtickers, runsecs);
        testme<StdHashMapType<wrapped>>("std::unordered_map<wrap::std>", snap, tickers,
                                        numevents, numtickers, runsecs);
        testme<StdHashMapType<mmaped>>("std::unordered_map<wrap::mmap>", snap, tickers,
                                       numevents, numtickers, runsecs);
        testme<StdHashMapType<transp>>("std::unordered_map<wrap::thp>", snap, tickers,
                                       numevents, numtickers, runsecs);
        testme<StdHashMapType<hugepage>>("std::unordered_map<wrap::huge>", snap, tickers,

                                         numevents, numtickers, runsecs);
        testme<StdHashMapType<boostpmr>>("std::unordered_map<boostpmr>", snap, tickers,
                                         numevents, numtickers, runsecs);
    }

    // Print summary
    snap.summary("Map");
}