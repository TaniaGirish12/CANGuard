#include "canguard/ResultWriter.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <utility>

namespace canguard {

namespace {

std::ofstream openForWriting(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output file: " + path.string());
    return out;
}

void checkWritten(const std::ofstream& out, const std::filesystem::path& path) {
    if (!out) throw std::runtime_error("failed while writing: " + path.string());
}

}  // namespace

ResultWriter::ResultWriter(std::filesystem::path outputDirectory, std::vector<std::string> ruleNames)
    : outputDirectory_(std::move(outputDirectory)), ruleNames_(std::move(ruleNames)) {
    std::filesystem::create_directories(outputDirectory_);
}

void ResultWriter::writeMetrics(const Metrics& metrics) const {
    const auto path = outputDirectory_ / "metrics.csv";
    std::ofstream out = openForWriting(path);

    out << "metric,value\n"
        << "true_positives," << metrics.truePositives << '\n'
        << "false_positives," << metrics.falsePositives << '\n'
        << "true_negatives," << metrics.trueNegatives << '\n'
        << "false_negatives," << metrics.falseNegatives << '\n'
        << std::fixed << std::setprecision(6)
        << "precision," << metrics.precision << '\n'
        << "recall," << metrics.recall << '\n'
        << "f1," << metrics.f1 << '\n'
        << "false_positive_rate," << metrics.falsePositiveRate << '\n';
    for (const RuleStats& rule : metrics.rules) {
        out << "rule_" << rule.name << "_fired," << rule.fired() << '\n'
            << "rule_" << rule.name << "_true_positives," << rule.truePositives << '\n'
            << "rule_" << rule.name << "_false_positives," << rule.falsePositives << '\n';
    }
    checkWritten(out, path);
}

void ResultWriter::writeTimings(const std::vector<StageSummary>& timings) const {
    const auto path = outputDirectory_ / "timings.csv";
    std::ofstream out = openForWriting(path);

    out << "stage,mean_ms,std_ms,share_percent\n" << std::fixed << std::setprecision(3);
    for (const StageSummary& stage : timings) {
        out << stage.name << ',' << stage.meanMilliseconds << ',' << stage.stdMilliseconds << ','
            << stage.sharePercent << '\n';
    }
    checkWritten(out, path);
}

void ResultWriter::writeAlerts(const std::vector<Alert>& alerts) const {
    const auto path = outputDirectory_ / "alerts.csv";
    std::ofstream out = openForWriting(path);

    out << "timestamp,id,rules_fired,true_label\n";
    std::string line;
    char prefix[48];
    for (const Alert& alert : alerts) {
        const int prefixLength = std::snprintf(prefix, sizeof prefix, "%.6f,%04x,", alert.timestamp,
                                               static_cast<unsigned>(alert.id));
        line.assign(prefix, static_cast<std::size_t>(prefixLength));

        bool firstRule = true;
        for (std::size_t i = 0; i < ruleNames_.size(); ++i) {
            if (((alert.ruleMask >> i) & 1U) == 0) continue;
            if (!firstRule) line += '|';
            line += ruleNames_[i];
            firstRule = false;
        }
        line += alert.isAttack ? ",T\n" : ",R\n";
        out << line;
    }
    checkWritten(out, path);
}

}  // namespace canguard
