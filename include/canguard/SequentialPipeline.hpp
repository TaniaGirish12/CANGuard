#pragma once

#include "canguard/IPipeline.hpp"

namespace canguard {

// Reads, trains, detects and evaluates one stage after another on the calling thread.
class SequentialPipeline final : public IPipeline {
public:
    RunResult run(const Options& options) override;
};

}  // namespace canguard
