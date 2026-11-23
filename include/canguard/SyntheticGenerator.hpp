#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canguard/CanFrame.hpp"

namespace canguard {

enum class AttackType { None, Dos, Fuzzy, Spoof };

// Returns std::nullopt for names other than dos, fuzzy, spoof or none.
std::optional<AttackType> parseAttackType(std::string_view name);

struct SyntheticConfig {
    double durationSeconds = 60.0;
    AttackType attack = AttackType::None;
    std::uint64_t seed = 1;
    double startTimestamp = 1478198376.0;

    // Attack bursts start after a clean lead-in so --train-from-input has normal traffic to learn.
    double firstBurstSeconds = 10.0;
    double burstLengthSeconds = 3.0;
    double burstGapSeconds = 7.0;
};

class SyntheticGenerator {
public:
    // Throws std::invalid_argument for a non-positive duration or burst length.
    explicit SyntheticGenerator(SyntheticConfig config);

    // Same config and seed always produce the same frames, on every platform.
    std::vector<CanFrame> generate() const;

    // Writes the attack-format CSV (no header). Throws std::runtime_error on I/O failure.
    static void writeCsv(const std::vector<CanFrame>& frames, const std::string& path);

private:
    SyntheticConfig config_;
};

}  // namespace canguard
