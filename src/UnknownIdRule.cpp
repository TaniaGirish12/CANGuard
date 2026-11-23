#include "canguard/UnknownIdRule.hpp"

namespace canguard {

RuleResult UnknownIdRule::evaluate(const CanFrame&, IdState&, const IdProfile* profile) const {
    return {profile == nullptr};
}

std::string_view UnknownIdRule::name() const {
    return "UnknownId";
}

}  // namespace canguard
