#include "canguard/RateRule.hpp"

#include <algorithm>

namespace canguard {

namespace {

constexpr double kMillisecondsPerSecond = 1000.0;

// Without this floor an ID slower than one message per window would expect less than one message,
// so every single normal message would exceed the limit.
constexpr double kMinimumExpectedCount = 1.0;

}  // namespace

RateRule::RateRule(const DetectorConfig& config)
    : windowSeconds_(config.windowMs / kMillisecondsPerSecond), rateFactor_(config.rateFactor) {}

RuleResult RateRule::evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const {
    // Pruned even for unknown IDs, so a flood on one never grows its state without bound.
    const double windowStart = frame.timestamp - windowSeconds_;
    while (!state.recentTimestamps.empty() && state.recentTimestamps.front() <= windowStart) {
        state.recentTimestamps.pop_front();
    }
    if (profile == nullptr) return {false};

    const double countInWindow = static_cast<double>(state.recentTimestamps.size()) + 1.0;
    const double expectedCount =
        std::max(kMinimumExpectedCount, profile->expectedCountInWindow(windowSeconds_));
    return {countInWindow > expectedCount * rateFactor_};
}

std::string_view RateRule::name() const {
    return "Rate";
}

}  // namespace canguard
