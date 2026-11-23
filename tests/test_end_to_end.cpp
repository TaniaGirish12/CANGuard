#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "TestRegistry.hpp"
#include "TempPath.hpp"
#include "canguard/SequentialPipeline.hpp"
#include "canguard/SyntheticGenerator.hpp"

using canguard::AttackType;
using canguard::Options;
using canguard::RunResult;
using canguard::SequentialPipeline;
using canguard::SyntheticConfig;
using canguard::SyntheticGenerator;

namespace {

// Writes a generated file into the temp folder and returns its path.
std::filesystem::path generateFile(const std::string& name, AttackType attack, double seconds, std::uint64_t seed,
                                   std::size_t* frameCount = nullptr) {
    SyntheticConfig config;
    config.attack = attack;
    config.durationSeconds = seconds;
    config.seed = seed;
    const auto frames = SyntheticGenerator(config).generate();
    if (frameCount != nullptr) *frameCount = frames.size();

    const auto path = canguard::testing::uniqueTempPath("e2e_" + name + ".csv");
    SyntheticGenerator::writeCsv(frames, path.string());
    return path;
}

RunResult runTrainingFromInput(const std::filesystem::path& input, double trainingSeconds) {
    Options options;
    options.inputPath = input.string();
    options.trainFromInputSeconds = trainingSeconds;
    return SequentialPipeline().run(options);
}

}  // namespace

TEST_CASE(endToEndDosFileTrainedFromItsOwnInputReachesRecallOfAtLeast90Percent) {
    const auto input = generateFile("dos", AttackType::Dos, 60.0, 7);
    const RunResult result = runTrainingFromInput(input, 5.0);
    std::filesystem::remove(input);

    CHECK(result.metrics.truePositives + result.metrics.falseNegatives == 50000);
    CHECK(result.metrics.recall >= 0.9);
    CHECK(result.metrics.precision >= 0.9);
}

TEST_CASE(endToEndFuzzyAndSpoofFilesAreDetectedToo) {
    for (const AttackType attack : {AttackType::Fuzzy, AttackType::Spoof}) {
        const auto input = generateFile("other_attacks", attack, 60.0, 11);
        const RunResult result = runTrainingFromInput(input, 5.0);
        std::filesystem::remove(input);

        CHECK(result.metrics.recall >= 0.9);
        CHECK(result.metrics.falsePositiveRate <= 0.05);
    }
}

TEST_CASE(endToEndTrainsOnASeparateAttackFreeFile) {
    const auto normal = generateFile("normal", AttackType::None, 30.0, 3);
    const auto input = generateFile("with_attack", AttackType::Dos, 60.0, 4);

    Options options;
    options.trainPath = normal.string();
    options.inputPath = input.string();
    const RunResult result = SequentialPipeline().run(options);
    std::filesystem::remove(normal);
    std::filesystem::remove(input);

    CHECK(result.metrics.recall >= 0.9);
    CHECK(result.metrics.precision >= 0.9);
}

TEST_CASE(endToEndReportsCountsTimingsAndAlerts) {
    std::size_t expectedFrames = 0;
    const auto input = generateFile("counts", AttackType::Dos, 40.0, 5, &expectedFrames);
    const RunResult result = runTrainingFromInput(input, 5.0);
    std::filesystem::remove(input);

    CHECK(result.frameCount == expectedFrames);
    CHECK(result.skippedLines == 0);
    CHECK(result.alerts.size() == result.metrics.truePositives + result.metrics.falsePositives);
    CHECK(result.ruleNames.size() == 4);

    CHECK(result.timings.size() == 4);
    if (result.timings.size() == 4) {
        CHECK(result.timings[0].name == "read+parse");
        CHECK(result.timings[1].name == "train");
        CHECK(result.timings[2].name == "detect");
        CHECK(result.timings[3].name == "evaluate");
    }
}

TEST_CASE(endToEndCountsMalformedInputLines) {
    const auto input = generateFile("malformed", AttackType::Dos, 20.0, 6);
    {
        std::ofstream append(input, std::ios::app | std::ios::binary);
        append << "this is not a CAN message\n1478198999.0,0316,8,01\n";
    }
    const RunResult result = runTrainingFromInput(input, 5.0);
    std::filesystem::remove(input);

    CHECK(result.skippedLines == 2);
}

TEST_CASE(endToEndFailsClearlyWhenThereIsNothingToTrainOn) {
    const auto onlyAttacks = canguard::testing::uniqueTempPath("e2e_only_attacks.csv");
    std::ofstream(onlyAttacks) << "1478198376.000000,0000,8,00,00,00,00,00,00,00,00,T\n";

    bool threw = false;
    try {
        runTrainingFromInput(onlyAttacks, 5.0);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("no normal") != std::string::npos;
    }
    std::filesystem::remove(onlyAttacks);
    CHECK(threw);
}

TEST_CASE(endToEndFailsClearlyWhenTheInputFileIsMissing) {
    bool threw = false;
    try {
        runTrainingFromInput("/nonexistent/canguard_missing.csv", 5.0);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("cannot open file") != std::string::npos;
    }
    CHECK(threw);
}
