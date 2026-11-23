#pragma once

#include <cstddef>
#include <cstdint>

#include "canguard/IPipeline.hpp"

namespace canguard {

// Phase 2 interface only: declared here, implemented later. Mode name: idpart.
//
// One dispatcher thread reads and parses the input, then sends every frame to the worker that
// owns its CAN ID, chosen as hash(id) mod N, through that worker's own BoundedBuffer. Each worker
// holds a Detector for its IDs only. Because all mutable detection state is per ID, the workers
// need no locking between them and every ID is still processed in timestamp order.
class IdPartitionedPipeline final : public IPipeline {
public:
    RunResult run(const Options& options) override;

    // The worker, from 0 to workerCount - 1, that owns this CAN ID.
    static std::size_t workerIndexFor(std::uint32_t id, std::size_t workerCount);
};

}  // namespace canguard
