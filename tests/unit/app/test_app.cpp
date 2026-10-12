// test_app.cpp —— App 模块：动画 / 配置 / 投递 / headless / 主循环
#include <doctest.h>

#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "termchart/app/animation.hpp"
#include "termchart/app/app.hpp"
#include "termchart/app/config.hpp"
#include "termchart/app/headless.hpp"
#include "termchart/app/immediate_app.hpp"
#include "termchart/app/loop.hpp"
#include "termchart/app/post.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/plugin/plugin.hpp"

namespace {

using namespace termchart::app;
using termchart::canvas::Canvas;
using termchart::ErrorCode;

// App 快照测试用的最小插件（本 TU 内自带，不跨文件共享测试夹具）。
class NoopPlugin final : public termchart::plugin::Plugin {
public:
    explicit NoopPlugin(std::string name) : manifest_{std::move(name), "1.0", "", ""} {}
    [[nodiscard]] const termchart::plugin::Manifest& manifest() const noexcept override {
        return manifest_;
    }

private:
    termchart::plugin::Manifest manifest_;
};

}  // namespace

TEST_CASE("app/animation: ease 端点、钳位与非有限输入") {
    for (const Easing e : {Easing::Linear, Easing::EaseInQuad, Easing::EaseOutQuad,
                           Easing::EaseInOutQuad, Easing::EaseInCubic, Easing::EaseOutCubic,
                           Easing::EaseInOutCubic}) {
        CHECK(ease(e, 0.0) == 0.0);
        CHECK(ease(e, 1.0) == 1.0);
    }

    // 中段取值符合各曲线定义。
    CHECK(ease(Easing::Linear, 0.25) == doctest::Approx(0.25));
    CHECK(ease(Easing::EaseInQuad, 0.5) == doctest::Approx(0.25));
    CHECK(ease(Easing::EaseOutQuad, 0.5) == doctest::Approx(0.75));
    CHECK(ease(Easing::EaseInCubic, 0.5) == doctest::Approx(0.125));

    // 界外钳位：输出仍在 [0,1]。
    CHECK(ease(Easing::Linear, -0.5) == 0.0);
    CHECK(ease(Easing::Linear, 1.5) == 1.0);
    CHECK(ease(Easing::EaseInQuad, -1.0) == 0.0);
    CHECK(ease(Easing::EaseOutQuad, 2.0) == 1.0);

    // NaN/Inf 不可比较 → 视为界外 → 0。
    const double nan = 0.0 / 0.0;
    const double inf = 1.0 / 0.0;
    CHECK(ease(Easing::Linear, nan) == 0.0);
    CHECK(ease(Easing::Linear, inf) == 0.0);
}

TEST_CASE("app/animation: Animation 纯函数进度与退化时长") {
    Animation anim(2.0, Easing::EaseInQuad);
    CHECK(anim.duration() == doctest::Approx(2.0));
    CHECK(anim.easing() == Easing::EaseInQuad);

    // 未 start → 0。
    CHECK(anim.value_at(100.0) == 0.0);

    anim.start_at(10.0);
    CHECK(anim.value_at(10.0) == 0.0);
    CHECK(anim.value_at(11.0) == doctest::Approx(0.25));  // t=0.5 过 EaseInQuad
    CHECK(anim.value_at(12.0) == 1.0);
    CHECK_FALSE(anim.finished_at(11.9));
    CHECK(anim.finished_at(12.0));
    CHECK(anim.finished_at(13.0));
}

TEST_CASE("app/animation: 零/负时长立即完成") {
    Animation zero(0.0, Easing::Linear);
    CHECK(zero.duration() == 0.0);
    zero.start_at(5.0);
    CHECK(zero.value_at(5.0) == 1.0);
    CHECK(zero.finished_at(5.0));

    Animation negative(-1.0);
    CHECK(negative.duration() == 0.0);
    negative.start_at(0.0);
    CHECK(negative.value_at(0.0) == 1.0);
}

