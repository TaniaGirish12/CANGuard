#include "canguard/PayloadRule.hpp"

#include <algorithm>
#include <cstdlib>

namespace canguard {

namespace {

constexpr int kJumpFactor = 2;
constexpr int kJumpSlack = 4;

}  // namespace

RuleResult PayloadRule::evaluate(const CanFrame& frame, IdState& state, const IdProfile* profile) const {
    if (profile == nullptr) return {false};
    if (frame.dlc != profile->usualDlc) return {true};
    if (state.previousMessages == 0 || state.lastDlc != frame.dlc) return {false};

    const std::size_t byteCount = std::min<std::size_t>(frame.dlc, frame.data.size());
    for (std::size_t i = 0; i < byteCount; ++i) {
        const int jump = std::abs(static_cast<int>(frame.data[i]) - static_cast<int>(state.lastData[i]));
        const int largestNormalJump = kJumpFactor * profile->maxByteJump[i] + kJumpSlack;
        const bool outsideTrainedRange = frame.data[i] < profile->minByte[i] || frame.data[i] > profile->maxByte[i];
        if (jump > largestNormalJump && outsideTrainedRange) return {true};
    }
    return {false};
}

std::string_view PayloadRule::name() const {
    return "Payload";
}

}  // namespace canguard
