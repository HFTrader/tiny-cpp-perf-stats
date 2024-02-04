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

#include "Snapshot.h"
#include "Allocators.h"
#include "StringUtils.h"
#include "Datasets.h"
#include "Ticker.h"
#include "DateUtils.h"
#include "Regression.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/container/flat_map.hpp>

// A fake order book
struct OrderBook {
    uint32_t count = 0;
    uint8_t dummy[128];
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
    snap.stop(key.c_str(), numtickers, counter);

    // The sum of all counters has to match
    for (auto& ev : bookmap) {
        counter -= ev.second.count;
    }
    assert(counter == 0);
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
    // Read all tickers from string into a vector
    double totalvolume = 0;
    std::vector<TickerInfo> tickers;
    split(datasets::bats, '\n', [&tickers, &totalvolume](std::string_view line) -> bool {
        auto [ticker, svolume, dummy] = split<3>(line, ',');
        try {
            uint64_t volume = std::stol(std::string(svolume));
            uint32_t index = tickers.size();
            TickerInfo tk;
            tk.index = index;
            tk.ticker = ticker;
            tk.volume = volume;
            totalvolume += volume;
            tickers.push_back(tk);
        } catch (...) {
        }
        return true;
    });
    if (tickers.empty()) {
        std::cerr << "Could not read tickers file" << '\n';
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
    std::vector<std::string> counter_names{"cycles", "instructions", "cache-misses",
                                           "branch-misses"};
    Snapshot snap(counter_names);
    for (uint32_t numtickers = 500; numtickers < 6500; numtickers += 500) {
        std::cout << "Tickers:" << numtickers << '\n';
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
    summary(snap.getEvents(), "Map");
}