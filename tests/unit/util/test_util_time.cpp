// test_util_time.cpp —— 计时、帧计时、超时、调度器、可取消任务
//
// 计时相关的用例一律使用**注入的时间点**（tick_at / restart_at / tick(stamp)），
// 不依赖真实时间流逝，因此在任何机器上都稳定，也不会因为 CI 慢而抖动。
#include <doctest.h>

#include <chrono>
#include <cstddef>
#include <vector>

#include "termchart/util/scheduler.hpp"
#include "termchart/util/task.hpp"
#include "termchart/util/timer.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

TimePoint at_ms(long long ms) {
    return TimePoint{} + std::chrono::milliseconds(ms);
}

Duration ms(long long value) {
    return std::chrono::milliseconds(value);
}

}  // namespace

TEST_CASE("util/timer: 单调起点稳定，秒表非负") {
    CHECK(monotonic_start() == monotonic_start());  // function-local static
    const Stopwatch watch;
    CHECK(watch.elapsed_seconds() >= 0.0);
    CHECK(watch.elapsed_ms() >= 0.0);
}

TEST_CASE("util/timer: 帧计时首帧为 0，随后给出间隔与 FPS") {
    FrameTimer timer(0.0);  // 不平滑，便于断言精确值
    CHECK(timer.tick_at(at_ms(0)) == doctest::Approx(0.0));
    CHECK(timer.frame_count() == 1);
    CHECK(timer.fps() == doctest::Approx(0.0));  // delta 还是 0，不返回无穷

    CHECK(timer.tick_at(at_ms(16)) == doctest::Approx(0.016));
    CHECK(timer.delta_seconds() == doctest::Approx(0.016));
    CHECK(timer.fps() == doctest::Approx(62.5));
    CHECK(timer.frame_count() == 2);

    // 时间回退（换了时间源）夹到 0，FPS 不会变成负数
    CHECK(timer.tick_at(at_ms(10)) == doctest::Approx(0.0));
    CHECK(timer.delta_seconds() == doctest::Approx(0.0));
    CHECK(timer.fps() == doctest::Approx(0.0));

    timer.reset();
    CHECK(timer.frame_count() == 0);
    CHECK(timer.tick_at(at_ms(100)) == doctest::Approx(0.0));  // 重置后又是首帧
}

TEST_CASE("util/timer: 帧计时平滑按系数混合") {
    FrameTimer timer(0.5);
    (void)timer.tick_at(at_ms(0));            // 首帧
    CHECK(timer.tick_at(at_ms(10)) == doctest::Approx(0.010));  // 原始间隔照旧返回
    CHECK(timer.delta_seconds() == doctest::Approx(0.010));     // 首次有值即原始值
    (void)timer.tick_at(at_ms(30));           // 原始 0.020
    // 平滑值 = 0.5*0.010 + 0.5*0.020 = 0.015
    CHECK(timer.delta_seconds() == doctest::Approx(0.015));
}

TEST_CASE("util/timer: 超时判断与剩余时间") {
    Timeout timeout(ms(100));
    timeout.restart_at(at_ms(0));
    CHECK_FALSE(timeout.expired_at(at_ms(99)));
    CHECK(timeout.expired_at(at_ms(100)));   // 到点即超时
    CHECK(timeout.expired_at(at_ms(1000)));
    CHECK(timeout.remaining_seconds(at_ms(40)) == doctest::Approx(0.06));
    CHECK(timeout.remaining_seconds(at_ms(500)) == doctest::Approx(0.0));  // 不返回负数
    CHECK(timeout.limit() == ms(100));
}

