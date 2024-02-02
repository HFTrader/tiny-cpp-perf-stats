/**
 * Copyright Vitorian LLC (2020)
 *
 * This application will test if spinlocks are ok to be used in a shared memory
 * configuration and what is the best solution
 *
 */
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <xmmintrin.h>

#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <atomic>
#include <set>

#include "Snapshot.h"
#include "MMapFile.h"
#include "MicroStats.h"

constexpr bool DEBUG = false;

// Bryce Adelstein Lelbach — The C++20 synchronization library
// https://www.youtube.com/watch?v=z7fPxjZKsBY
class BryceSpinLock {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() noexcept {
        for (std::uint64_t k = 0; flag.test_and_set(std::memory_order_acquire); ++k) {
            if (k < 4)
                ;
            else if (k < 16)
                __asm__ __volatile__("rep;nop" ::: "memory");
            else if (k < 64)
                sched_yield();
            else {
                timespec rqtp = {0, 0};
                rqtp.tv_sec = 0;
                rqtp.tv_nsec = 1000;
                nanosleep(&rqtp, nullptr);
            }
        }
    }
    bool try_lock() noexcept {
        return flag.test_and_set(std::memory_order_acquire);
    }
    void unlock() noexcept {
        flag.clear(std::memory_order_release);
    }
    bool locked() noexcept {
        bool locked = flag.test_and_set(std::memory_order_acquire);
        if (!locked) {
            flag.clear(std::memory_order_release);
            return false;
        }
        return true;
    }
};

class BryceSimplifiedSpinLock {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() noexcept {
        for (std::uint64_t k = 0; flag.test_and_set(std::memory_order_acquire); ++k) {
            if (k < 4)
                ;
            else
                sched_yield();
        }
    }
    bool try_lock() noexcept {
        return flag.test_and_set(std::memory_order_acquire);
    }
    void unlock() noexcept {
        flag.clear(std::memory_order_release);
    }
    bool locked() noexcept {
        bool locked = flag.test_and_set(std::memory_order_acquire);
        if (!locked) {
            flag.clear(std::memory_order_release);
            return false;
        }
        return true;
    }
};

class TASSpinLock {
public:
    TASSpinLock() {
        __sync_lock_release(&val);
    }
    ~TASSpinLock() {
    }

    void lock() noexcept {
        while (!try_lock()) {
            __asm__ __volatile__("rep; nop;" ::: "memory");
        }
    }
    bool try_lock() noexcept {
        return __sync_lock_test_and_set(&val, 1) == 0;
    }
    void unlock() noexcept {
        __sync_synchronize();
        __sync_lock_release(&val);
    }
    bool locked() const noexcept {
        return val != 0;
    }

private:
    volatile uint32_t val;
};

// https://rigtorp.se/spinlock/
class RigtorpSpinLock {
public:
    RigtorpSpinLock() : lock_(0) {
    }
    ~RigtorpSpinLock() {
    }

    void lock() noexcept {
        for (;;) {
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                return;
            }
            while (lock_.load(std::memory_order_relaxed)) {
                for (uint32_t j = 0; j < 10; ++j) __builtin_ia32_pause();
            }
        }
    }
    bool try_lock() noexcept {
        return !lock_.load(std::memory_order_relaxed) &&
               !lock_.exchange(true, std::memory_order_acquire);
    }
    void unlock() noexcept {
        lock_.store(false, std::memory_order_release);
    }
    bool locked() noexcept {
        return lock_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<bool> lock_;
};

uint64_t busywait(uint64_t cycles) {
    uint64_t tstart = __builtin_ia32_rdtsc();
    uint64_t tend = tstart + cycles;
    while (__builtin_ia32_rdtsc() < tend)
        ;
    return __builtin_ia32_rdtsc() - tstart;
}

struct TestOptions {
    std::string testname;
    uint32_t testnum;
    uint32_t numprocesses;
    uint32_t numloops;
    uint32_t verbose = 0;
    uint32_t offset;
    uint32_t waitcycles;
};

