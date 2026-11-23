#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace canguard {

// What normal traffic looks like for one CAN ID.
struct IdProfile {
    std::uint32_t id = 0;
    std::size_t messageCount = 0;
    double trainingSeconds = 0.0;  // time span of the whole training data, shared by every ID
    std::uint8_t usualDlc = 0;
    double interArrivalMean = 0.0;
    double interArrivalStd = 0.0;
    std::array<std::uint8_t, 8> minByte{};
    std::array<std::uint8_t, 8> maxByte{};
    std::array<std::uint8_t, 8> maxByteJump{};  // largest change of a byte between two consecutive messages

    bool isByteConstant(std::size_t index) const { return minByte[index] == maxByte[index]; }

    double expectedRatePerSecond() const {
        return trainingSeconds > 0.0 ? static_cast<double>(messageCount) / trainingSeconds : 0.0;
    }

    double expectedCountInWindow(double windowSeconds) const {
        return expectedRatePerSecond() * windowSeconds;
    }
};

// The keys of the map are the known-ID set.
using ProfileMap = std::unordered_map<std::uint32_t, IdProfile>;

}  // namespace canguard
