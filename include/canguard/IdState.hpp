#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

#include "canguard/CanFrame.hpp"

namespace canguard {

// Runtime state of one CAN ID. Rules see it as it was before the current message;
// the detector calls record() once every rule has run.
struct IdState {
    double lastTimestamp = 0.0;
    std::size_t previousMessages = 0;
    std::deque<double> recentTimestamps;
    std::array<std::uint8_t, 8> lastData{};
    std::uint8_t lastDlc = 0;

    void record(const CanFrame& frame) {
        lastTimestamp = frame.timestamp;
        ++previousMessages;
        recentTimestamps.push_back(frame.timestamp);
        lastData = frame.data;
        lastDlc = frame.dlc;
    }
};

}  // namespace canguard
