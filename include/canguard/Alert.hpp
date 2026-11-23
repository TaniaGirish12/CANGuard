#pragma once

#include <cstdint>

#include "canguard/Detector.hpp"

namespace canguard {

// A message the detector classified as an attack, with the evidence and the ground truth.
struct Alert {
    double timestamp = 0.0;
    std::uint32_t id = 0;
    RuleMask ruleMask = 0;
    bool isAttack = false;  // ground-truth label from the input
};

}  // namespace canguard