TEST_CASE("app/config: 类型化读写、覆盖与跨类型回退") {
    Config cfg;

    // 未设置 → monostate，has 为 false。
    CHECK_FALSE(cfg.has("missing"));
    CHECK(std::holds_alternative<std::monostate>(cfg.get("missing")));

    cfg.set("flag", ConfigValue{true});
    cfg.set("num", ConfigValue{static_cast<std::int64_t>(42)});
    cfg.set("ratio", ConfigValue{0.5});
    cfg.set("label", ConfigValue{std::string("hello")});
    CHECK(cfg.size() == 4);
    CHECK(cfg.has("num"));

    // 读什么类型得什么类型。
    CHECK(cfg.get_bool("flag") == true);
    CHECK(cfg.get_int("num") == 42);
    CHECK(cfg.get_double("ratio") == doctest::Approx(0.5));
    CHECK(cfg.get_string("label") == "hello");

    // 跨类型读取回退默认值，不做隐式转换。
    CHECK(cfg.get_double("num", 9.9) == doctest::Approx(9.9));
    CHECK(cfg.get_string("num", "x") == "x");
    CHECK(cfg.get_bool("num", true) == true);
    CHECK(cfg.get_int("flag", 7) == 7);

    // 同 key 覆盖写。
    cfg.set("num", ConfigValue{static_cast<std::int64_t>(100)});
    CHECK(cfg.get_int("num") == 100);
    CHECK(cfg.size() == 4);

    // remove：存在返回 true，再删返回 false。
    CHECK(cfg.remove("num"));
    CHECK_FALSE(cfg.has("num"));
    CHECK(cfg.get("num").index() == 0);  // monostate
    CHECK_FALSE(cfg.remove("num"));
}

TEST_CASE("app/config: 全局实例与便捷函数") {
    Config& global = Config::global();
    CHECK(&global == &Config::global());

    const std::string key = "app_test_convenience_key";
    set_config(key, ConfigValue{static_cast<std::int64_t>(7)});
    CHECK(get_config(key).index() == 2);  // int64
    CHECK(std::get<std::int64_t>(get_config(key)) == 7);
    CHECK(Config::global().get_int(key) == 7);

    // 清理，不给其他用例留副作用。
    CHECK(Config::global().remove(key));
    CHECK_FALSE(Config::global().has(key));
}

TEST_CASE("app/post: PostQueue 空投递忽略、take 按序取走、drain 本线程执行") {
    PostQueue queue;
    CHECK(queue.pending() == 0);
    CHECK(queue.take().empty());

    // 空 task 不占队列位。
    queue.post(PostQueue::Task{});
    CHECK(queue.pending() == 0);

    // 先投递（记录投递序），后消费。
    std::vector<int> consumed;
    for (const int v : {1, 2, 3}) {
        queue.post([&consumed, v]() { consumed.push_back(v); });
    }
    CHECK(queue.pending() == 3);

    // take：FIFO 整体取走，由消费方自行执行。
    std::vector<PostQueue::Task> tasks = queue.take();
    REQUIRE(tasks.size() == 3);
    CHECK(queue.pending() == 0);
    for (PostQueue::Task& task : tasks) {
        task();
    }
    REQUIRE(consumed.size() == 3);
    CHECK(consumed[0] == 1);
    CHECK(consumed[1] == 2);
    CHECK(consumed[2] == 3);

    // drain：取走并就地执行，返回执行数。
    queue.post([]() {});
    queue.post([]() {});
    CHECK(queue.drain() == 2);
    CHECK(queue.pending() == 0);
    CHECK(queue.drain() == 0);
}

TEST_CASE("app/post: 跨线程投递，主线程消费（先投递后消费，无竞态）") {
    // PostQueue：生产线程投 5 条，join 后主线程统一消费。
    PostQueue queue;
    {
        std::thread producer([&queue]() {
            for (int v = 0; v < 5; ++v) {
                queue.post([&queue, v]() { static_cast<void>(v); });
            }
        });
        producer.join();
    }
    CHECK(queue.pending() == 5);
    const std::vector<PostQueue::Task> tasks = queue.take();
    CHECK(tasks.size() == 5);

    // Mailbox：后写覆盖未消费旧值，主线程只看到最新值。
    Mailbox<int> mailbox;
    {
        std::thread producer([&mailbox]() {
            mailbox.post(1);
            mailbox.post(2);
        });
        producer.join();
    }
    int value = 0;
    REQUIRE(mailbox.consume(value));
    CHECK(value == 2);
    CHECK(mailbox.empty());
}

