#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "TestRegistry.hpp"
#include "canguard/BoundedBuffer.hpp"

using canguard::BoundedBuffer;

namespace {

// Long enough for a thread that is not blocked to finish; blocked threads must still be waiting.
void letOtherThreadsRun() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}  // namespace

TEST_CASE(boundedBufferKeepsFifoOrder) {
    BoundedBuffer<int> buffer(4);
    for (int i = 1; i <= 4; ++i) CHECK(buffer.push(i));
    for (int i = 1; i <= 4; ++i) CHECK(buffer.pop() == i);
}

TEST_CASE(boundedBufferRejectsZeroCapacity) {
    bool threw = false;
    try {
        BoundedBuffer<int> buffer(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE(boundedBufferPushBlocksWhenFullUntilAnItemIsPopped) {
    BoundedBuffer<int> buffer(2);
    buffer.push(1);
    buffer.push(2);

    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        buffer.push(3);
        pushed = true;
    });

    letOtherThreadsRun();
    CHECK(!pushed);

    CHECK(buffer.pop() == 1);
    producer.join();
    CHECK(pushed);
    CHECK(buffer.pop() == 2);
    CHECK(buffer.pop() == 3);
}

TEST_CASE(boundedBufferPopBlocksWhenEmptyUntilAnItemArrives) {
    BoundedBuffer<int> buffer(2);

    std::atomic<bool> popped{false};
    std::optional<int> received;
    std::thread consumer([&] {
        received = buffer.pop();
        popped = true;
    });

    letOtherThreadsRun();
    CHECK(!popped);

    buffer.push(42);
    consumer.join();
    CHECK(popped);
    CHECK(received == 42);
}

TEST_CASE(boundedBufferFourProducersAndFourConsumersTransfer100000ItemsExactlyOnce) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kItemsPerProducer = 25000;
    constexpr int kTotalItems = kProducers * kItemsPerProducer;

    BoundedBuffer<int> buffer(16);
    std::vector<std::vector<int>> received(kConsumers);

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&, c] {
            while (const std::optional<int> item = buffer.pop()) received[c].push_back(*item);
        });
    }

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kItemsPerProducer; ++i) buffer.push(p * kItemsPerProducer + i);
        });
    }

    for (std::thread& producer : producers) producer.join();
    buffer.close();
    for (std::thread& consumer : consumers) consumer.join();

    std::vector<int> timesSeen(kTotalItems, 0);
    std::size_t totalReceived = 0;
    for (const std::vector<int>& items : received) {
        totalReceived += items.size();
        for (const int item : items) ++timesSeen[static_cast<std::size_t>(item)];
    }
    CHECK(totalReceived == static_cast<std::size_t>(kTotalItems));
    CHECK(std::all_of(timesSeen.begin(), timesSeen.end(), [](int count) { return count == 1; }));
}

TEST_CASE(boundedBufferCloseWakesAllBlockedConsumers) {
    BoundedBuffer<int> buffer(2);
    std::atomic<int> returnedNothing{0};

    std::vector<std::thread> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back([&] {
            if (!buffer.pop().has_value()) ++returnedNothing;
        });
    }

    letOtherThreadsRun();
    CHECK(returnedNothing == 0);

    buffer.close();
    for (std::thread& consumer : consumers) consumer.join();
    CHECK(returnedNothing == 4);
}

TEST_CASE(boundedBufferCloseWakesAllBlockedProducers) {
    BoundedBuffer<int> buffer(1);
    buffer.push(1);
    std::atomic<int> refused{0};

    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&] {
            if (!buffer.push(2)) ++refused;
        });
    }

    letOtherThreadsRun();
    CHECK(refused == 0);

    buffer.close();
    for (std::thread& producer : producers) producer.join();
    CHECK(refused == 4);
}

TEST_CASE(boundedBufferPopAfterCloseDrainsQueuedItemsThenReturnsNothing) {
    BoundedBuffer<int> buffer(4);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.close();

    CHECK(!buffer.push(4));
    CHECK(buffer.pop() == 1);
    CHECK(buffer.pop() == 2);
    CHECK(buffer.pop() == 3);
    CHECK(!buffer.pop().has_value());
    CHECK(!buffer.pop().has_value());
}

TEST_CASE(boundedBufferCloseCanBeCalledTwice) {
    BoundedBuffer<int> buffer(1);
    buffer.close();
    buffer.close();
    CHECK(!buffer.pop().has_value());
    CHECK(!buffer.push(1));
}

TEST_CASE(boundedBufferHoldsMoveOnlyItems) {
    BoundedBuffer<std::unique_ptr<int>> buffer(2);
    buffer.push(std::make_unique<int>(7));
    const auto item = buffer.pop();
    CHECK(item.has_value() && **item == 7);
}
