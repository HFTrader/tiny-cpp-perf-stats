#include <cstdint>
#include <vector>
#include <iostream>
#include <random>
#include <sstream>
#include "SchedulerPriorityQueue.h"
#include "SchedulerMultimap.h"
#include "Snapshot.h"
#include "Regression.h"

template <class Scheduler>
void test(Snapshot& snap, uint64_t numitems, uint64_t numclasses, uint64_t numloops) {
    Scheduler sch;

    std::vector<Event*> events;
    createEvents(numitems, numclasses, events);

    std::uniform_int_distribution<uint64_t> d(0, numitems);
    std::mt19937 gen;
    std::vector<uint64_t> erand(numitems);
    for (uint64_t j = 0; j < numitems; ++j) {
        erand[j] = d(gen);
    }

    snap.start();

    // Fill up container
    uint64_t counter = 0;
    for (uint64_t j = 0; j < numitems; ++j) {
        uint64_t tm = erand[j];
        sch.schedule(tm, events[j], (void*)&counter);
    }

    snap.stop("FillUp", numitems, numitems);

    // Run through time
    snap.start();
    for (uint64_t n = 0; n < numloops; ++n) {
        for (uint64_t j = 0; j < numitems; ++j) {
            sch.check(n * numitems + j);
        }
    }
    snap.stop("Check", numitems, numitems * numloops);

    // Fill up container again
    snap.start();
    for (uint64_t j = 0; j < numitems; ++j) {
        uint64_t tm = erand[j];
        sch.schedule(tm, events[j], (void*)&counter);
    }
    snap.stop("Refill", numitems, numitems);

    // Run through time again, now refilling
    snap.start();
    for (uint64_t n = 0; n < numloops; ++n) {
        for (uint64_t j = 0; j < numitems; ++j) {
            if (sch.check(j)) {
                uint32_t k = (numitems - 1) - j;
                uint64_t tm = n * numitems + erand[k];
                sch.schedule(tm, events[k], (void*)&counter);
            }
        }
    }
    snap.stop("ReCheck", numitems, numitems * numloops);
}

int main(int argc, char* argv[]) {
    uint32_t numclasses = argc > 1 ? atoi(argv[1]) : 1;
    for (std::string ctype : {"PriorityQueue", "MultiMap"}) {
        Snapshot snap({"cycles"});
        for (uint64_t numitems : {50000, 100000, 250000, 1000000, 2500000, 10000000}) {
            // This is to maintain the time for each run approx constant
            uint64_t numloops = 10000000. / uint64_t(numitems);
            if (ctype == "PriorityQueue") {
                test<SchedulerPriorityQueue>(snap, numitems, numclasses, numloops);
            } else if (ctype == "MultiMap") {
                test<SchedulerMultimap>(snap, numitems, numclasses, numloops);
            }
        }

        summary(snap.getEvents(), ctype, "cycles");
    };

    return 0;
}
