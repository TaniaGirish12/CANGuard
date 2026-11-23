#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace canguard {

struct CanFrame {
    double timestamp = 0.0;
    std::uint32_t id = 0;
    std::uint8_t dlc = 0;
    std::array<std::uint8_t, 8> data{};
    bool isAttack = false;
    std::size_t lineNumber = 0;
};

}  // namespace canguard
