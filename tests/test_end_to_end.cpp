// End-to-end tests run the whole program on the real HCRL Car-Hacking recordings.
// The dataset is not part of the repository: download it into data/raw (see the Dataset section of the README).
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "TestRegistry.hpp"
#include "TempPath.hpp"
#include "canguard/SequentialPipeline.hpp"

#ifndef CANGUARD_DATA_DIR
#error "CMake must define CANGUARD_DATA_DIR (the data/raw folder)"
#endif

using canguard::Options;
using canguard::RunResult;
using canguard::SequentialPipeline;

namespace {

// Number of messages and of attack messages in the published DoS_dataset.csv.
constexpr std::size_t kDosFrames = 3665771;
constexpr std::size_t kDosAttackFrames = 587521;

// Path of a dataset file. A missing file fails the test with a message that says what to do.
std::filesystem::path datasetFile(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path(CANGUARD_DATA_DIR) / name;
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("dataset file not found: " + path.string() +
                                 ". Download the HCRL Car-Hacking dataset into data/raw (see the Dataset section of the README).");
    }
    return path;
}

RunResult runTrainingFromInput(const std::filesystem::path& input, double trainingSeconds) {
    Options options;
    options.inputPath = input.string();
    options.trainFromInputSeconds = trainingSeconds;
    return SequentialPipeline().run(options);
}

// Copies the first `lines` lines of a file into a new temp file and returns its path.
std::filesystem::path firstLinesCopy(const std::filesystem::path& source, std::size_t lines, const std::string& name) {
    const auto path = canguard::testing::uniqueTempPath(name);
    std::ifstream in(source, std::ios::binary);
    std::ofstream out(path, std::ios::binary);
    std::string line;
    for (std::size_t i = 0; i < lines && std::getline(in, line); ++i) out << line << '\n';
    return path;
}

}  // namespace

TEST_CASE(endToEndDosFileTrainedFromItsOwnInputIsDetectedAlmostPerfectly) {
    const RunResult result = runTrainingFromInput(datasetFile("DoS_dataset.csv"), 60.0);

    CHECK(result.metrics.truePositives + result.metrics.falseNegatives == kDosAttackFrames);
    CHECK(result.metrics.recall >= 0.99);
    CHECK(result.metrics.precision >= 0.99);
}

TEST_CASE(endToEndFuzzyAndSpoofingFilesAreDetectedToo) {
    for (const char* name : {"Fuzzy_dataset.csv", "gear_dataset.csv", "RPM_dataset.csv"}) {
        const RunResult result = runTrainingFromInput(datasetFile(name), 60.0);

        CHECK(result.metrics.recall >= 0.95);
        CHECK(result.metrics.precision >= 0.85);
    }
}

TEST_CASE(endToEndTrainsOnTheSeparateAttackFreeRecording) {
    Options options;
    options.trainPath = datasetFile("normal_run_data.txt").string();
    options.inputPath = datasetFile("DoS_dataset.csv").string();
    const RunResult result = SequentialPipeline().run(options);

    CHECK(result.metrics.recall >= 0.99);
    CHECK(result.metrics.precision >= 0.9);
}

TEST_CASE(endToEndReportsCountsTimingsAndAlerts) {
    const RunResult result = runTrainingFromInput(datasetFile("DoS_dataset.csv"), 60.0);

    CHECK(result.frameCount == kDosFrames);
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
    // A slice of the real file with two broken lines added at the end.
    const auto input = firstLinesCopy(datasetFile("DoS_dataset.csv"), 100000, "e2e_malformed.csv");
    {
        std::ofstream append(input, std::ios::app | std::ios::binary);
        append << "this is not a CAN message\n1478198999.0,0316,8,01\n";
    }
    const RunResult result = runTrainingFromInput(input, 5.0);
    std::filesystem::remove(input);

    CHECK(result.frameCount == 100000);
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
