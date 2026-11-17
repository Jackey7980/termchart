// test_app_loop.cpp —— App 主循环端到端集成测试
//
// 聚焦"注入时钟 → 多 tick 回调 → stop/移除 → 帧预算"的集成行为（与 test_app.cpp
// 的单元覆盖互补：这里验证多个回调协同与跨多次 run 的状态推进）。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/app/loop.hpp"

namespace {

using namespace termchart::app;

}  // namespace

TEST_CASE("integration/app_loop: 多 tick 回调按注册序推进，移除后不再调用") {
    double now = 0.0;
    Loop loop([&now]() noexcept { return now; });

    std::vector<int> order;
    const std::uint64_t id_a = loop.add_tick([&](double) { order.push_back(1); });
    const std::uint64_t id_b = loop.add_tick([&](double) { order.push_back(2); });
    REQUIRE(id_a != 0);
    REQUIRE(id_b != 0);

    now = 0.1;
    CHECK(loop.run(1) == 1);
    REQUIRE(order.size() == 2);
    CHECK(order[0] == 1);  // a 先注册先调用
    CHECK(order[1] == 2);

    // 移除 a：此后只剩 b。
    REQUIRE(loop.remove_tick(id_a));
    order.clear();
    now = 0.2;
    CHECK(loop.run(1) == 1);
    REQUIRE(order.size() == 1);
    CHECK(order[0] == 2);
}

TEST_CASE("integration/app_loop: 累计 delta 与帧预算相符，stop 中途退出") {
    double now = 0.0;
    Loop loop([&now]() noexcept { return now; });
    loop.set_max_delta(0.25);

    double elapsed = 0.0;
    int ticks = 0;
    loop.add_tick([&](double d) {
        elapsed += d;
        ++ticks;
        now += 0.1;  // 为下一帧推进注入时钟
        if (ticks == 3) {
            loop.stop();
        }
    });

    // 每帧推进 0.1（< max_delta）；第 3 帧 tick 内 stop → 单次 run 只跑 3 帧。
    // 注意 stop 标志每次 run() 开始清除，所以必须在一次 run 内验证中途退出。
    CHECK(loop.run(5) == 3);
    CHECK(ticks == 3);
    // 首帧 delta=0（Loop 语义），第 2/3 帧各 0.1 ⇒ 累计 0.2。
    CHECK(elapsed == doctest::Approx(0.2));
    CHECK(loop.iterations() == 3);
}

TEST_CASE("integration/app_loop: 无 tick 回调仍推进帧计数") {
    Loop loop;
    CHECK(loop.run(4) == 4);
    CHECK(loop.iterations() == 4);
}
