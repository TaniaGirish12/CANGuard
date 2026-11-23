#pragma once

#include <cstddef>

namespace canguard {

struct DetectorConfig {
    double k = 3.0;           // TimingRule: standard deviations below the mean inter-arrival time
    double r = 0.2;           // TimingRule: fraction of the mean inter-arrival time as a lower bound
    double windowMs = 100.0;  // RateRule: length of the sliding window
    double rateFactor = 3.0;  // RateRule: allowed multiple of the expected count in the window
    std::size_t minScore = 1; // rules that must fire for a message to count as an attack
};

}  // namespace canguard
