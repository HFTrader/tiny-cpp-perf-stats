#pragma once

#include <pthread.h>
#include <sys/sysinfo.h>
#include <vector>

std::vector<std::size_t> getThreadAffinity();

bool setThreadAfinity(const std::vector<std::size_t>& cores);

bool setThreadAfinity(std::size_t core);

std::size_t getCurrentCore();

std::size_t getNumberOfCores();

const char* getPolicyName(int policy);
bool setPriority(int scheduler, int prio);
void setRealtimePriority(int prio);
void setIdlePriority(int prio);
void setStandardPriority(int prio);