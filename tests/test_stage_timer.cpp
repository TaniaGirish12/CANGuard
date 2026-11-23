#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

#include "TestRegistry.hpp"
#include "canguard/StageTimer.hpp"

using canguard::StageSummary;
using canguard::StageTimer;
using canguard::StageTiming;

namespace {

bool nearlyEqual(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

void sleepMilliseconds(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

}  // namespace

TEST_CASE(stageTimerMeasuresElapsedWallClockTime) {
    StageTimer timer;
    timer.start("sleep");
    sleepMilliseconds(20);
    timer.stop("sleep");

    CHECK(timer.timings().size() == 1);
    if (timer.timings().size() != 1) return;
    CHECK(timer.timings()[0].name == "sleep");
    CHECK(timer.timings()[0].milliseconds >= 19.0);
    CHECK(timer.timings()[0].milliseconds < 2000.0);
}

TEST_CASE(stageTimerKeepsStagesInOrderAndAccumulatesRepeats) {
    StageTimer timer;
    timer.measure("first", [] { sleepMilliseconds(5); });
    timer.measure("second", [] { sleepMilliseconds(5); });
    timer.measure("first", [] { sleepMilliseconds(15); });

    CHECK(timer.timings().size() == 2);
    if (timer.timings().size() != 2) return;
    CHECK(timer.timings()[0].name == "first");
    CHECK(timer.timings()[1].name == "second");
    CHECK(timer.timings()[0].milliseconds >= 19.0);
}

TEST_CASE(stageTimerRejectsStoppingAStageThatNeverStarted) {
    StageTimer timer;
    bool threw = false;
    try {
        timer.stop("missing");
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE(stageTimerSummarizesMeanStdAndShareOverRuns) {
    const std::vector<std::vector<StageTiming>> runs = {
        {{"read", 10.0}, {"detect", 30.0}},
        {{"read", 20.0}, {"detect", 30.0}},
    };
    const std::vector<StageSummary> summary = StageTimer::summarize(runs);

    CHECK(summary.size() == 2);
    if (summary.size() != 2) return;
    CHECK(summary[0].name == "read");
    CHECK(nearlyEqual(summary[0].meanMilliseconds, 15.0));
    CHECK(nearlyEqual(summary[0].stdMilliseconds, 5.0));
    CHECK(nearlyEqual(summary[0].sharePercent, 100.0 * 15.0 / 45.0));
    CHECK(summary[1].name == "detect");
    CHECK(nearlyEqual(summary[1].meanMilliseconds, 30.0));
    CHECK(nearlyEqual(summary[1].stdMilliseconds, 0.0));
    CHECK(nearlyEqual(summary[1].sharePercent, 100.0 * 30.0 / 45.0));
}

TEST_CASE(stageTimerSummaryOfASingleRunHasZeroStd) {
    const std::vector<StageSummary> summary = StageTimer::summarize({{{"only", 12.0}}});
    CHECK(summary.size() == 1);
    if (summary.size() != 1) return;
    CHECK(nearlyEqual(summary[0].stdMilliseconds, 0.0));
    CHECK(nearlyEqual(summary[0].sharePercent, 100.0));
}

TEST_CASE(stageTimerSummaryHandlesNoRunsAndZeroTotals) {
    CHECK(StageTimer::summarize({}).empty());

    const std::vector<StageSummary> summary = StageTimer::summarize({{{"instant", 0.0}}});
    CHECK(summary.size() == 1);
    if (summary.size() == 1) CHECK(summary[0].sharePercent == 0.0);
}
