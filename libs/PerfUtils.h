#pragma once
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <unistd.h>

#ifdef HAVE_LIBPFM
#include <perfmon/pfmlib_perf_event.h>
#endif

#ifndef perf_event_open
static int perf_event_open(struct perf_event_attr *hw_event_uptr, pid_t pid, int cpu,
                           int group_fd, unsigned long flags) {
    int ret = syscall(__NR_perf_event_open, hw_event_uptr, pid, cpu, group_fd, flags);
    return ret;
}
#endif

/** Initialize libpfm if available */
bool initialize_libpfm();

bool translate(const char *events[], perf_event_attr *evds, size_t size);