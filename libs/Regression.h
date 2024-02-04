#pragma once

#include "Snapshot.h"
#include <iostream>

void summary(const Snapshot::EventMap &samples, const std::string &header,
             const std::string &dependent_name = "cycles", std::ostream &out = std::cout);
