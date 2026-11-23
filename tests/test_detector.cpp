#include <string_view>

#include "TestRegistry.hpp"
#include "canguard/Detector.hpp"
#include "canguard/ProfileBuilder.hpp"

using canguard::CanFrame;
using canguard::Detection;
using canguard::Detector;
using canguard::DetectorConfig;
using canguard::ProfileBuilder;
using canguard::ProfileMap;
using canguard::RuleMask;

namespace {

constexpr std::uint32_t kIdA = 0x100;
constexpr std::uint32_t kIdB = 0x200;

CanFrame makeFrame(double timestamp, std::uint32_t id, std::uint8_t byte0 = 11, std::size_t lineNumber = 0) {
    CanFrame frame;
    frame.timestamp = timestamp;
    frame.id = id;
    frame.dlc = 2;
    frame.data = {byte0, 20};
    frame.lineNumber = lineNumber;
    return frame;
}

// Gap before frame i of a stream that alternates between 9 ms and 11 ms (mean 10 ms, std 1 ms).
double gapBefore(std::size_t i) {
    return (i % 2 == 1) ? 0.009 : 0.011;
}

// Two IDs with 10 ms timing and byte 0 cycling through 10..12, trained for about one second.
ProfileMap trainTwoIds() {
    ProfileBuilder builder;
    double timestamp = 0.0;
    for (std::size_t i = 0; i < 101; ++i) {
        const auto byte0 = static_cast<std::uint8_t>(10 + i % 3);
        builder.add(makeFrame(timestamp, kIdA, byte0));
        builder.add(makeFrame(timestamp + 0.003, kIdB, byte0));
        timestamp += (i % 2 == 0) ? 0.009 : 0.011;
    }
    return builder.build();
}

RuleMask bitOf(const Detector& detector, std::string_view ruleName) {
    for (std::size_t i = 0; i < detector.ruleCount(); ++i) {
        if (detector.ruleName(i) == ruleName) return RuleMask{1} << i;
    }
    return 0;
}

// Feeds `count` normal frames of both IDs starting at `start`; returns the time of the last frame.
double feedNormalTraffic(Detector& detector, double start, std::size_t count, bool& anyAttack) {
    double timestamp = start;
    for (std::size_t i = 0; i < count; ++i) {
        timestamp += gapBefore(i);
        const auto byte0 = static_cast<std::uint8_t>(10 + i % 3);
        anyAttack |= detector.process(makeFrame(timestamp, kIdA, byte0)).predictedAttack;
        anyAttack |= detector.process(makeFrame(timestamp + 0.003, kIdB, byte0)).predictedAttack;
    }
    return timestamp;
}

}  // namespace

TEST_CASE(detectorExposesItsFourRulesInOrder) {
    const ProfileMap profiles = trainTwoIds();
    const Detector detector(profiles, DetectorConfig{});
    CHECK(detector.ruleCount() == 4);
    CHECK(detector.ruleName(0) == "UnknownId");
    CHECK(detector.ruleName(1) == "Timing");
    CHECK(detector.ruleName(2) == "Rate");
    CHECK(detector.ruleName(3) == "Payload");
}

TEST_CASE(detectorFlagsUnknownIdAndRecordsTheRule) {
    const ProfileMap profiles = trainTwoIds();
    Detector detector(profiles, DetectorConfig{});
    const Detection detection = detector.process(makeFrame(2.0, 0x7ff, 11, 42));
    CHECK(detection.predictedAttack);
    CHECK(detection.ruleMask == bitOf(detector, "UnknownId"));
    CHECK(detection.index == 42);
}

TEST_CASE(detectorLeavesNormalTrafficAlone) {
    const ProfileMap profiles = trainTwoIds();
    Detector detector(profiles, DetectorConfig{});
    bool anyAttack = false;
    feedNormalTraffic(detector, 1.5, 300, anyAttack);
    CHECK(!anyAttack);
}

TEST_CASE(detectorFlagsFloodOfAlteredMessages) {
    const ProfileMap profiles = trainTwoIds();
    Detector detector(profiles, DetectorConfig{});
    bool anyAttack = false;
    double timestamp = feedNormalTraffic(detector, 1.5, 100, anyAttack);

    Detection first;
    Detection last;
    for (int i = 0; i < 50; ++i) {
        timestamp += 0.001;
        last = detector.process(makeFrame(timestamp, kIdA, 99));
        if (i == 0) first = last;
    }

    // The first altered message arrives too soon and jumps away from the normal payload ...
    CHECK(first.predictedAttack);
    CHECK(first.ruleMask == (bitOf(detector, "Timing") | bitOf(detector, "Payload")));
    // ... the later ones repeat that payload, so only the timing and rate evidence remains.
    CHECK(last.predictedAttack);
    CHECK(last.ruleMask == (bitOf(detector, "Timing") | bitOf(detector, "Rate")));
}

TEST_CASE(detectorRequiresMinScoreRulesToFire) {
    const ProfileMap profiles = trainTwoIds();
    DetectorConfig config;
    config.minScore = 2;
    Detector detector(profiles, config);

    const Detection detection = detector.process(makeFrame(2.0, 0x7ff));  // only UnknownId can fire
    CHECK(!detection.predictedAttack);
    CHECK(detection.ruleMask == bitOf(detector, "UnknownId"));
}

// IDs are interleaved (B follows A by 3 ms), so this also shows that state is kept per ID.
TEST_CASE(detectorFlagsEarlyMessageOfOneIdOnly) {
    const ProfileMap profiles = trainTwoIds();
    Detector detector(profiles, DetectorConfig{});
    bool anyAttack = false;
    const double timestamp = feedNormalTraffic(detector, 1.5, 20, anyAttack);

    const Detection early = detector.process(makeFrame(timestamp + 0.003 + 0.002, kIdB));
    CHECK(early.predictedAttack);
    CHECK(early.ruleMask == bitOf(detector, "Timing"));

    const Detection onTime = detector.process(makeFrame(timestamp + 0.011, kIdA));
    CHECK(!onTime.predictedAttack);
}
