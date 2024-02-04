#pragma once

#include "Snapshot.h"
#include <iostream>

using SampleVector = std::vector<Snapshot::Sample>;
using SampleMap = std::unordered_map<std::string, SampleVector>;
SampleMap samples;

void summary(const SampleMap &samples, const std::string &header,
             std::ostream &out = std::cout);