template <class SpinLock>
void testSpinLock(Snapshot& snap, TestOptions& opt) {
    // Create path
    char shmname[PATH_MAX];
    ::sprintf(shmname, "mmtest-%d", ::getpid());
    MMapFile map;
    if (!map.init(shmname, opt.offset * sizeof(uint32_t) + 4)) return;

    // Fork children
    uint32_t procnum = 0;
    std::vector<pid_t> pids(opt.numprocesses, 0);
    for (uint32_t j = 1; j < opt.numprocesses; ++j) {
        pids[j] = fork();
        if (pids[j] == 0) {  // Child
            procnum = j;
            break;
        }
    }

    void* ptr = map.data();
    bool isparent = (procnum == 0);
    char prname[32];
    ::sprintf(prname, "Process-%d", procnum);
    if (DEBUG) fprintf(stdout, "[%s] Memory: %p  %d\n", prname, ptr, *((uint32_t*)ptr));
    if (DEBUG) fprintf(stdout, "[%s] Test starting...\n", prname);

    // Start test
    volatile uint32_t* baseptr = (volatile uint32_t*)ptr;
    SpinLock* lock = new ((void*)&baseptr[0]) SpinLock;
    volatile uint32_t* counter = &baseptr[opt.offset];

    if (DEBUG)
        fprintf(stdout, "[%s] Status: [%s]\n", prname,
                lock->locked() ? "Locked" : "Unlocked");
    if (DEBUG) fprintf(stdout, "[%s] Starting \n", prname);
    uint32_t value = 0;
    MicroStats<3> ustats;
    snap.start();
    uint64_t sumcycles = 0;
    while (value < opt.numloops) {
        if (DEBUG) fprintf(stdout, "[%s] Locking...\n", prname);
        uint64_t t0 = __builtin_ia32_rdtsc();
        _mm_mfence();
        lock->lock();
        if (DEBUG) fprintf(stdout, "[%s] Locked...%d\n", prname, *counter);
        if ((*counter % opt.numprocesses) == procnum) {
            *counter += 1;
            if (DEBUG)
                fprintf(stdout, "[%s] Incrementing counter to %d\n", prname, *counter);
        }
        value = *counter;
        if (DEBUG) fprintf(stdout, "[%s] Unlocking...\n", prname);
        lock->unlock();
        _mm_mfence();
        uint64_t t1 = __builtin_ia32_rdtsc();
        if (DEBUG) fprintf(stdout, "[%s] Unlocked...\n", prname);
        uint64_t elapsed = (t1 - t0);
        sumcycles += elapsed;
        ustats.add(elapsed);
        busywait(opt.waitcycles);
    }

    Snapshot::Sample sample = snap.stop("Loop", 1, opt.numloops);
    fprintf(stdout,
            "%s,%s,Reps,%d,Pad,%d,Wait,%d,Wall,%ld,Cyc,%1.1f,Instr,%1.1f,Cache,%1.1f,"
            "Branch,%1.1f,"
            "P10,%1.0f,P50,%1.0f,P90,%1.0f,P99,%1.0f\n",
            opt.testname.c_str(), prname, opt.numloops, opt.offset, opt.waitcycles,
            sumcycles / opt.numloops, sample["cycles"], sample["instructions"],
            sample["cache-misses"], sample["branch-misses"], ustats.percentile(10),
            ustats.percentile(50), ustats.percentile(90), ustats.percentile(99));

    // Wait for children
    if (isparent) {
        int status = 0;
        for (uint32_t j = 1; j < opt.numprocesses; ++j) {
            ::waitpid(pids[j], &status, 0);
        }
        MMapFile::unlink(shmname);
    } else {
        exit(0);
    }
}

const std::array<const char*, 4> testnames = {"RigtorpSpinLock", "BryceSpinLock",
                                              "BryceSimplifiedSpinLock", "TASSpinLock"};

struct CommandLineOptions {
    int verbose = 0;
    std::set<uint32_t> alltests;
    std::set<uint32_t> alloffsets;
    std::set<uint32_t> allcycles;
    std::set<uint32_t> allloops;
    std::set<uint32_t> allprocesses;
};

