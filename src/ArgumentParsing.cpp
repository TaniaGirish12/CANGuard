#include "canguard/ArgumentParsing.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>

namespace canguard {

std::optional<double> parseDouble(std::string_view text) {
    if (text.empty() || std::isspace(static_cast<unsigned char>(text.front()))) return std::nullopt;

    const std::string terminated(text);
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(terminated.c_str(), &end);
    if (end != terminated.c_str() + terminated.size() || errno != 0 || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint64_t> parseUnsigned(std::string_view text) {
    const bool onlyDigits = std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    if (text.empty() || !onlyDigits) return std::nullopt;

    const std::string terminated(text);
    errno = 0;
    const unsigned long long value = std::strtoull(terminated.c_str(), nullptr, 10);
    if (errno != 0) return std::nullopt;
    return value;
}

}  // namespace canguard
