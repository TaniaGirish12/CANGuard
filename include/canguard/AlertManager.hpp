#pragma once

#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "canguard/Alert.hpp"

namespace canguard {

// Phase 2 interface only: declared here, implemented later.
//
// Puts the alerts of independently processed chunks back into input order. A consumer thread
// waits on a condition variable until the next chunk in sequence has arrived, then appends its
// alerts, so workers may finish their chunks in any order.
class AlertManager {
public:
    // Starts the consumer thread.
    AlertManager();

    // Stops the consumer thread if finish() was not called.
    ~AlertManager();

    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;

    // Hands over the alerts of one chunk. Chunks are numbered from 0 and may arrive in any order.
    void submit(std::size_t chunkSequenceNumber, std::vector<Alert> alerts);

    // Waits until chunks 0 .. chunkCount - 1 have all been submitted and ordered, stops the
    // consumer thread and returns every alert in input order.
    std::vector<Alert> finish(std::size_t chunkCount);

private:
    std::mutex mutex_;
    std::condition_variable chunkArrived_;
    std::map<std::size_t, std::vector<Alert>> pendingChunks_;
    std::vector<Alert> orderedAlerts_;
    std::size_t nextChunk_ = 0;
    std::thread consumer_;
};

}  // namespace canguard
