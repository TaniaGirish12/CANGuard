#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "canguard/Alert.hpp"
#include "canguard/Evaluator.hpp"
#include "canguard/Options.hpp"
#include "canguard/StageTimer.hpp"

namespace canguard {

struct RunResult {
    Metrics metrics;
    std::vector<StageTiming> timings;
    std::vector<Alert> alerts;             // every message classified as an attack, in input order
    std::vector<std::string> ruleNames;    // ruleNames[i] names bit i of Alert::ruleMask
    std::size_t frameCount = 0;            // parsed messages in the input file
    std::size_t skippedLines = 0;          // malformed lines in the input file
    std::size_t trainingSkippedLines = 0;  // malformed lines in the --train file
};

class IPipeline {
public:
    virtual ~IPipeline() = default;

    // Throws std::runtime_error when a file cannot be read or there is nothing to train on.
    virtual RunResult run(const Options& options) = 0;
};

}  // namespace canguard
