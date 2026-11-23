#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "canguard/Alert.hpp"
#include "canguard/Evaluator.hpp"
#include "canguard/StageTimer.hpp"

namespace canguard {

// Writes the result files into one output folder, which is created if it does not exist.
// Every method throws std::runtime_error when a file cannot be written.
class ResultWriter {
public:
    // ruleNames[i] names the rule behind bit i of Alert::ruleMask.
    ResultWriter(std::filesystem::path outputDirectory, std::vector<std::string> ruleNames);

    // metrics.csv: "metric,value" rows with the confusion matrix, the rates and per-rule counts.
    void writeMetrics(const Metrics& metrics) const;

    // timings.csv: stage,mean_ms,std_ms,share_percent
    void writeTimings(const std::vector<StageSummary>& timings) const;

    // alerts.csv: timestamp,id,rules_fired,true_label (rules joined by '|', label T or R as in the dataset)
    void writeAlerts(const std::vector<Alert>& alerts) const;

private:
    std::filesystem::path outputDirectory_;
    std::vector<std::string> ruleNames_;
};

}  // namespace canguard
