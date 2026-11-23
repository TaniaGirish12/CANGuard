#include <cmath>

#include "TestRegistry.hpp"
#include "canguard/ProfileBuilder.hpp"

using canguard::CanFrame;
using canguard::IdProfile;
using canguard::ProfileBuilder;
using canguard::ProfileMap;

namespace {

bool nearlyEqual(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

CanFrame makeFrame(double timestamp, std::uint32_t id, std::uint8_t dlc,
                   std::array<std::uint8_t, 8> data = {}) {
    CanFrame frame;
    frame.timestamp = timestamp;
    frame.id = id;
    frame.dlc = dlc;
    frame.data = data;
    return frame;
}

}  // namespace

TEST_CASE(emptyBuilderProducesNoProfiles) {
    CHECK(ProfileBuilder().build().empty());
}

TEST_CASE(profilesHoldKnownIdsAndMessageCounts) {
    ProfileBuilder builder;
    builder.add(makeFrame(0.0, 0x100, 2));
    builder.add(makeFrame(0.1, 0x100, 2));
    builder.add(makeFrame(0.2, 0x200, 2));

    const ProfileMap profiles = builder.build();
    CHECK(profiles.size() == 2);
    CHECK(profiles.count(0x100) == 1);
    CHECK(profiles.count(0x300) == 0);
    CHECK(profiles.at(0x100).messageCount == 2);
    CHECK(profiles.at(0x200).messageCount == 1);
}

TEST_CASE(interArrivalMeanAndStdAreComputedPerId) {
    ProfileBuilder builder;
    // ID 0x100: gaps 0.1, 0.1, 0.1. ID 0x300: gaps 0.1 and 0.3, so mean 0.2 and std 0.1.
    for (const double t : {0.0, 0.1, 0.2, 0.3}) builder.add(makeFrame(t, 0x100, 1));
    for (const double t : {0.05, 0.15, 0.45}) builder.add(makeFrame(t, 0x300, 1));

    const ProfileMap profiles = builder.build();
    CHECK(nearlyEqual(profiles.at(0x100).interArrivalMean, 0.1));
    CHECK(nearlyEqual(profiles.at(0x100).interArrivalStd, 0.0));
    CHECK(nearlyEqual(profiles.at(0x300).interArrivalMean, 0.2));
    CHECK(nearlyEqual(profiles.at(0x300).interArrivalStd, 0.1));
}

TEST_CASE(singleMessageIdHasZeroInterArrivalStatistics) {
    ProfileBuilder builder;
    builder.add(makeFrame(1.0, 0x400, 1));

    const IdProfile profile = builder.build().at(0x400);
    CHECK(profile.interArrivalMean == 0.0);
    CHECK(profile.interArrivalStd == 0.0);
}

TEST_CASE(byteRangesAndConstantBytesAreLearned) {
    ProfileBuilder builder;
    builder.add(makeFrame(0.0, 0x100, 2, {10, 20}));
    builder.add(makeFrame(0.1, 0x100, 2, {12, 20}));
    builder.add(makeFrame(0.2, 0x100, 2, {11, 20}));

    const IdProfile profile = builder.build().at(0x100);
    CHECK(profile.minByte[0] == 10);
    CHECK(profile.maxByte[0] == 12);
    CHECK(!profile.isByteConstant(0));
    CHECK(profile.minByte[1] == 20);
    CHECK(profile.maxByte[1] == 20);
    CHECK(profile.isByteConstant(1));
}

TEST_CASE(usualDlcIsTheMostFrequentOne) {
    ProfileBuilder builder;
    builder.add(makeFrame(0.0, 0x100, 8));
    builder.add(makeFrame(0.1, 0x100, 5));
    builder.add(makeFrame(0.2, 0x100, 8));

    CHECK(builder.build().at(0x100).usualDlc == 8);
}

TEST_CASE(expectedRateComesFromMessageCountOverTrainingSpan) {
    ProfileBuilder builder;
    for (const double t : {0.0, 0.1, 0.2, 0.3}) builder.add(makeFrame(t, 0x100, 1));
    builder.add(makeFrame(0.15, 0x200, 1));

    const ProfileMap profiles = builder.build();
    CHECK(nearlyEqual(profiles.at(0x100).trainingSeconds, 0.3));
    CHECK(nearlyEqual(profiles.at(0x100).expectedRatePerSecond(), 4.0 / 0.3));
    CHECK(nearlyEqual(profiles.at(0x100).expectedCountInWindow(0.1), 4.0 / 3.0));
    CHECK(nearlyEqual(profiles.at(0x200).expectedRatePerSecond(), 1.0 / 0.3));
}

TEST_CASE(expectedRateIsZeroWhenTrainingSpansNoTime) {
    ProfileBuilder builder;
    builder.add(makeFrame(5.0, 0x100, 1));

    CHECK(builder.build().at(0x100).expectedRatePerSecond() == 0.0);
}

TEST_CASE(largestByteJumpBetweenConsecutiveMessagesIsLearned) {
    ProfileBuilder builder;
    builder.add(makeFrame(0.0, 0x100, 2, {10, 20}));
    builder.add(makeFrame(0.1, 0x100, 2, {12, 20}));  // jumps 2 and 0
    builder.add(makeFrame(0.2, 0x100, 2, {11, 25}));  // jumps 1 and 5
    builder.add(makeFrame(0.3, 0x100, 2, {11, 20}));  // jumps 0 and 5

    const IdProfile profile = builder.build().at(0x100);
    CHECK(profile.maxByteJump[0] == 2);
    CHECK(profile.maxByteJump[1] == 5);
}

TEST_CASE(byteJumpsAreOnlyMeasuredBetweenMessagesWithTheSameDlc) {
    ProfileBuilder builder;
    builder.add(makeFrame(0.0, 0x100, 2, {10, 20}));
    builder.add(makeFrame(0.1, 0x100, 3, {200, 20, 0}));
    builder.add(makeFrame(0.2, 0x100, 2, {11, 20}));

    const IdProfile profile = builder.build().at(0x100);
    CHECK(profile.maxByteJump[0] == 0);
    CHECK(profile.maxByteJump[1] == 0);
}