TEST_CASE("util/scheduler: 一次性任务到点触发一次并出队") {
    Scheduler scheduler;
    int fired = 0;
    // 注册与 tick 都用注入时间（after_at / tick(stamp)）——两个时间源不能混用。
    const Scheduler::TaskId id = scheduler.after_at(at_ms(0), ms(50), [&fired] { ++fired; });
    CHECK(scheduler.contains(id));
    CHECK(scheduler.pending() == 1);

    CHECK(scheduler.tick(at_ms(49)) == 0);
    CHECK(fired == 0);
    CHECK(scheduler.tick(at_ms(50)) == 1);
    CHECK(fired == 1);
    CHECK_FALSE(scheduler.contains(id));  // 已出队
    CHECK(scheduler.pending() == 0);

    CHECK(scheduler.tick(at_ms(1000)) == 0);  // 不会重复触发
    CHECK(fired == 1);
}

TEST_CASE("util/scheduler: 周期任务按固定间隔重排，不补偿追赶") {
    Scheduler scheduler;
    int fired = 0;
    (void)scheduler.every_at(at_ms(0), ms(100), [&fired] { ++fired; });

    CHECK(scheduler.tick(at_ms(100)) == 1);
    CHECK(scheduler.tick(at_ms(150)) == 0);
    CHECK(scheduler.tick(at_ms(200)) == 1);
    CHECK(fired == 2);

    // 卡顿一秒后只补跑一次（从"当次时刻"重排），不会连续喷发
    CHECK(scheduler.tick(at_ms(1300)) == 1);
    CHECK(fired == 3);
    CHECK(scheduler.tick(at_ms(1301)) == 0);

    const auto due = scheduler.next_due();
    REQUIRE(due.has_value());
    CHECK(*due == at_ms(1400));
}

TEST_CASE("util/scheduler: 取消生效；回调里可安全增删任务") {
    Scheduler scheduler;
    int a = 0;
    int b = 0;
    // second 先声明（它的 id 要被 first 的回调捕获），再赋实际任务 id。
    Scheduler::TaskId second = 0;
    const Scheduler::TaskId first = scheduler.after_at(at_ms(0), ms(10), [&] {
        ++a;
        (void)scheduler.cancel(second);  // 取消同一轮里也到期的另一个任务
    });
    second = scheduler.after_at(at_ms(0), ms(10), [&b] { ++b; });

    CHECK(scheduler.tick(at_ms(10)) == 1);  // 只跑了 first
    CHECK(a == 1);
    CHECK(b == 0);
    CHECK_FALSE(scheduler.contains(first));
    CHECK_FALSE(scheduler.contains(second));

    // 回调里登记的新任务留到下一轮，本轮不递归
    int later = 0;
    int rounds = 0;
    (void)scheduler.after_at(at_ms(0), ms(10), [&] {
        ++rounds;
        // 新任务以"当前 tick 的时刻"为基准，due == 20，但本轮仍不执行
        (void)scheduler.after_at(at_ms(20), ms(0), [&later] { ++later; });
    });
    CHECK(scheduler.tick(at_ms(20)) == 1);
    CHECK(rounds == 1);
    CHECK(later == 0);
    CHECK(scheduler.tick(at_ms(20)) == 1);
    CHECK(later == 1);

    CHECK_FALSE(scheduler.cancel(9999));  // 不存在的任务
    CHECK(scheduler.pending() == 0);
}

TEST_CASE("util/task: 取消与完成是两件事，空 token 恒为非") {
    TaskHandle handle;
    CancellationToken token = handle.token();
    REQUIRE(token.valid());

    CHECK_FALSE(token.cancelled());
    CHECK_FALSE(token.finished());
    CHECK_FALSE(handle.cancelled());
    CHECK_FALSE(handle.finished());

    handle.cancel();
    CHECK(token.cancelled());
    CHECK(handle.cancelled());
    CHECK_FALSE(token.finished());  // 取消不代表跑完

    handle.finish();
    CHECK(token.finished());

    // 不同句柄互不影响
    TaskHandle other;
    CHECK_FALSE(other.token().cancelled());

    // 空 token：合法但恒假
    const CancellationToken empty;
    CHECK_FALSE(empty.valid());
    CHECK_FALSE(empty.cancelled());
    CHECK_FALSE(empty.finished());
}
