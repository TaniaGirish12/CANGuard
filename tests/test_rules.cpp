#include "TestRegistry.hpp"
#include "canguard/DetectorConfig.hpp"
#include "canguard/PayloadRule.hpp"
#include "canguard/ProfileBuilder.hpp"
#include "canguard/RateRule.hpp"
#include "canguard/TimingRule.hpp"
#include "canguard/UnknownIdRule.hpp"

using canguard::CanFrame;
using canguard::DetectorConfig;
using canguard::IdProfile;
using canguard::IdState;
using canguard::PayloadRule;
using canguard::ProfileBuilder;
using canguard::ProfileMap;
using canguard::RateRule;
using canguard::TimingRule;
using canguard::UnknownIdRule;

namespace {

constexpr std::uint32_t kKnownId = 0x100;

CanFrame makeFrame(double timestamp, std::uint32_t id, std::uint8_t dlc,
                   std::array<std::uint8_t, 8> data = {}) {
    CanFrame frame;
    frame.timestamp = timestamp;
    frame.id = id;
    frame.dlc = dlc;
    frame.data = data;
    return frame;
}

// Trains one ID whose messages alternate between two gaps. Byte 0 cycles through 10..12, byte 1 is
// always 20, and the DLC is 2.
ProfileMap trainOnAlternatingGaps(std::uint32_t id, double firstGap, double secondGap,
                                  std::size_t frameCount = 101) {
    ProfileBuilder builder;
    double timestamp = 0.0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        builder.add(makeFrame(timestamp, id, 2, {static_cast<std::uint8_t>(10 + i % 3), 20}));
        timestamp += (i % 2 == 0) ? firstGap : secondGap;
    }
    return builder.build();
}

// Normal inter-arrival of 10 ms with a standard deviation of 1 ms, so the timing limit is 7 ms.
ProfileMap trainTightTiming() {
    return trainOnAlternatingGaps(kKnownId, 0.009, 0.011);
}

IdState stateAfter(std::size_t previousMessages, double lastTimestamp) {
    IdState state;
    state.previousMessages = previousMessages;
    state.lastTimestamp = lastTimestamp;
    return state;
}

// Fills the window with `count` earlier messages, `spacing` seconds apart, ending just before `now`.
IdState stateWithRecentMessages(double now, std::size_t count, double spacing) {
    IdState state;
    for (std::size_t i = count; i >= 1; --i) {
        state.recentTimestamps.push_back(now - spacing * static_cast<double>(i));
    }
    state.previousMessages = count;
    state.lastTimestamp = now - spacing;
    return state;
}

// State of an ID whose previous message carried `lastData`.
IdState stateAfterPayload(std::array<std::uint8_t, 8> lastData, std::uint8_t lastDlc = 2) {
    IdState state;
    state.previousMessages = 1;
    state.lastDlc = lastDlc;
    state.lastData = lastData;
    return state;
}

}  // namespace

TEST_CASE(detectorConfigDefaultsMatchTheSpecification) {
    const DetectorConfig config;
    CHECK(config.k == 3.0);
    CHECK(config.r == 0.2);
    CHECK(config.windowMs == 100.0);
    CHECK(config.rateFactor == 3.0);
    CHECK(config.minScore == 1);
}

TEST_CASE(idStateRecordTracksLastTimestampCountWindowAndPayload) {
    IdState state;
    state.record(makeFrame(1.0, kKnownId, 2, {10, 20}));
    state.record(makeFrame(1.5, kKnownId, 2, {11, 21}));
    CHECK(state.lastTimestamp == 1.5);
    CHECK(state.previousMessages == 2);
    CHECK(state.recentTimestamps.size() == 2);
    CHECK(state.lastDlc == 2);
    CHECK(state.lastData[0] == 11);
    CHECK(state.lastData[1] == 21);
}

TEST_CASE(ruleNamesMatchTheConsoleOutput) {
    const DetectorConfig config;
    CHECK(UnknownIdRule().name() == "UnknownId");
    CHECK(TimingRule(config).name() == "Timing");
    CHECK(RateRule(config).name() == "Rate");
    CHECK(PayloadRule().name() == "Payload");
}

