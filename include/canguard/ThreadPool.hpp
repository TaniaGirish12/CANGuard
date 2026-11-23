#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace canguard {

// Phase 2 interface only: declared here, implemented later.
//
// A fixed set of worker threads taking tasks from one queue that a mutex and a condition
// variable guard. Workers sleep while the queue is empty.
class ThreadPool {
public:
    // Starts workerCount workers, or one when workerCount is 0.
    explicit ThreadPool(std::size_t workerCount);

    // Graceful: runs every task already queued, then joins the workers.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Queues a task. The future gives its result, or rethrows the exception it threw.
    // Throws std::runtime_error once shutdown() has been called.
    template <typename Task>
    auto submit(Task&& task) -> std::future<std::invoke_result_t<std::decay_t<Task>>>;

    // Stops accepting tasks, finishes the queued ones and joins the workers. Safe to call twice.
    void shutdown();

    std::size_t workerCount() const;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable taskAvailable_;
    bool stopping_ = false;
};

}  // namespace canguard
