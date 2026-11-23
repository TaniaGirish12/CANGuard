#include "canguard/TimingRule.hpp"

#include <algorithm>

namespace canguard {

namespace {

constexpr std::size_t kMinPreviousMessages = 2;

}  // namespace

TimingRule::TimingRule(const DetectorConfig& config) : k_(config.k), r_(config.r) {}

RuleResult TimingRule::evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const {
    if (profile == nullptr || state.previousMessages < kMinPreviousMessages) return {false};

    const double interArrival = frame.timestamp - state.lastTimestamp;
    const double minimumNormal =
        std::max(profile->interArrivalMean - k_ * profile->interArrivalStd, profile->interArrivalMean * r_);
    return {interArrival < minimumNormal};
}

std::string_view TimingRule::name() const {
    return "Timing";
}

}  // namespace canguard