TEST_CASE(unknownIdRuleFiresOnlyForIdsOutsideTraining) {
    const ProfileMap profiles = trainTightTiming();
    IdState state;
    CHECK(UnknownIdRule().evaluate(makeFrame(1.0, 0x999, 2), state, nullptr).fired);
    CHECK(!UnknownIdRule().evaluate(makeFrame(1.0, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(timingRuleFiresWhenMessageArrivesTooSoon) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfter(2, 5.0);
    const TimingRule rule{DetectorConfig{}};
    CHECK(rule.evaluate(makeFrame(5.003, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(timingRuleDoesNotFireOnNormalInterArrival) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfter(2, 5.0);
    const TimingRule rule{DetectorConfig{}};
    CHECK(!rule.evaluate(makeFrame(5.010, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
    CHECK(!rule.evaluate(makeFrame(5.020, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(timingRuleNeedsTwoPreviousMessagesAndAProfile) {
    const ProfileMap profiles = trainTightTiming();
    const TimingRule rule{DetectorConfig{}};
    const CanFrame tooSoon = makeFrame(5.001, kKnownId, 2);

    IdState oneMessage = stateAfter(1, 5.0);
    CHECK(!rule.evaluate(tooSoon, oneMessage, &profiles.at(kKnownId)).fired);

    IdState twoMessages = stateAfter(2, 5.0);
    CHECK(rule.evaluate(tooSoon, twoMessages, &profiles.at(kKnownId)).fired);
    CHECK(!rule.evaluate(tooSoon, twoMessages, nullptr).fired);
}

TEST_CASE(timingRuleFallsBackToFractionOfMeanWhenStdIsLarge) {
    // mean 10 ms, std 8 ms: mean - 3 * std is negative, so the limit is r * mean = 2 ms.
    const ProfileMap profiles = trainOnAlternatingGaps(kKnownId, 0.002, 0.018);
    IdState state = stateAfter(2, 5.0);
    const TimingRule rule{DetectorConfig{}};
    CHECK(rule.evaluate(makeFrame(5.001, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
    CHECK(!rule.evaluate(makeFrame(5.003, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(timingRuleHonoursKSetting) {
    const ProfileMap profiles = trainTightTiming();
    DetectorConfig strict;
    strict.k = 1.0;  // limit becomes 9 ms
    IdState state = stateAfter(2, 5.0);
    const CanFrame frame = makeFrame(5.008, kKnownId, 2);
    CHECK(TimingRule(strict).evaluate(frame, state, &profiles.at(kKnownId)).fired);
    CHECK(!TimingRule(DetectorConfig{}).evaluate(frame, state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(rateRuleFiresWhenIdFloodsTheWindow) {
    // Expected about 10 messages per 100 ms, so the limit is about 30. Here 36 arrive within 35 ms.
    const ProfileMap profiles = trainTightTiming();
    const double now = 10.0;
    IdState state = stateWithRecentMessages(now, 35, 0.001);
    CHECK(RateRule(DetectorConfig{}).evaluate(makeFrame(now, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(rateRuleDoesNotFireAtTheNormalRate) {
    const ProfileMap profiles = trainTightTiming();
    const double now = 10.0;
    IdState state = stateWithRecentMessages(now, 9, 0.010);
    CHECK(!RateRule(DetectorConfig{}).evaluate(makeFrame(now, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(rateRuleIgnoresMessagesOutsideTheWindow) {
    const ProfileMap profiles = trainTightTiming();
    const double now = 10.0;
    IdState state = stateWithRecentMessages(now - 1.0, 100, 0.001);  // all more than 900 ms old
    CHECK(!RateRule(DetectorConfig{}).evaluate(makeFrame(now, kKnownId, 2), state, &profiles.at(kKnownId)).fired);
    CHECK(state.recentTimestamps.empty());
}

TEST_CASE(rateRuleHonoursWindowAndRateFactorSettings) {
    const ProfileMap profiles = trainTightTiming();
    const double now = 10.0;
    const CanFrame frame = makeFrame(now, kKnownId, 2);

    DetectorConfig lowFactor;
    lowFactor.rateFactor = 1.0;  // limit about 10.1: 15 messages in the window is too many
    IdState busy = stateWithRecentMessages(now, 14, 0.005);
    CHECK(RateRule(lowFactor).evaluate(frame, busy, &profiles.at(kKnownId)).fired);

    DetectorConfig shortWindow;
    shortWindow.windowMs = 10.0;  // limit about 3 messages per 10 ms: the same 15 messages are mostly outside
    IdState sameTraffic = stateWithRecentMessages(now, 14, 0.005);
    CHECK(!RateRule(shortWindow).evaluate(frame, sameTraffic, &profiles.at(kKnownId)).fired);
}

TEST_CASE(rateRuleNeverFiresOnASingleMessageOfASlowId) {
    // One message per second: the expected count per 100 ms is 0.11, yet one message must stay normal.
    const ProfileMap profiles = trainOnAlternatingGaps(0x200, 1.0, 1.0, 11);
    const double now = 20.0;
    const CanFrame frame = makeFrame(now, 0x200, 2);

    IdState quiet;
    CHECK(!RateRule(DetectorConfig{}).evaluate(frame, quiet, &profiles.at(0x200)).fired);

    IdState burst = stateWithRecentMessages(now, 4, 0.01);
    CHECK(RateRule(DetectorConfig{}).evaluate(frame, burst, &profiles.at(0x200)).fired);
}

TEST_CASE(rateRuleDoesNotFireWithoutAProfileButStillPrunesState) {
    IdState state = stateWithRecentMessages(5.0, 100, 0.001);  // all long outside the window
    CHECK(!RateRule(DetectorConfig{}).evaluate(makeFrame(10.0, 0x999, 2), state, nullptr).fired);
    CHECK(state.recentTimestamps.empty());
}

// trainTightTiming(): byte 0 cycles 10, 11, 12 (largest jump 2, so jumps up to 2 * 2 + 4 = 8 are normal);
// byte 1 is always 20 (largest jump 0, so jumps up to 4 are normal).

TEST_CASE(payloadRuleDoesNotFireOnTrainedPayloads) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfterPayload({10, 20});
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {12, 20}), state, &profiles.at(kKnownId)).fired);
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {10, 20}), state, &profiles.at(kKnownId)).fired);
}

TEST_CASE(payloadRuleFiresOnJumpToValueOutsideTrainedRange) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfterPayload({11, 20});
    const IdProfile* profile = &profiles.at(kKnownId);
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {99, 20}), state, profile).fired);
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {0, 20}), state, profile).fired);
}

TEST_CASE(payloadRuleIgnoresSlowDriftOutsideTheTrainedRange) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfterPayload({12, 20});
    const IdProfile* profile = &profiles.at(kKnownId);
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {13, 20}), state, profile).fired);  // jump 1
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {20, 20}), state, profile).fired);  // jump 8, the limit
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {21, 20}), state, profile).fired);   // jump 9
}

TEST_CASE(payloadRuleFiresWhenConstantByteChangesByMoreThanTheSlack) {
    const ProfileMap profiles = trainTightTiming();
    IdState state = stateAfterPayload({11, 20});
    const IdProfile* profile = &profiles.at(kKnownId);
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {11, 24}), state, profile).fired);  // jump 4
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 2, {11, 25}), state, profile).fired);   // jump 5
}

TEST_CASE(payloadRuleIgnoresJumpThatLandsInsideTheTrainedRange) {
    // Byte 0 climbs 0, 10, ..., 100 in training: range [0, 100], largest jump 10, so up to 24 is normal.
    ProfileBuilder builder;
    for (std::size_t i = 0; i <= 10; ++i) {
        builder.add(makeFrame(static_cast<double>(i) * 0.01, kKnownId, 1, {static_cast<std::uint8_t>(i * 10)}));
    }
    const ProfileMap profiles = builder.build();
    const IdProfile* profile = &profiles.at(kKnownId);

    IdState state = stateAfterPayload({0}, 1);
    CHECK(!PayloadRule().evaluate(makeFrame(2.0, kKnownId, 1, {90}), state, profile).fired);   // jump 90, but in range
    CHECK(PayloadRule().evaluate(makeFrame(2.0, kKnownId, 1, {150}), state, profile).fired);   // jump 150, out of range
}

TEST_CASE(payloadRuleNeedsAComparablePreviousMessageToMeasureAJump) {
    const ProfileMap profiles = trainTightTiming();
    const IdProfile* profile = &profiles.at(kKnownId);
    const CanFrame farAway = makeFrame(1.0, kKnownId, 2, {99, 20});

    IdState firstMessage;
    CHECK(!PayloadRule().evaluate(farAway, firstMessage, profile).fired);

    IdState otherDlc = stateAfterPayload({11, 20, 0}, 3);
    CHECK(!PayloadRule().evaluate(farAway, otherDlc, profile).fired);
}

TEST_CASE(payloadRuleFiresWhenDlcDiffersFromUsual) {
    const ProfileMap profiles = trainTightTiming();
    IdState state;
    const IdProfile* profile = &profiles.at(kKnownId);
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 3, {11, 20, 0}), state, profile).fired);
    CHECK(PayloadRule().evaluate(makeFrame(1.0, kKnownId, 1, {11}), state, profile).fired);
}

TEST_CASE(payloadRuleDoesNotFireWithoutAProfile) {
    IdState state = stateAfterPayload({0, 0});
    CHECK(!PayloadRule().evaluate(makeFrame(1.0, 0x999, 2, {255, 255}), state, nullptr).fired);
}
