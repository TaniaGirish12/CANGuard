#include <string>
#include <vector>

#include "TestRegistry.hpp"
#include "canguard/ConsoleReport.hpp"

using canguard::ConsoleReport;
using canguard::Options;
using canguard::RuleStats;
using canguard::RunResult;
using canguard::StageSummary;

namespace {

RuleStats makeRule(const std::string& name, std::size_t truePositives, std::size_t falsePositives) {
    RuleStats rule;
    rule.name = name;
    rule.truePositives = truePositives;
    rule.falsePositives = falsePositives;
    return rule;
}

StageSummary makeStage(const std::string& name, double mean, double std, double share) {
    StageSummary stage;
    stage.name = name;
    stage.meanMilliseconds = mean;
    stage.stdMilliseconds = std;
    stage.sharePercent = share;
    return stage;
}

RunResult makeResult() {
    RunResult result;
    result.frameCount = 4621702;
    result.metrics.precision = 0.8;
    result.metrics.recall = 1.0;
    result.metrics.f1 = 2.0 * 0.8 / 1.8;
    result.metrics.falsePositiveRate = 0.25;
    // Same order as the detector: UnknownId, Timing, Rate, Payload.
    result.metrics.rules = {makeRule("UnknownId", 10, 0), makeRule("Timing", 5, 2), makeRule("Rate", 0, 0),
                            makeRule("Payload", 3, 4)};
    return result;
}

const std::vector<StageSummary> kTimings = {
    makeStage("read+parse", 400.0, 2.0, 80.0),
    makeStage("train", 5.0, 0.0, 1.0),
    makeStage("detect", 75.0, 1.0, 15.0),
    makeStage("evaluate", 20.0, 0.5, 4.0),
};

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE(consoleReportMatchesTheSpecifiedLayout) {
    Options options;
    options.inputPath = "data/raw/RPM_dataset.csv";

    CHECK(ConsoleReport::format(options, makeResult(), kTimings) ==
          "CANGuard | mode=sequential | input=RPM_dataset.csv | frames=4621702 | skipped=0\n"
          "Stage          time (ms)    share\n"
          "read+parse         400.0    80.0%\n"
          "train                5.0     1.0%\n"
          "detect              75.0    15.0%\n"
          "evaluate            20.0     4.0%\n"
          "Precision 0.800  Recall 1.000  F1 0.889  FPR 0.250\n"
          "Rules fired: Timing 7  Rate 0  Payload 7  UnknownId 10\n");
}

TEST_CASE(consoleReportMentionsSkippedLinesOfTheTrainingFile) {
    Options options;
    options.inputPath = "RPM_dataset.csv";
    RunResult result = makeResult();
    result.skippedLines = 3;
    result.trainingSkippedLines = 1;

    const std::string report = ConsoleReport::format(options, result, kTimings);
    CHECK(contains(report, "| skipped=3 (+1 in training file)\n"));
}

TEST_CASE(consoleReportAddsStdColumnWhenRepeating) {
    Options options;
    options.inputPath = "RPM_dataset.csv";
    options.repeat = 3;

    const std::string report = ConsoleReport::format(options, makeResult(), kTimings);
    CHECK(contains(report, "Stage          time (ms)   std (ms)    share\n"));
    CHECK(contains(report, "read+parse         400.0        2.0    80.0%\n"));
}
