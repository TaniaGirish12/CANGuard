#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace canguard {

struct StageTiming {
    std::string name;
    double milliseconds = 0.0;
};

// Timing of one stage over one or more repeated runs.
struct StageSummary {
    std::string name;
    double meanMilliseconds = 0.0;
    double stdMilliseconds = 0.0;
    double sharePercent = 0.0;  // of the sum of all stage means
};

// Wall-clock time per named stage. Stages keep the order in which they first stopped,
// and timing the same stage again adds to it. Not thread-safe.
class StageTimer {
public:
    void start(const std::string& stage);

    // Throws std::logic_error if the stage was not started.
    void stop(const std::string& stage);

    template <typename Work>
    void measure(const std::string& stage, Work&& work) {
        start(stage);
        work();
        stop(stage);
    }

    const std::vector<StageTiming>& timings() const { return timings_; }

    // Mean and standard deviation of every stage over repeated runs, in order of first appearance.
    static std::vector<StageSummary> summarize(const std::vector<std::vector<StageTiming>>& runs);

private:
    using Clock = std::chrono::steady_clock;

    std::unordered_map<std::string, Clock::time_point> running_;
    std::vector<StageTiming> timings_;
};

}  // namespace canguard
