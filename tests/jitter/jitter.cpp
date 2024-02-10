
#include <vector>
#include <cstdint>
#include <iostream>
#include <set>
#include <cstring>

#include "CpuUtils.h"
#include "MicroStats.h"
#include "TimingUtils.h"

template <typename Container>
std::set<typename Container::value_type> makeSet(const Container& container) {
    std::set<typename Container::value_type> cset;
    for (const auto& value : container) {
        std::cout << "Inserting " << value << std::endl;
        cset.insert(value);
    }
    return cset;
}
auto affinity = makeSet(getThreadAffinity());

bool isIsolated(int core) {
    return affinity.find(core) == affinity.end();
}

const char* yn(bool flag) {
    return flag ? "Y" : "N";
}

struct Options {
    std::size_t wait_ticks = 3000000000;
    std::size_t tick_threshold = 300;
    std::size_t core;
    int policy;
    int prio;
};

using Histogram = MicroStats<8>;

void testJitter(Options& opt) {
    Histogram hist;
    uint64_t start = tic();
    uint64_t finish = start + opt.wait_ticks;
    uint64_t now;
    uint64_t last = tic();
    while ((now = tic()) < finish) {
        uint64_t diff = forward_difference(now, last);
        if (diff > opt.tick_threshold) hist.add(diff);
        last = now;
    }
    unsigned cpu = sched_getcpu();
    printf(
        "Testing core %-2ld cpu:%-2d Isol:%-2s Policy:%-12s Prio:%-2d Events:%ld "
        "Pct1/50/99:%ld/%ld/%ld\n",
        opt.core, cpu, yn(isIsolated(opt.core)), getPolicyName(opt.policy), opt.prio,
        hist.count(), long(hist.percentile(1)), long(hist.percentile(50)),
        long(hist.percentile(99)));
}

static void* runtest(void* args) {
    Options& opt = *((Options*)args);
    testJitter(opt);
    return nullptr;
}

void testCore(Options opt) {
    // Sets affinity to a given core
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(opt.core, &mask);
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = opt.prio;
    int res = pthread_attr_setaffinity_np(&attr, sizeof(mask), &mask);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setaffinity: %d %s\n", res, strerror(res));
    }
    res = pthread_attr_setschedpolicy(&attr, opt.policy);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setschedpolicy: %d %s\n", res, strerror(res));
    }
    res = pthread_attr_setschedparam(&attr, &param);
    if (res != 0) {
        fprintf(stderr, "pthread_attr_setschedparam: %d %s\n", res, strerror(res));
    }

    // Launches
    pthread_t tid;
    res = pthread_create(&tid, &attr, runtest, &opt);
    if (res != 0) {
        perror("pthread_create");
    }

    // Waits
    void* result;
    pthread_join(tid, &result);
}

int main() {
    auto numcores = getNumberOfCores();
    auto affinity = makeSet(getThreadAffinity());
    printf("Reported:%ld Avail:%ld Cores:", numcores, affinity.size());
    for (auto core : affinity) {
        printf("%ld ", core);
    }
    printf("\n");

    for (int core = 0; core < numcores; ++core) {
        for (auto policy : {SCHED_RR, SCHED_OTHER}) {
            int min_prio = sched_get_priority_min(policy);
            int max_prio = sched_get_priority_max(policy);
            std::set<int> priorities{min_prio, max_prio};
            for (auto prio : priorities) {
                Options opt;
                opt.core = core;
                opt.policy = policy;
                opt.prio = prio;
                testCore(opt);
            }
        }
    }
}