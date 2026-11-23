#pragma once

#include "canguard/DetectorConfig.hpp"
#include "canguard/IRule.hpp"

namespace canguard {

// Fires when a message arrives much sooner after the previous one of the same ID than it normally does:
// inter-arrival < max(mean - k * std, mean * r). Needs two earlier messages of the ID.
class TimingRule final : public IRule {
public:
    explicit TimingRule(const DetectorConfig& config);

    RuleResult evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const override;
    std::string_view name() const override;

private:
    double k_;
    double r_;
};

}  // namespace canguard
