#include "PerfCounter.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

PerfCounter::PerfCounter( int event )
{
    _fd = -1;
    init( event );
}

PerfCounter::PerfCounter()
{
    _fd = -1;
}

static int perf_event_open( struct perf_event_attr *hw_event_uptr,
                                   pid_t pid,
                                   int cpu,
                                   int group_fd,
                                   unsigned long flags )
{
    int ret = syscall(
        __NR_perf_event_open, hw_event_uptr, pid, cpu, group_fd, flags);
    return ret;
}


bool PerfCounter::init( int event, int group ) {
    // already initialized
    if ( _fd>=0 ) return false;

    struct perf_event_attr pe;
    long long count;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = event; //PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    //pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    int pid = getpid();
    int fd = perf_event_open(&pe, 0, -1, group, 0);
    if (fd == -1) {
       fprintf(stderr, "Error opening leader %llx\n", pe.config);
    }
    _fd = fd;
}

bool PerfCounter::start() {
    if ( _fd<0 ) return false;
    ioctl(_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0);
    return true;
}

uint64_t PerfCounter::stop() {
    uint64_t count = 0;
    if ( _fd>=0 ) {
        ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0);
        int nb = read(_fd, &count, sizeof(count) );
        if ( nb!=sizeof(count) ) return 0;
    }
    return count;
}

void PerfCounter::close() {
    if ( _fd>=0 ) {
        ::close(_fd);
        _fd = -1;
    }
}

PerfCounter::~PerfCounter()
{
    close();
}
