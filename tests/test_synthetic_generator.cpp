#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <vector>

#include "TestRegistry.hpp"
#include "TempPath.hpp"
#include "canguard/CanLogReader.hpp"
#include "canguard/SyntheticGenerator.hpp"

using canguard::AttackType;
using canguard::CanFrame;
using canguard::SyntheticConfig;
using canguard::SyntheticGenerator;

namespace {

SyntheticConfig makeConfig(AttackType attack, double seconds = 40.0, std::uint64_t seed = 1) {
    SyntheticConfig config;
    config.attack = attack;
    config.durationSeconds = seconds;
    config.seed = seed;
    return config;
}

std::vector<CanFrame> attackFrames(const std::vector<CanFrame>& frames) {
    std::vector<CanFrame> attacks;
    std::copy_if(frames.begin(), frames.end(), std::back_inserter(attacks),
                 [](const CanFrame& f) { return f.isAttack; });
    return attacks;
}

bool sameFrame(const CanFrame& a, const CanFrame& b) {
    return a.timestamp == b.timestamp && a.id == b.id && a.dlc == b.dlc && a.data == b.data &&
           a.isAttack == b.isAttack;
}

bool sameFrames(const std::vector<CanFrame>& a, const std::vector<CanFrame>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), sameFrame);
}

}  // namespace

TEST_CASE(sameSeedGivesIdenticalFrames) {
    const auto first = SyntheticGenerator(makeConfig(AttackType::Fuzzy)).generate();
    const auto second = SyntheticGenerator(makeConfig(AttackType::Fuzzy)).generate();
    CHECK(!first.empty());
    CHECK(sameFrames(first, second));
}

TEST_CASE(differentSeedsGiveDifferentFrames) {
    const auto first = SyntheticGenerator(makeConfig(AttackType::Fuzzy, 40.0, 1)).generate();
    const auto second = SyntheticGenerator(makeConfig(AttackType::Fuzzy, 40.0, 2)).generate();
    CHECK(!sameFrames(first, second));
}

TEST_CASE(framesAreInTimestampOrder) {
    const auto frames = SyntheticGenerator(makeConfig(AttackType::Spoof)).generate();
    CHECK(std::is_sorted(frames.begin(), frames.end(),
                         [](const CanFrame& a, const CanFrame& b) { return a.timestamp < b.timestamp; }));
}

TEST_CASE(noAttackModeProducesOnlyNormalFrames) {
    const auto frames = SyntheticGenerator(makeConfig(AttackType::None)).generate();
    CHECK(frames.size() > 10000);
    CHECK(attackFrames(frames).empty());
}

TEST_CASE(dosBurstsUseIdZeroEvery300Microseconds) {
    // 40 s with bursts at 10, 20 and 30 s, each 3 s long: 3 * (3 s / 0.3 ms) frames.
    const auto frames = SyntheticGenerator(makeConfig(AttackType::Dos)).generate();
    const auto attacks = attackFrames(frames);
    CHECK(attacks.size() == 30000);
    if (attacks.size() != 30000) return;

    CHECK(std::all_of(attacks.begin(), attacks.end(), [](const CanFrame& f) { return f.id == 0 && f.dlc == 8; }));
    CHECK(std::abs(attacks[1].timestamp - attacks[0].timestamp - 0.0003) < 1e-7);
    CHECK(std::abs(attacks.front().timestamp - frames.front().timestamp - 10.0) < 0.1);
}

TEST_CASE(attacksStartAfterTheCleanLeadIn) {
    const auto frames = SyntheticGenerator(makeConfig(AttackType::Dos)).generate();
    const double leadInEnd = frames.front().timestamp + 9.9;
    const bool noAttackInLeadIn = std::none_of(frames.begin(), frames.end(), [&](const CanFrame& f) {
        return f.isAttack && f.timestamp < leadInEnd;
    });
    CHECK(noAttackInLeadIn);
}

TEST_CASE(fuzzyBurstsUseManyRandomIdsEvery500Microseconds) {
    const auto attacks = attackFrames(SyntheticGenerator(makeConfig(AttackType::Fuzzy)).generate());
    CHECK(attacks.size() == 18000);
    if (attacks.size() < 2) return;

    std::set<std::uint32_t> ids;
    for (const CanFrame& frame : attacks) ids.insert(frame.id);
    CHECK(ids.size() > 500);
    CHECK(std::abs(attacks[1].timestamp - attacks[0].timestamp - 0.0005) < 1e-7);
}

TEST_CASE(spoofBurstsReuseAKnownIdWithAlteredData) {
    const auto frames = SyntheticGenerator(makeConfig(AttackType::Spoof)).generate();
    const auto attacks = attackFrames(frames);
    CHECK(attacks.size() == 9000);
    if (attacks.empty()) return;

    const std::uint32_t spoofedId = attacks.front().id;
    CHECK(std::all_of(attacks.begin(), attacks.end(), [&](const CanFrame& f) { return f.id == spoofedId; }));

    const auto normal = std::find_if(frames.begin(), frames.end(), [&](const CanFrame& f) {
        return !f.isAttack && f.id == spoofedId;
    });
    CHECK(normal != frames.end());
    if (normal != frames.end()) CHECK(normal->data[0] != attacks.front().data[0]);
}

TEST_CASE(rejectsInvalidConfiguration) {
    bool zeroDurationThrew = false;
    try {
        SyntheticGenerator(makeConfig(AttackType::None, 0.0));
    } catch (const std::invalid_argument&) {
        zeroDurationThrew = true;
    }
    CHECK(zeroDurationThrew);
}

TEST_CASE(parsesAttackTypeNames) {
    CHECK(canguard::parseAttackType("dos") == AttackType::Dos);
    CHECK(canguard::parseAttackType("fuzzy") == AttackType::Fuzzy);
    CHECK(canguard::parseAttackType("spoof") == AttackType::Spoof);
    CHECK(canguard::parseAttackType("none") == AttackType::None);
    CHECK(!canguard::parseAttackType("flood").has_value());
}

TEST_CASE(writtenCsvIsReadBackByCanLogReader) {
    const auto frames = SyntheticGenerator(makeConfig(AttackType::Spoof, 20.0)).generate();
    const auto path = canguard::testing::uniqueTempPath("synthetic.csv");
    SyntheticGenerator::writeCsv(frames, path.string());
    const canguard::ReadResult result = canguard::CanLogReader(path.string()).readAll();
    std::filesystem::remove(path);

    CHECK(result.skippedLines == 0);
    CHECK(result.frames.size() == frames.size());
    if (result.frames.size() != frames.size()) return;
    CHECK(sameFrames(result.frames, frames));
}
