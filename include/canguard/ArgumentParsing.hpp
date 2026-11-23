#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace canguard {

// Whole text must be a finite decimal number; returns std::nullopt otherwise.
std::optional<double> parseDouble(std::string_view text);

// Whole text must be decimal digits that fit in 64 bits; returns std::nullopt otherwise.
std::optional<std::uint64_t> parseUnsigned(std::string_view text);

}  // namespace canguard
