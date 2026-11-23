#pragma once

#include "canguard/IPipeline.hpp"

namespace canguard {

// Phase 2 interface only: declared here, implemented later. Mode name: timewin.
//
// Splits the input into time chunks that are parsed and processed in parallel. The detection
// state at a chunk border depends on the previous chunk, which is handled in one of two ways:
// each chunk first replays a short warm-up overlap taken from the end of the previous chunk, or
// chunks hand their final per-ID state to the next one in order, with a condition variable
// waking the next chunk when its state is ready.
class TimeWindowPipeline final : public IPipeline {
public:
    RunResult run(const Options& options) override;
};

}  // namespace canguard
