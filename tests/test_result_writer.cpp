#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "TestRegistry.hpp"
#include "TempPath.hpp"
#include "canguard/ResultWriter.hpp"

using canguard::Alert;
using canguard::Metrics;
using canguard::ResultWriter;
using canguard::RuleStats;
using canguard::StageSummary;

namespace {

const std::vector<std::string> kRuleNames = {"UnknownId", "Timing", "Rate", "Payload"};

std::filesystem::path freshOutputDirectory(const std::string& name) {
    const auto directory = canguard::testing::uniqueTempPath("results_" + name);
    std::filesystem::remove_all(directory);
    return directory;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace

TEST_CASE(resultWriterWritesMetricsCsv) {
    const auto directory = freshOutputDirectory("metrics");
    Metrics metrics;
    metrics.truePositives = 4;
    metrics.falsePositives = 1;
    metrics.trueNegatives = 5;
    metrics.falseNegatives = 1;
    metrics.precision = 0.8;
    metrics.recall = 0.8;
    metrics.f1 = 0.8;
    metrics.falsePositiveRate = 1.0 / 6.0;
    RuleStats timing;
    timing.name = "Timing";
    timing.truePositives = 3;
    timing.falsePositives = 1;
    metrics.rules = {timing};

    ResultWriter(directory, kRuleNames).writeMetrics(metrics);

    CHECK(readFile(directory / "metrics.csv") ==
          "metric,value\n"
          "true_positives,4\n"
          "false_positives,1\n"
          "true_negatives,5\n"
          "false_negatives,1\n"
          "precision,0.800000\n"
          "recall,0.800000\n"
          "f1,0.800000\n"
          "false_positive_rate,0.166667\n"
          "rule_Timing_fired,4\n"
          "rule_Timing_true_positives,3\n"
          "rule_Timing_false_positives,1\n");
    std::filesystem::remove_all(directory);
}

TEST_CASE(resultWriterWritesTimingsCsv) {
    const auto directory = freshOutputDirectory("timings");
    StageSummary readParse;
    readParse.name = "read+parse";
    readParse.meanMilliseconds = 12.5;
    readParse.stdMilliseconds = 0.5;
    readParse.sharePercent = 25.0;
    StageSummary detect;
    detect.name = "detect";
    detect.meanMilliseconds = 37.5;
    detect.stdMilliseconds = 1.0;
    detect.sharePercent = 75.0;

    ResultWriter(directory, kRuleNames).writeTimings({readParse, detect});

    CHECK(readFile(directory / "timings.csv") ==
          "stage,mean_ms,std_ms,share_percent\n"
          "read+parse,12.500,0.500,25.000\n"
          "detect,37.500,1.000,75.000\n");
    std::filesystem::remove_all(directory);
}

TEST_CASE(resultWriterWritesAlertsCsvWithRuleNamesAndTrueLabel) {
    const auto directory = freshOutputDirectory("alerts");
    Alert caught;
    caught.timestamp = 1478198376.389427;
    caught.id = 0x316;
    caught.ruleMask = (1U << 1) | (1U << 3);
    caught.isAttack = true;
    Alert falseAlarm;
    falseAlarm.timestamp = 1478198377.5;
    falseAlarm.id = 0x0;
    falseAlarm.ruleMask = 1U << 0;
    falseAlarm.isAttack = false;

    ResultWriter(directory, kRuleNames).writeAlerts({caught, falseAlarm});

    CHECK(readFile(directory / "alerts.csv") ==
          "timestamp,id,rules_fired,true_label\n"
          "1478198376.389427,0316,Timing|Payload,T\n"
          "1478198377.500000,0000,UnknownId,R\n");
    std::filesystem::remove_all(directory);
}

TEST_CASE(resultWriterWritesHeaderOnlyWhenThereAreNoAlerts) {
    const auto directory = freshOutputDirectory("no_alerts");
    ResultWriter(directory, kRuleNames).writeAlerts({});
    CHECK(readFile(directory / "alerts.csv") == "timestamp,id,rules_fired,true_label\n");
    std::filesystem::remove_all(directory);
}

TEST_CASE(resultWriterCreatesMissingOutputDirectories) {
    const auto directory = freshOutputDirectory("nested") / "deeper" / "still";
    ResultWriter(directory, kRuleNames).writeTimings({});
    CHECK(std::filesystem::exists(directory / "timings.csv"));
    std::filesystem::remove_all(freshOutputDirectory("nested"));
}

TEST_CASE(resultWriterThrowsWhenTheOutputFileCannotBeOpened) {
    const auto directory = freshOutputDirectory("blocked");
    const ResultWriter writer(directory, kRuleNames);
    std::filesystem::create_directory(directory / "metrics.csv");  // a folder where the file should go

    bool threw = false;
    try {
        writer.writeMetrics(Metrics{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
    std::filesystem::remove_all(directory);
}
