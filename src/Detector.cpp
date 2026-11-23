#include "canguard/Detector.hpp"

#include "canguard/PayloadRule.hpp"
#include "canguard/RateRule.hpp"
#include "canguard/TimingRule.hpp"
#include "canguard/UnknownIdRule.hpp"

namespace canguard {

Detector::Detector(const ProfileMap& profiles, const DetectorConfig& config)
    : profiles_(profiles), config_(config) {
    rules_.push_back(std::make_unique<UnknownIdRule>());
    rules_.push_back(std::make_unique<TimingRule>(config));
    rules_.push_back(std::make_unique<RateRule>(config));
    rules_.push_back(std::make_unique<PayloadRule>());
}

Detection Detector::process(const CanFrame& frame) {
    IdState& state = states_[frame.id];
    const auto profileIterator = profiles_.find(frame.id);
    const IdProfile* profile = profileIterator == profiles_.end() ? nullptr : &profileIterator->second;

    Detection detection;
    detection.index = frame.lineNumber;
    std::size_t firedRules = 0;
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        if (rules_[i]->evaluate(frame, state, profile).fired) {
            detection.ruleMask |= RuleMask{1} << i;
            ++firedRules;
        }
    }
    state.record(frame);

    detection.predictedAttack = firedRules >= config_.minScore;
    return detection;
}

std::size_t Detector::ruleCount() const {
    return rules_.size();
}

std::string_view Detector::ruleName(std::size_t ruleIndex) const {
    return rules_.at(ruleIndex)->name();
}

}  // namespace canguard
