#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "canguard/CanFrame.hpp"
#include "canguard/DetectorConfig.hpp"
#include "canguard/IRule.hpp"
#include "canguard/IdProfile.hpp"
#include "canguard/IdState.hpp"

namespace canguard {

// Bit i is set when the rule with index i (see Detector::ruleName) fired.
using RuleMask = std::uint32_t;

struct Detection {
    std::size_t index = 0;  // line number of the frame, which identifies it in the input
    bool predictedAttack = false;
    RuleMask ruleMask = 0;
};

// Runs the rules on frames in timestamp order. All mutable state is kept per CAN ID, so
// detectors that own disjoint sets of IDs can run on different threads.
class Detector {
public:
    // The profiles are shared and read-only; they must outlive the detector.
    Detector(const ProfileMap& profiles, const DetectorConfig& config);
    Detector(ProfileMap&&, const DetectorConfig&) = delete;

    Detection process(const CanFrame& frame);

    std::size_t ruleCount() const;
    std::string_view ruleName(std::size_t ruleIndex) const;

private:
    const ProfileMap& profiles_;
    DetectorConfig config_;
    std::vector<std::unique_ptr<IRule>> rules_;
    std::unordered_map<std::uint32_t, IdState> states_;
};

}  // namespace canguard
