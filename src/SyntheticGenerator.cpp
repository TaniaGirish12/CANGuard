#include "canguard/SyntheticGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <stdexcept>
#include <utility>

namespace canguard {

namespace {

using Rng = std::mt19937_64;

constexpr std::int64_t kMicrosPerSecond = 1'000'000;
constexpr std::int64_t kJitterDivisor = 50;  // each message may be up to 2% of its period early or late
constexpr std::uint8_t kNoiseRange = 8;
constexpr std::uint8_t kCounterRange = 16;
constexpr std::uint32_t kMaxStandardId = 0x800;
constexpr std::int64_t kDosPeriodMicros = 300;
constexpr std::int64_t kFuzzyPeriodMicros = 500;
constexpr std::int64_t kSpoofPeriodMicros = 1000;
constexpr std::uint8_t kSpoofAlteration = 0x80;

enum class ByteKind { Constant, Counter, Noise };

struct ByteBehaviour {
    ByteKind kind;
    std::uint8_t base;
};

ByteBehaviour constant(std::uint8_t value) { return {ByteKind::Constant, value}; }
ByteBehaviour counter() { return {ByteKind::Counter, 0}; }
ByteBehaviour noise(std::uint8_t base) { return {ByteKind::Noise, base}; }

struct SignalSpec {
    std::uint32_t id;
    std::int64_t periodMicros;
    std::vector<ByteBehaviour> bytes;  // one entry per data byte, so size() is the DLC
};

// Periodic IDs with stable payloads: constant bytes, a rolling counter, and small bounded noise.
const std::vector<SignalSpec>& normalSignals() {
    static const std::vector<SignalSpec> signals = {
        {0x316, 10'000, {constant(0x05), noise(0x20), constant(0x68), noise(0x08), noise(0x20),
                         constant(0x21), constant(0x00), counter()}},
        {0x18f, 10'000, {constant(0xfe), noise(0x50), constant(0x00), constant(0x00), constant(0x00),
                         constant(0x3c), constant(0x00), constant(0x00)}},
        {0x260, 10'000, {constant(0x19), constant(0x21), constant(0x22), constant(0x30), noise(0x00),
                         constant(0x8e), constant(0x6d), counter()}},
        {0x2a0, 20'000, {noise(0x60), constant(0x00), constant(0x9a), constant(0x1d), constant(0x97),
                         constant(0x02), constant(0xbd), constant(0x00)}},
        {0x329, 20'000, {noise(0x40), constant(0xbb), constant(0x7f), constant(0x14), constant(0x11),
                         constant(0x20), constant(0x00), constant(0x14)}},
        {0x545, 20'000, {constant(0xd8), constant(0x00), constant(0x00), noise(0x88), constant(0x00),
                         constant(0x00), constant(0x00), constant(0x00)}},
        {0x153, 20'000, {constant(0x00), constant(0x21), constant(0x10), constant(0xff), constant(0x00),
                         constant(0xff), constant(0x00), constant(0x00)}},
        {0x2b0, 50'000, {constant(0xff), constant(0x7f), constant(0x00), constant(0x05), noise(0x49)}},
        {0x350, 50'000, {constant(0x05), constant(0x20), noise(0x14), constant(0x68), noise(0x70),
                         constant(0x00), constant(0x00), noise(0x20)}},
        {0x43f, 100'000, {constant(0x00), constant(0x40), constant(0x60), constant(0xff), noise(0x78),
                          noise(0x60), constant(0x08), constant(0x00)}},
        {0x2c0, 100'000, {constant(0x14), constant(0x00)}},
    };
    return signals;
}

const SignalSpec& spoofTarget() {
    return normalSignals().front();
}

// Integer microseconds keep timestamps exact and identical across platforms.
struct Timeline {
    std::int64_t startMicros;
    std::int64_t durationMicros;

    double timestampAt(std::int64_t offsetMicros) const {
        return static_cast<double>(startMicros + offsetMicros) / static_cast<double>(kMicrosPerSecond);
    }
};

std::int64_t toMicros(double seconds) {
    return std::llround(seconds * static_cast<double>(kMicrosPerSecond));
}

// std distributions differ between standard libraries, so map raw bits ourselves.
std::uint64_t uniformBelow(Rng& rng, std::uint64_t bound) {
    return rng() % bound;
}

CanFrame makeFrame(const Timeline& timeline, std::int64_t offsetMicros, std::uint32_t id,
                   std::size_t dlc, bool isAttack) {
    CanFrame frame;
    frame.timestamp = timeline.timestampAt(offsetMicros);
    frame.id = id;
    frame.dlc = static_cast<std::uint8_t>(dlc);
    frame.isAttack = isAttack;
    return frame;
}

std::uint8_t normalByte(const ByteBehaviour& behaviour, std::int64_t messageIndex, Rng& rng) {
    switch (behaviour.kind) {
        case ByteKind::Constant:
            return behaviour.base;
        case ByteKind::Counter:
            return static_cast<std::uint8_t>(messageIndex % kCounterRange);
        case ByteKind::Noise:
            return static_cast<std::uint8_t>(behaviour.base + uniformBelow(rng, kNoiseRange));
    }
    return behaviour.base;
}

