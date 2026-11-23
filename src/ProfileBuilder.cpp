#include "canguard/ProfileBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace canguard {

void ProfileBuilder::add(const CanFrame& frame) {
    if (frameCount_ == 0) {
        firstTimestamp_ = frame.timestamp;
        lastTimestamp_ = frame.timestamp;
    } else {
        firstTimestamp_ = std::min(firstTimestamp_, frame.timestamp);
        lastTimestamp_ = std::max(lastTimestamp_, frame.timestamp);
    }
    ++frameCount_;

    Accumulator& state = accumulators_[frame.id];
    if (state.messageCount > 0) {
        const double interval = frame.timestamp - state.lastTimestamp;
        if (interval >= 0.0) {
            ++state.intervalCount;
            const double deviation = interval - state.intervalMean;
            state.intervalMean += deviation / static_cast<double>(state.intervalCount);
            state.intervalSquaredDeviations += deviation * (interval - state.intervalMean);
        }
    }
    const std::size_t dlc = std::min<std::size_t>(frame.dlc, 8);
    const bool comparableWithPrevious = state.messageCount > 0 && state.lastDlc == frame.dlc;

    state.lastTimestamp = frame.timestamp;
    ++state.messageCount;
    ++state.dlcCounts[dlc];
    for (std::size_t i = 0; i < dlc; ++i) {
        state.minByte[i] = std::min(state.minByte[i], frame.data[i]);
        state.maxByte[i] = std::max(state.maxByte[i], frame.data[i]);
        if (comparableWithPrevious) {
            const auto jump = static_cast<std::uint8_t>(std::max(frame.data[i], state.lastData[i]) -
                                                        std::min(frame.data[i], state.lastData[i]));
            state.maxByteJump[i] = std::max(state.maxByteJump[i], jump);
        }
    }
    state.lastData = frame.data;
    state.lastDlc = frame.dlc;
}

ProfileMap ProfileBuilder::build() const {
    const double trainingSeconds = lastTimestamp_ - firstTimestamp_;

    ProfileMap profiles;
    profiles.reserve(accumulators_.size());
    for (const auto& [id, state] : accumulators_) {
        IdProfile profile;
        profile.id = id;
        profile.messageCount = state.messageCount;
        profile.trainingSeconds = trainingSeconds;
        profile.usualDlc = static_cast<std::uint8_t>(
            std::max_element(state.dlcCounts.begin(), state.dlcCounts.end()) - state.dlcCounts.begin());
        profile.interArrivalMean = state.intervalMean;
        profile.interArrivalStd =
            state.intervalCount > 0
                ? std::sqrt(state.intervalSquaredDeviations / static_cast<double>(state.intervalCount))
                : 0.0;
        profile.minByte = state.minByte;
        profile.maxByte = state.maxByte;
        profile.maxByteJump = state.maxByteJump;
        profiles.emplace(id, profile);
    }
    return profiles;
}

}  // namespace canguard
