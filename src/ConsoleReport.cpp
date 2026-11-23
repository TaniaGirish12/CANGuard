#include "canguard/ConsoleReport.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace canguard {

namespace {

constexpr int kStageWidth = 12;
constexpr int kTimeWidth = 12;
constexpr int kStdWidth = 11;
constexpr int kShareWidth = 8;

// Order used in the console output; it differs from the order the detector runs the rules in.
const char* const kRuleDisplayOrder[] = {"Timing", "Rate", "Payload", "UnknownId"};

std::size_t firedCount(const Metrics& metrics, const std::string& ruleName) {
    for (const RuleStats& rule : metrics.rules) {
        if (rule.name == ruleName) return rule.fired();
    }
    return 0;
}

}  // namespace

std::string ConsoleReport::format(const Options& options, const RunResult& result,
                                  const std::vector<StageSummary>& timings) {
    const bool showStd = options.repeat > 1;
    std::ostringstream out;

    out << "CANGuard | mode=" << options.mode << " | input=" << std::filesystem::path(options.inputPath).filename().string()
        << " | frames=" << result.frameCount << " | skipped=" << result.skippedLines;
    if (result.trainingSkippedLines > 0) out << " (+" << result.trainingSkippedLines << " in training file)";
    out << '\n';

    out << std::left << std::setw(kStageWidth) << "Stage" << std::right << std::setw(kTimeWidth) << "time (ms)";
    if (showStd) out << std::setw(kStdWidth) << "std (ms)";
    out << std::setw(kShareWidth + 1) << "share" << '\n';

    out << std::fixed << std::setprecision(1);
    for (const StageSummary& stage : timings) {
        out << std::left << std::setw(kStageWidth) << stage.name << std::right << std::setw(kTimeWidth)
            << stage.meanMilliseconds;
        if (showStd) out << std::setw(kStdWidth) << stage.stdMilliseconds;
        out << std::setw(kShareWidth) << stage.sharePercent << "%\n";
    }

    const Metrics& metrics = result.metrics;
    out << std::setprecision(3) << "Precision " << metrics.precision << "  Recall " << metrics.recall << "  F1 "
        << metrics.f1 << "  FPR " << metrics.falsePositiveRate << '\n';

    out << "Rules fired:";
    bool firstRule = true;
    for (const char* ruleName : kRuleDisplayOrder) {
        out << (firstRule ? " " : "  ") << ruleName << ' ' << firedCount(metrics, ruleName);
        firstRule = false;
    }
    out << '\n';
    return out.str();
}

}  // namespace canguard
