#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "canguard/CanFrame.hpp"
#include "canguard/IdProfile.hpp"

namespace canguard {

// Learns one IdProfile per CAN ID from normal frames fed in timestamp order.
class ProfileBuilder {
public:
    void add(const CanFrame& frame);
    ProfileMap build() const;

private:
    struct Accumulator {
        std::size_t messageCount = 0;
        double lastTimestamp = 0.0;

        // Running mean and sum of squared deviations of the inter-arrival times (Welford).
        std::size_t intervalCount = 0;
        double intervalMean = 0.0;
        double intervalSquaredDeviations = 0.0;

        std::array<std::size_t, 9> dlcCounts{};
        std::array<std::uint8_t, 8> minByte = filledArray(0xFF);
        std::array<std::uint8_t, 8> maxByte = filledArray(0x00);

        std::array<std::uint8_t, 8> lastData{};
        std::uint8_t lastDlc = 0;
        std::array<std::uint8_t, 8> maxByteJump{};

        static std::array<std::uint8_t, 8> filledArray(std::uint8_t value) {
            std::array<std::uint8_t, 8> bytes;
            bytes.fill(value);
            return bytes;
        }
    };

    std::unordered_map<std::uint32_t, Accumulator> accumulators_;
    std::size_t frameCount_ = 0;
    double firstTimestamp_ = 0.0;
    double lastTimestamp_ = 0.0;
};

}  // namespace canguard
