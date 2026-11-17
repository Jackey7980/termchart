// test_thread_post.cpp —— 后台线程投递 + 唤醒的端到端集成测试
//
// 并发模型：后台线程只经 PostQueue/Mailbox 投递数据，notify 唤醒主线程，
// 主线程 take/drain 后消费。这里验证完整闭环：数据保序、无竞态、唤醒不丢。
#include <doctest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "termchart/app/post.hpp"
#include "termchart/term/waker.hpp"

namespace {

using namespace termchart;

}  // namespace

TEST_CASE("integration/thread_post: 生产线程投递 + waker 唤醒 + 主线程 take 消费") {
    app::PostQueue queue;
    term::Waker waker;
    REQUIRE(waker.valid());

    constexpr int kCount = 32;
    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            queue.post([i] { static_cast<void>(i); });
        }
        waker.notify();
    });

    // 主线程等待唤醒（生产线程已 notify）。
    const bool woke = waker.wait(2000);
    producer.join();
    REQUIRE(woke);
    waker.drain();

    CHECK(queue.pending() == kCount);
    const std::vector<app::PostQueue::Task> tasks = queue.take();
    REQUIRE(tasks.size() == kCount);
    CHECK(queue.pending() == 0);
    for (auto& task : tasks) {
        task();
    }
}

TEST_CASE("integration/thread_post: 多生产者投递计数精确，无丢失/重复") {
    app::PostQueue queue;
    std::atomic<int> executed{0};

    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;
    std::vector<std::thread> producers;
    producers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        producers.emplace_back([&] {
            for (int i = 0; i < kPerThread; ++i) {
                queue.post([&executed] { executed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    for (auto& thread : producers) {
        thread.join();
    }

    CHECK(queue.pending() == kThreads * kPerThread);
    CHECK(queue.drain() == static_cast<std::size_t>(kThreads * kPerThread));
    CHECK(executed.load() == kThreads * kPerThread);
    CHECK(queue.pending() == 0);
}

TEST_CASE("integration/thread_post: Mailbox 跨线程最新值语义") {
    app::Mailbox<int> mailbox;

    std::thread producer([&] {
        for (int i = 0; i < 50; ++i) {
            mailbox.post(i);
        }
    });
    producer.join();

    int value = -1;
    REQUIRE(mailbox.consume(value));
    CHECK(value == 49);  // 覆盖语义：只留最后写入
    CHECK(mailbox.empty());
}
