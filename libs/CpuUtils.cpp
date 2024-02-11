#include "CpuUtils.h"
#include <cstdio>
#include <sys/mman.h>

static auto affinity = makeSet(getThreadAffinity());
bool isIsolated(int core) {
    return affinity.find(core) == affinity.end();
}

bool lockAllMemory() {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        printf("Failed to lock memory. Please increase RLIMIT_MEMLOCK\n");
        return false;
    }
    return true;
}

std::vector<std::size_t> getThreadAffinity() {
    std::vector<std::size_t> cores;
    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    ::pthread_getaffinity_np(self, sizeof(cpuset), &cpuset);
    for (int j = 0; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &cpuset)) {
            cores.push_back(j);
        }
    }
    return cores;
}

bool setThreadAfinity(const std::vector<std::size_t>& cores) {
    pthread_t self = pthread_self();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (std::size_t core : cores) {
        CPU_SET(core, &mask);
    }
    int err = ::pthread_setaffinity_np(self, sizeof(mask), &mask);
    return (err == 0);
}

bool setThreadAfinity(std::size_t core) {
    pthread_t self = pthread_self();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);
    int err = ::pthread_setaffinity_np(self, sizeof(mask), &mask);
    return (err == 0);
}

std::size_t getCurrentCore() {
    return sched_getcpu();
}

std::size_t getNumberOfCores() {
    return get_nprocs_conf();
}

const char* getPolicyName(int policy) {
    switch (policy) {
        case SCHED_RR: return "RoundRobin"; break;
        case SCHED_BATCH: return "Batch"; break;
        case SCHED_OTHER: return "Standard"; break;
        case SCHED_IDLE: return "Idle"; break;
        default: return "N/A"; break;
    }
}

bool setPriority(int scheduler, int prio) {
    pthread_t tid = pthread_self();
    struct sched_param param;
    int policy;
    int res = pthread_getschedparam(tid, &policy, &param);
    if (res != 0) {
        perror("pthread_getschedparam");
        return false;
    }
    param.sched_priority = prio;
    res = pthread_setschedparam(tid, scheduler, &param);
    if (res != 0) {
        perror("pthread_setschedparam");
        return false;
    }
    return true;
}

void setRealtimePriority(int prio) {
    setPriority(SCHED_RR, prio);
}

void setIdlePriority(int prio) {
    setPriority(SCHED_IDLE, prio);
}

void setStandardPriority(int prio) {
    setPriority(SCHED_OTHER, prio);
}