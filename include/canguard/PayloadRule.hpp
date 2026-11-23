#pragma once

#include "canguard/IRule.hpp"

namespace canguard {

// Fires when the DLC differs from the usual one, or when a data byte both
//   - jumps, changing by more than twice the largest change seen between two consecutive
//     messages of this ID in training, plus 4, and
//   - lands outside the [min, max] it had in training.
// Slow drift of a sensor value never jumps, and a byte that was constant in training has min == max,
// so a sudden change in it is caught. The first message of an ID has nothing to jump from.
//
// Testing a byte against the trained range alone flags most normal traffic on real recordings,
// because sensor values keep moving outside whatever range the training window happened to cover.
class PayloadRule final : public IRule {
public:
    RuleResult evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const override;
    std::string_view name() const override;
};

}  // namespace canguard
