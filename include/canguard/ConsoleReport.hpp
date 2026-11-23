#pragma once

#include <string>
#include <vector>

#include "canguard/IPipeline.hpp"
#include "canguard/Options.hpp"
#include "canguard/StageTimer.hpp"

namespace canguard {

// Builds the table printed at the end of a run. With --repeat above 1 the timing table also shows
// the standard deviation.
class ConsoleReport {
public:
    static std::string format(const Options& options, const RunResult& result,
                              const std::vector<StageSummary>& timings);
};

}  // namespace canguard