TEST_CASE("app/post: Mailbox 最新值语义与空信箱") {
    Mailbox<int> mailbox;
    int out = 0;

    // 空信箱：consume 失败，empty 为 true。
    CHECK(mailbox.empty());
    CHECK_FALSE(mailbox.consume(out));

    // 单值往返。
    mailbox.post(7);
    CHECK_FALSE(mailbox.empty());
    REQUIRE(mailbox.consume(out));
    CHECK(out == 7);
    CHECK(mailbox.empty());
    CHECK_FALSE(mailbox.consume(out));  // 取走即清空。

    // 覆盖语义：未消费的旧值被丢弃。
    mailbox.post(10);
    mailbox.post(11);
    REQUIRE(mailbox.consume(out));
    CHECK(out == 11);
    CHECK(mailbox.empty());
}

TEST_CASE("app/headless: render_once 产出非空字节串，零尺寸拒绝") {
    const auto draw = [](termchart::canvas::Canvas& frame) {
        frame.draw_text({1.0, 1.0}, "hi");
    };

    auto result = render_once(8, 4, draw);
    REQUIRE(result.has_value());
    CHECK_FALSE(result.value().empty());

    // 空画布没有可渲染的东西 → InvalidArgument。
    auto rejected = render_once(0, 4, draw);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error() == ErrorCode::InvalidArgument);
    auto rejected_h = render_once(8, 0, draw);
    REQUIRE_FALSE(rejected_h.has_value());
    CHECK(rejected_h.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("app/headless: 会话增量帧、未变空帧与 reset 全量") {
    HeadlessSession session(10, 3);
    CHECK(session.width() == 10);
    CHECK(session.height() == 3);

    // 画布内容真实落格（渲染产出非空 CellBuffer 的内容侧证据）。
    session.canvas().draw_text({0.0, 0.0}, "ab");
    CHECK(session.canvas().at(0, 0).cp == U'a');
    CHECK(session.canvas().at(1, 0).cp == U'b');

    // 首帧全量：非空。
    const std::string first = session.frame();
    CHECK_FALSE(first.empty());

    // 画面未变 → 无差异 → 空串（增量路径契约）。
    CHECK(session.frame().empty());

    // frame_full 无视 prev，始终全量非空。
    CHECK_FALSE(session.frame_full().empty());

    // reset 丢弃 prev：下一帧 frame() 必为全量。
    session.reset();
    CHECK_FALSE(session.frame().empty());
}

TEST_CASE("app/loop: 注入时钟、首帧零 delta 与 max_delta 钳制") {
    double now = 100.0;
    Loop loop([&now]() noexcept { return now; });

    std::vector<double> deltas;
    loop.add_tick([&deltas](double d) { deltas.push_back(d); });

    // 首帧 delta = 0。
    CHECK(loop.run(1) == 1);
    REQUIRE(deltas.size() == 1);
    CHECK(deltas[0] == 0.0);

    // 正常推进（delta 小于默认 max_delta=0.1，不被钳制）。
    now += 0.05;
    loop.run(1);
    CHECK(deltas.back() == doctest::Approx(0.05));

    // 掉帧保护：大跳被钳到默认 0.1。
    now += 5.0;
    loop.run(1);
    CHECK(deltas.back() == doctest::Approx(0.1));

    // 非有限或 <=0 的上限被忽略。
    loop.set_max_delta(-1.0);
    CHECK(loop.max_delta() == doctest::Approx(0.1));
    loop.set_max_delta(0.5);
    CHECK(loop.max_delta() == doctest::Approx(0.5));

    now += 2.0;
    loop.run(1);
    CHECK(deltas.back() == doctest::Approx(0.5));

    // 帧数累计跨多次 run()；run(0) 一帧不跑。
    CHECK(loop.iterations() == 4);
    CHECK(loop.run(0) == 0);
    CHECK(loop.iterations() == 4);
}

TEST_CASE("app/loop: 空回调拒绝、移除回调与 tick 内 stop") {
    Loop loop;

    // 空回调被忽略并返回 0。
    CHECK(loop.add_tick(Loop::TickFn{}) == 0);

    std::uint64_t id_a = loop.add_tick([](double) {});
    std::uint64_t id_b = loop.add_tick([](double) {});
    CHECK(id_a != 0);
    CHECK(id_b == id_a + 1);

    // remove：存在返回 true，再删返回 false。
    CHECK(loop.remove_tick(id_a));
    CHECK_FALSE(loop.remove_tick(id_a));

    // tick 内调用 stop：本帧回调后退出，即使还有剩余帧数预算。
    int calls = 0;
    loop.add_tick([&calls, &loop](double) {
        ++calls;
        if (calls == 2) {
            loop.stop();
        }
    });
    CHECK(loop.run(10) == 2);
    CHECK(calls == 2);

    // stop 标志在每次 run() 开始时清除：再次 run 正常跑满。
    CHECK(loop.run(3) == 3);
    CHECK(calls == 5);
    CHECK(loop.iterations() == 5);
}

TEST_CASE("app/immediate: 每帧重放（清→画→收），帧数与回调次数一致") {
    double now = 0.0;
    int drawn = 0;
    ImmediateApp app_(10, 4,
                      [&drawn](Canvas& c, double) {
                          ++drawn;
                          c.draw_text({0.0, 0.0}, "X");
                      },
                      [&now]() noexcept { return now; });

    now = 0.5;
    CHECK(app_.run(3) == 3);
    CHECK(drawn == 3);
    REQUIRE(app_.frames().size() == 3);
    CHECK_FALSE(app_.frames()[0].empty());

    // 跨 run() 累计：帧数与回调次数同步增长。
    now = 1.0;
    CHECK(app_.run(2) == 2);
    CHECK(drawn == 5);
    CHECK(app_.frame_count() == 5);
    CHECK(app_.frames().size() == 5);

    // 帧上限 0 → 0 帧；stop 后提前退出。
    CHECK(app_.run(0) == 0);
    app_.loop().add_tick([&app_](double) { app_.stop(); });
    CHECK(app_.run(10) == 1);
    CHECK(app_.frame_count() == 6);
}

TEST_CASE("app/App: 构造期冻结注册表快照，create_plugin 只查快照") {
    auto& registry = termchart::plugin::PluginRegistry::instance();
    registry.clear_for_test();
    REQUIRE(registry.add({"app_frozen", "1.0", "", ""},
                         [] { return std::make_unique<NoopPlugin>("app_frozen"); }));

    const App application;  // 快照在此刻冻结
    REQUIRE(registry.add({"app_late", "1.0", "", ""},
                         [] { return std::make_unique<NoopPlugin>("app_late"); }));

    CHECK(application.supports_plugin("app_frozen"));
    CHECK_FALSE(application.supports_plugin("app_late"));  // 冻结后注册不可见

    std::unique_ptr<termchart::plugin::Plugin> frozen = application.create_plugin("app_frozen");
    REQUIRE(frozen != nullptr);
    CHECK(frozen->manifest().name == "app_frozen");
    CHECK(application.create_plugin("app_late") == nullptr);
    CHECK(application.create_plugin("missing") == nullptr);

    // 全局注册表清空不影响 App 的快照；插件清单按注册顺序。
    registry.clear_for_test();
    CHECK(application.supports_plugin("app_frozen"));
    const std::vector<termchart::plugin::Manifest> list = application.plugins();
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "app_frozen");
    CHECK(application.plugin_descriptors().size() == 1);

    // config() 是全局配置入口。
    CHECK(&App::config() == &Config::global());
}