void parseCommandLine(int argc, char* argv[], CommandLineOptions& opt) {
    if (argc < 2) {
        std::cout << "Usage: memorymap [options]\n";
        std::cout << "Options:\n";
        std::cout << "    -p <number processes> number of processes to run\n";
        std::cout << "    -t <test number>      test to run (default:all)\n";
        std::cout << "    -w <cycles>           number of busy cycles to wait\n";
        std::cout << "    -o <offset>           padding/offset data-to-lock\n";
        std::cout << "    -r <repetitions>      number of repetitions\n";
        std::cout << "    -v                    verbose\n";
        exit(0);
    }

    std::cout << "Test,Process,Loops,Offset,Cycles,CPUCycles,Instructions,CacheMisses,"
                 "BranchMisses\n";
    for (int j = 1; j < argc;) {
        // starts with '--'
        if (::strcmp("-p", argv[j]) == 0) {
            if (j + 1 == argc) {
                fprintf(stderr, "Missing -p argument\n");
                exit(1);
            }
            opt.allprocesses.insert(::atoi(argv[j + 1]));
            j += 2;
        } else if (::strcmp("-t", argv[j]) == 0) {
            if (j + 1 == argc) {
                fprintf(stderr, "Missing -t argument\n");
                exit(1);
            }
            int testnum = ::atoi(argv[j + 1]);
            opt.alltests.insert(testnum);
            j += 2;
        } else if (::strcmp("-v", argv[j]) == 0) {
            opt.verbose += 1;
            j += 1;
        } else if (::strcmp("-o", argv[j]) == 0) {
            if (j + 1 == argc) {
                fprintf(stderr, "Missing -o argument\n");
                exit(1);
            }
            opt.alloffsets.insert(::atoi(argv[j + 1]));
            j += 2;
        } else if (::strcmp("-w", argv[j]) == 0) {
            if (j + 1 == argc) {
                fprintf(stderr, "Missing -w argument\n");
                exit(1);
            }
            opt.allcycles.insert(::atoi(argv[j + 1]));
            j += 2;
        } else {
            fprintf(stderr, "Ignored %d-th argument [%s]\n", j, argv[j]);
            j += 1;
        }
    }

    if (opt.alltests.empty()) {
        for (uint32_t j = 0; j < testnames.size(); ++j) opt.alltests.insert(j);
    }
    if (opt.allprocesses.empty()) {
        opt.allprocesses.insert({2, 3, 4, 5, 6, 7});
    }
    if (opt.allcycles.empty()) {
        opt.allcycles.insert({0, 16, 256, 1024, 16384});
    }
    if (opt.alloffsets.empty()) {
        opt.alloffsets.insert({1, 16, 128});
    }
    if (opt.allloops.empty()) {
        opt.allloops.insert({1 << 10, 1 << 12, 1 << 14, 1 << 16, 1 << 18, 1 << 20});
    }
}

struct strproxy {
    const char* start;
    size_t length;
    strproxy() {
        start = nullptr;
        length = 0;
    }
    strproxy(const std::string& str) {
        start = str.data();
        length = str.size();
    }
    strproxy(const std::string& str, size_t size) {
        start = str.data();
        length = size;
    }
    strproxy(const std::string& str, size_t pos, size_t size) {
        start = str.data() + pos;
        length = size - pos;
    }
    template <std::size_t N>
    strproxy(const char (&value)[N]) {
        start = value;
        length = N - 1;
    }
    operator std::string() const {
        return std::string(start, length);
    }
    bool operator==(const strproxy& rhs) const {
        if (length != rhs.length) return false;
        return ::memcmp(start, rhs.start, length) == 0;
    }
};

static int64_t getHugePageSize() {
    FILE* fs = fopen("/proc/meminfo", "r");
    while (true) {
        char buffer[256];
        const char* s = fgets(buffer, sizeof(buffer), fs);
        if (s != buffer) break;
        s = strstr(buffer, "Hugepagesize");
        if (s != nullptr) {
            s = strchr(s, ':');
            if (s != nullptr) {
                char* ends = nullptr;
                long value = strtol(++s, &ends, 10);
                if (ends != s) {
                    if (strstr(ends, "kB") != nullptr) {
                        return value * 1024;
                    }
                    if (strstr(ends, "mB") != nullptr) {
                        return value * (1024L * 1024);
                    }
                    if (strstr(ends, "gB") != nullptr) {
                        return value * (1024L * 1024 * 1024);
                    }
                }
            }
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    int64_t hugepagesize = getHugePageSize();
    std::cout << "Huge page size:" << hugepagesize << '\n';

    CommandLineOptions options;
    parseCommandLine(argc, argv, options);

    TestOptions test;
    for (uint32_t testnum : options.alltests) {
        for (uint32_t numprocesses : options.allprocesses) {
            for (uint32_t offset : options.alloffsets) {
                for (uint32_t cycles : options.allcycles) {
                    Snapshot snap;
                    for (uint32_t numloops : options.allloops) {
                        test.testname = testnames[testnum];
                        test.testnum = testnum;
                        test.numprocesses = numprocesses;
                        test.offset = offset;
                        test.numloops = numloops;
                        test.waitcycles = cycles;
                        switch (testnum % 4) {
                            case 0: testSpinLock<RigtorpSpinLock>(snap, test); break;
                            case 1: testSpinLock<BryceSpinLock>(snap, test); break;
                            case 2:
                                testSpinLock<BryceSimplifiedSpinLock>(snap, test);
                                break;
                            case 3: testSpinLock<TASSpinLock>(snap, test); break;
                            default: break;
                        };
                    }
                    snap.summary(testnames[testnum]);
                }
            }
        }
    }
}