void addNormalTraffic(std::vector<CanFrame>& frames, const Timeline& timeline, Rng& rng) {
    for (const SignalSpec& signal : normalSignals()) {
        const std::int64_t maxJitter = signal.periodMicros / kJitterDivisor;
        const auto phase = static_cast<std::int64_t>(uniformBelow(rng, signal.periodMicros));

        for (std::int64_t index = 0;; ++index) {
            const auto jitter = static_cast<std::int64_t>(uniformBelow(rng, 2 * maxJitter + 1)) - maxJitter;
            const std::int64_t offset = std::max<std::int64_t>(0, phase + index * signal.periodMicros + jitter);
            if (offset >= timeline.durationMicros) break;

            CanFrame frame = makeFrame(timeline, offset, signal.id, signal.bytes.size(), false);
            for (std::size_t i = 0; i < signal.bytes.size(); ++i) {
                frame.data[i] = normalByte(signal.bytes[i], index, rng);
            }
            frames.push_back(frame);
        }
    }
}

CanFrame dosFrame(const Timeline& timeline, std::int64_t offset, Rng&) {
    return makeFrame(timeline, offset, 0x000, 8, true);  // all-zero payload on the highest priority ID
}

CanFrame fuzzyFrame(const Timeline& timeline, std::int64_t offset, Rng& rng) {
    const auto id = static_cast<std::uint32_t>(uniformBelow(rng, kMaxStandardId));
    CanFrame frame = makeFrame(timeline, offset, id, 8, true);
    for (std::uint8_t& byte : frame.data) byte = static_cast<std::uint8_t>(uniformBelow(rng, 256));
    return frame;
}

CanFrame spoofFrame(const Timeline& timeline, std::int64_t offset, Rng&) {
    const SignalSpec& target = spoofTarget();
    CanFrame frame = makeFrame(timeline, offset, target.id, target.bytes.size(), true);
    for (std::size_t i = 0; i < target.bytes.size(); ++i) {
        frame.data[i] = static_cast<std::uint8_t>(target.bytes[i].base ^ kSpoofAlteration);
    }
    return frame;
}

template <typename MakeAttackFrame>
void addBursts(std::vector<CanFrame>& frames, const Timeline& timeline, const SyntheticConfig& config,
               std::int64_t periodMicros, MakeAttackFrame makeAttackFrame, Rng& rng) {
    const std::int64_t length = toMicros(config.burstLengthSeconds);
    const std::int64_t stride = length + toMicros(config.burstGapSeconds);

    for (std::int64_t start = toMicros(config.firstBurstSeconds); start < timeline.durationMicros;
         start += stride) {
        const std::int64_t end = std::min(start + length, timeline.durationMicros);
        for (std::int64_t offset = start; offset < end; offset += periodMicros) {
            frames.push_back(makeAttackFrame(timeline, offset, rng));
        }
    }
}

void addAttackTraffic(std::vector<CanFrame>& frames, const Timeline& timeline,
                      const SyntheticConfig& config, Rng& rng) {
    switch (config.attack) {
        case AttackType::None:
            return;
        case AttackType::Dos:
            addBursts(frames, timeline, config, kDosPeriodMicros, dosFrame, rng);
            return;
        case AttackType::Fuzzy:
            addBursts(frames, timeline, config, kFuzzyPeriodMicros, fuzzyFrame, rng);
            return;
        case AttackType::Spoof:
            addBursts(frames, timeline, config, kSpoofPeriodMicros, spoofFrame, rng);
            return;
    }
}

}  // namespace

std::optional<AttackType> parseAttackType(std::string_view name) {
    if (name == "none") return AttackType::None;
    if (name == "dos") return AttackType::Dos;
    if (name == "fuzzy") return AttackType::Fuzzy;
    if (name == "spoof") return AttackType::Spoof;
    return std::nullopt;
}

SyntheticGenerator::SyntheticGenerator(SyntheticConfig config) : config_(std::move(config)) {
    if (config_.durationSeconds <= 0.0) throw std::invalid_argument("duration must be positive");
    if (config_.burstLengthSeconds <= 0.0) throw std::invalid_argument("burst length must be positive");
    if (config_.burstGapSeconds < 0.0 || config_.firstBurstSeconds < 0.0) {
        throw std::invalid_argument("burst gap and first burst time must not be negative");
    }
}

std::vector<CanFrame> SyntheticGenerator::generate() const {
    const Timeline timeline{toMicros(config_.startTimestamp), toMicros(config_.durationSeconds)};
    Rng rng(config_.seed);

    std::vector<CanFrame> frames;
    addNormalTraffic(frames, timeline, rng);
    addAttackTraffic(frames, timeline, config_, rng);

    std::stable_sort(frames.begin(), frames.end(),
                     [](const CanFrame& a, const CanFrame& b) { return a.timestamp < b.timestamp; });
    for (std::size_t i = 0; i < frames.size(); ++i) frames[i].lineNumber = i + 1;
    return frames;
}

void SyntheticGenerator::writeCsv(const std::vector<CanFrame>& frames, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output file: " + path);

    char line[128];
    for (const CanFrame& frame : frames) {
        int length = std::snprintf(line, sizeof line, "%.6f,%04x,%u", frame.timestamp,
                                   static_cast<unsigned>(frame.id), static_cast<unsigned>(frame.dlc));
        for (std::uint8_t i = 0; i < frame.dlc; ++i) {
            length += std::snprintf(line + length, sizeof line - static_cast<std::size_t>(length), ",%02x",
                                    static_cast<unsigned>(frame.data[i]));
        }
        length += std::snprintf(line + length, sizeof line - static_cast<std::size_t>(length), ",%c\n",
                                frame.isAttack ? 'T' : 'R');
        out.write(line, length);
    }
    if (!out) throw std::runtime_error("failed while writing: " + path);
}

}  // namespace canguard
