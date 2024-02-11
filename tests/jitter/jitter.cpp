
#include <vector>
#include <cstdint>
#include <iostream>
#include <set>
#include <cstring>
#include <unistd.h>

#include "CpuUtils.h"
#include "MicroStats.h"
#include "TimingUtils.h"
#include "StringUtils.h"

using Histogram = MicroStats<8>;
const uint64_t ROUGHLY_ONE_SECOND_IN_TICKS = 3000000000;

struct Stats {
    std::size_t wait_ticks = 3 * ROUGHLY_ONE_SECOND_IN_TICKS;
    std::size_t core;
    int policy;
    int prio;
    pthread_t tid;
    bool print;
    bool async;
    Histogram hist;
    uint64_t pause;
    uint64_t events;
};

double calcFrequencyGHz(uint64_t ticks) {
    auto nop = [](uint64_t) {};
    auto t0 = utcnow();
    busyWait(ticks, nop);
    double elapsed_nanos = utcnow() - t0;
    return ticks / elapsed_nanos;
}

uint64_t calcQuantum(uint64_t ticks) {
    unsigned cpu = sched_getcpu();
    uint64_t quantum = std::numeric_limits<uint64_t>::max();
    uint64_t last = 0;
    auto update = [&last, &quantum, &cpu](uint64_t now) {
        uint64_t diff = now - last;  // forward_difference(now, last);
        if (diff < quantum) {
            quantum = diff;
        }
        unsigned newcpu = sched_getcpu();
        if (cpu != newcpu) {
            printf("Unusual migration from cpu %d to %d\n", cpu, newcpu);
            // Reset stats
            quantum = std::numeric_limits<uint64_t>::max();
            cpu = newcpu;
        }
        last = now;
    };
    busyWait(ticks, update);
    return quantum;
}

uint64_t quantum = calcQuantum(ROUGHLY_ONE_SECOND_IN_TICKS);
double freqGHz = calcFrequencyGHz(ROUGHLY_ONE_SECOND_IN_TICKS);

void collectJitterSamples(Stats& opt) {
    uint64_t threshold = 10 * quantum;
    uint64_t last = tic();
    busyWait(opt.wait_ticks, [&last, &opt, threshold](uint64_t now) {
        uint64_t diff = forward_difference(now, last);
        if (diff > threshold) opt.hist.add(diff);
        last = now;
    });
    if (opt.print) {
        unsigned cpu = sched_getcpu();
        printf(
            "Testing core %-2ld cpu:%-2d Isol:%-2s Events:%-6ld "
            "Pct1/50/99: %-7ld %-7ld %-7ld\n",
            opt.core, cpu, yn(isIsolated(opt.core)), opt.hist.count(),
            long(opt.hist.percentile(1)), long(opt.hist.percentile(50)),
            long(opt.hist.percentile(99.9)));
    }
    opt.pause = opt.hist.percentile(99.9);
    opt.events = opt.hist.count();
}

static void* runtest(void* args) {
    Stats& opt = *((Stats*)args);
    collectJitterSamples(opt);
    return nullptr;
}

void runJitterTestInCore(Stats& opt) {
    // Sets affinity to given core, and scheduling policy + priority.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(opt.core, &mask);
    int res = pthread_attr_setaffinity_np(&attr, sizeof(mask), &mask);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setaffinity: %d %s\n", res, strerror(res));
    }

    // Set schedule policy
    res = pthread_attr_setschedpolicy(&attr, opt.policy);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setschedpolicy: %d %s\n", res, strerror(res));
    }

    // Set schedule priority
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = opt.prio;
    res = pthread_attr_setschedparam(&attr, &param);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setschedparam: %d %s\n", res, strerror(res));
    }

    // Launches thread
    res = pthread_create(&opt.tid, &attr, runtest, &opt);
    if (res != 0) {
        perror("pthread_create");
    }

    if (opt.async) {
        // This is just so the results are printed in order
        usleep(10000);
    } else {
        // Wait for thread to finish
        void* retval;
        pthread_join(opt.tid, &retval);
    }
}

void runAllTests(bool async) {
    auto numcores = getNumberOfCores();
    auto wait_ticks = (async ? 10 : 1) * ROUGHLY_ONE_SECOND_IN_TICKS;

    std::vector<Stats> stats(numcores);
    for (int core = 0; core < numcores; ++core) {
        Stats& opt(stats[core]);
        opt.core = core;
        opt.wait_ticks = wait_ticks;
        opt.policy = SCHED_FIFO;
        opt.prio = sched_get_priority_max(SCHED_FIFO);
        opt.print = true;
        opt.async = async;
        runJitterTestInCore(opt);
    }

    // Wait for all threads to stop and collect stats
    uint64_t min_events = std::numeric_limits<uint64_t>::max();
    uint64_t min_pause = std::numeric_limits<uint64_t>::max();
    std::vector<uint64_t> isol_pause;
    for (Stats& s : stats) {
        if (async) {
            void* retval;
            pthread_join(s.tid, &retval);
        }
        min_events = std::min(min_events, s.events);
        min_pause = std::min(min_pause, s.pause);
        if (isIsolated(s.core)) {
            isol_pause.push_back(s.pause);
        }
    }

    // Take stats from isolated cores if they exist
    if (!isol_pause.empty()) {
        int num_isol = isol_pause.size();
        std::sort(isol_pause.begin(), isol_pause.end());
        min_pause = isol_pause[num_isol / 2];
    }
    printf("\n>>> MinEvents:%ld MinPause:%ld ticks \n\n", min_events, min_pause);

    int min_sd_prio = sched_get_priority_min(SCHED_OTHER);
    for (int core = 0; core < numcores; ++core) {
        Stats& opt(stats[core]);
        opt.core = core;
        opt.wait_ticks = wait_ticks;
        opt.policy = SCHED_OTHER;
        opt.prio = min_sd_prio;
        opt.print = false;
        opt.async = async;
        runJitterTestInCore(opt);
    }

    for (Stats& s : stats) {
        if (async) {
            void* retval;
            pthread_join(s.tid, &retval);
        }
        double wait_secs = s.wait_ticks / (freqGHz * 1E9);
        double excess_events = (double(s.hist.count()) - min_events) / wait_secs;
        double jitter = (double(s.hist.percentile(99.9)) - min_pause) / (freqGHz * 1E3);
        printf("Core:%-2ld  ExcessEvents: %6.0f/sec  Jitter: %6.1fus\n", s.core,
               excess_events, jitter);
    }
}

int main() {
    lockAllMemory();
    auto numcores = getNumberOfCores();
    auto affinity = makeSet(getThreadAffinity());
    int policy;
    struct sched_param param;
    pthread_getschedparam(pthread_self(), &policy, &param);

    printf("CPU Freq:%6.1f GHz Policy:%s Priority:%d Cores:%ld Avail:%ld  ", freqGHz,
           getPolicyName(policy), param.sched_priority, numcores, affinity.size());
    for (auto core : affinity) {
        printf("%ld ", core);
    }
    printf("\n");
    if (numcores == affinity.size()) {
        printf(
            "*** There are no isolated cores to get reliable stats on. Results may be "
            "misleading.\n");
    }
    printf("\n\n*********** Running asynchronous tests \n\n");
    runAllTests(true);
    printf("\n\n*********** Running synchronous tests (better) \n\n");
    runAllTests(false);
}
