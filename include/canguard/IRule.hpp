#pragma once

#include <string_view>

#include "canguard/CanFrame.hpp"
#include "canguard/IdProfile.hpp"
#include "canguard/IdState.hpp"

namespace canguard {

struct RuleResult {
    bool fired = false;
};

class IRule {
public:
    virtual ~IRule() = default;

    // profile is nullptr when the ID was not seen in training.
    virtual RuleResult evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const = 0;

    virtual std::string_view name() const = 0;
};

}  // namespace canguard
