#pragma once

#include "canguard/IPipeline.hpp"

namespace canguard {

// Phase 2 interface only: declared here, implemented later. Mode name: staged.
//
// A pipeline of threads connected by BoundedBuffers: a reader hands raw chunks to parser
// workers, a reorder step restores chunk order using each chunk's sequence number, a detector
// thread processes the frames in order, and an AlertManager collects the alerts. Bounded buffers
// keep a fast stage from running far ahead of a slow one.
class StagedPipeline final : public IPipeline {
public:
    RunResult run(const Options& options) override;
};

}  // namespace canguard
