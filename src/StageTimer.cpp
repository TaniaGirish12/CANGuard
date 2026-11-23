#include "canguard/StageTimer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace canguard {

void StageTimer::start(const std::string& stage) {
    running_[stage] = Clock::now();
}

void StageTimer::stop(const std::string& stage) {
    const auto now = Clock::now();
    const auto started = running_.find(stage);
    if (started == running_.end()) throw std::logic_error("stage was not started: " + stage);

    const double milliseconds = std::chrono::duration<double, std::milli>(now - started->second).count();
    running_.erase(started);

    const auto known = std::find_if(timings_.begin(), timings_.end(),
                                    [&](const StageTiming& timing) { return timing.name == stage; });
    if (known == timings_.end()) {
        timings_.push_back({stage, milliseconds});
    } else {
        known->milliseconds += milliseconds;
    }
}

std::vector<StageSummary> StageTimer::summarize(const std::vector<std::vector<StageTiming>>& runs) {
    std::vector<std::string> stageOrder;
    std::unordered_map<std::string, std::vector<double>> samples;
    for (const auto& run : runs) {
        for (const StageTiming& timing : run) {
            if (samples.find(timing.name) == samples.end()) stageOrder.push_back(timing.name);
            samples[timing.name].push_back(timing.milliseconds);
        }
    }

    std::vector<StageSummary> summaries;
    for (const std::string& stage : stageOrder) {
        const std::vector<double>& values = samples[stage];
        const auto count = static_cast<double>(values.size());
        const double mean = std::accumulate(values.begin(), values.end(), 0.0) / count;
        double squaredDeviations = 0.0;
        for (const double value : values) squaredDeviations += (value - mean) * (value - mean);

        StageSummary summary;
        summary.name = stage;
        summary.meanMilliseconds = mean;
        summary.stdMilliseconds = std::sqrt(squaredDeviations / count);
        summaries.push_back(summary);
    }

    double totalMean = 0.0;
    for (const StageSummary& summary : summaries) totalMean += summary.meanMilliseconds;
    if (totalMean > 0.0) {
        for (StageSummary& summary : summaries) summary.sharePercent = 100.0 * summary.meanMilliseconds / totalMean;
    }
    return summaries;
}

}  // namespace canguard
