#pragma once

#include "canguard/DetectorConfig.hpp"
#include "canguard/IRule.hpp"

namespace canguard {

// Fires when an ID sends too many messages inside the sliding window:
// count in the last window (this message included) > expected count * rateFactor.
// Drops timestamps that have left the window from the ID's state.
class RateRule final : public IRule {
public:
    explicit RateRule(const DetectorConfig& config);

    RuleResult evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const override;
    std::string_view name() const override;

private:
    double windowSeconds_;
    double rateFactor_;
};

}  // namespace canguard
