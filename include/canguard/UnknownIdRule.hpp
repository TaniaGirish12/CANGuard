#pragma once

#include "canguard/IRule.hpp"

namespace canguard {

// Fires when the ID never appeared in training.
class UnknownIdRule final : public IRule {
public:
    RuleResult evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const override;
    std::string_view name() const override;
};

}  // namespace canguard
