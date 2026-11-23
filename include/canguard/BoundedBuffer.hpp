#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <utility>

namespace canguard {

// Fixed-capacity FIFO queue for handing items between threads.
//
// The semaphores count free and filled slots so producers and consumers block without spinning;
// the mutex only protects the deque and the closed flag. close() lets every blocked thread
// finish: a thread that wakes because of the close passes the wake-up on to the next one.
template <typename T>
class BoundedBuffer {
public:
    // Throws std::invalid_argument for a capacity of zero.
    explicit BoundedBuffer(std::size_t capacity)
        : emptySlots_(static_cast<std::ptrdiff_t>(checkedCapacity(capacity))), filledSlots_(0) {}

    BoundedBuffer(const BoundedBuffer&) = delete;
    BoundedBuffer& operator=(const BoundedBuffer&) = delete;

    // Blocks while the buffer is full. Returns false, and drops the item, if the buffer is closed.
    bool push(T item) {
        emptySlots_.acquire();

        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            accepted = !closed_;
            if (accepted) items_.push_back(std::move(item));
        }

        if (accepted) {
            filledSlots_.release();
        } else {
            emptySlots_.release();
        }
        return accepted;
    }

    // Blocks while the buffer is empty. Returns std::nullopt once it is closed and drained.
    std::optional<T> pop() {
        filledSlots_.acquire();

        std::optional<T> item;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!items_.empty()) {
                item = std::move(items_.front());
                items_.pop_front();
            }
        }

        if (item) {
            emptySlots_.release();
        } else {
            filledSlots_.release();
        }
        return item;
    }

    // Wakes every blocked thread. Items already queued can still be popped; new pushes are refused.
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) return;
            closed_ = true;
        }
        emptySlots_.release();
        filledSlots_.release();
    }

private:
    static std::size_t checkedCapacity(std::size_t capacity) {
        if (capacity == 0) throw std::invalid_argument("BoundedBuffer capacity must be at least 1");
        return capacity;
    }

    std::counting_semaphore<> emptySlots_;
    std::counting_semaphore<> filledSlots_;
    std::mutex mutex_;
    std::deque<T> items_;
    bool closed_ = false;
};

}  // namespace canguard
