// test_plugin.cpp —— 插件模块：清单 / 注册表 / 静态加载 / 动态库
#include <doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/registry.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/plugin/dynamic.hpp"
#include "termchart/plugin/loader.hpp"
#include "termchart/plugin/manifest.hpp"
#include "termchart/plugin/plugin.hpp"

namespace {

using namespace termchart::plugin;
using termchart::ErrorCode;

// 记录生命周期钩子调用次数的哑插件。
class CountingPlugin : public Plugin {
public:
    explicit CountingPlugin(std::string name) { manifest_.name = std::move(name); }

    [[nodiscard]] const Manifest& manifest() const noexcept override { return manifest_; }

    termchart::Status on_load() noexcept override {
        ++loads;
        return ErrorCode::Ok;
    }
    void on_unload() noexcept override { ++unloads; }

    int loads = 0;
    int unloads = 0;

private:
    Manifest manifest_;
};

class DummyChart : public termchart::chart::Chart {
public:
    void draw(termchart::chart::DrawContext) override {}
};

}  // namespace

TEST_CASE("plugin/manifest: 合法性与单行描述") {
    SUBCASE("name 非空即合法，空名不合法") {
        Manifest valid;
        valid.name = "logger";
        CHECK(manifest_valid(valid));

        Manifest empty;
        CHECK_FALSE(manifest_valid(empty));
    }

    SUBCASE("describe 拼接 name@version，含描述时追加破折号段") {
        Manifest with_desc;
        with_desc.name = "logger";
        with_desc.version = "1.2.0";
        with_desc.description = "log sink";
        CHECK(describe(with_desc) == "logger@1.2.0 — log sink");

        Manifest no_desc;
        no_desc.name = "logger";
        no_desc.version = "1.2.0";
        CHECK(describe(no_desc) == "logger@1.2.0");
    }
}

TEST_CASE("plugin/registry: 注册、查找与工厂创建") {
    PluginRegistry& reg = PluginRegistry::instance();
    CHECK(&reg == &PluginRegistry::instance());
    reg.clear_for_test();

    Manifest m;
    m.name = "alpha";
    m.version = "1.0";
    CHECK(reg.add(m, []() { return std::make_unique<CountingPlugin>("alpha"); }));

    CHECK(reg.supports("alpha"));
    CHECK_FALSE(reg.supports("beta"));

    const Manifest* found = reg.find("alpha");
    REQUIRE(found != nullptr);
    CHECK(found->version == "1.0");
    CHECK(reg.find("beta") == nullptr);

    // 工厂语义：create() 每次产出新实例。
    auto first = reg.create("alpha");
    auto second = reg.create("alpha");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first.get() != second.get());
    auto* typed = dynamic_cast<CountingPlugin*>(first.get());
    REQUIRE(typed != nullptr);
    CHECK(typed->manifest().name == "alpha");

    CHECK(reg.create("beta") == nullptr);
    CHECK(reg.size() == 1);
}

TEST_CASE("plugin/registry: 重名、空名与空工厂被拒绝") {
    PluginRegistry& reg = PluginRegistry::instance();
    reg.clear_for_test();

    Manifest m;
    m.name = "Alpha";
    CHECK(reg.add(m, []() { return std::make_unique<CountingPlugin>("Alpha"); }));

    // 重名不区分大小写。
    Manifest dup;
    dup.name = "alpha";
    CHECK_FALSE(reg.add(dup, []() { return std::make_unique<CountingPlugin>("alpha"); }));
    CHECK(reg.size() == 1);

    // 大小写不敏感的查找口径与注册一致。
    CHECK(reg.supports("ALPHA"));
    CHECK(reg.find("ALPHA") != nullptr);

    // 空名拒绝。
    Manifest empty;
    CHECK_FALSE(reg.add(empty, []() { return std::make_unique<CountingPlugin>(""); }));
    CHECK(reg.size() == 1);

    // 空工厂拒绝。
    Manifest orphan;
    orphan.name = "gamma";
    CHECK_FALSE(reg.add(orphan, PluginFactory{}));
    CHECK_FALSE(reg.supports("gamma"));
    CHECK(reg.size() == 1);
}

TEST_CASE("plugin/registry: 快照语义（浅拷贝，注册后追加不可见）") {
    PluginRegistry& reg = PluginRegistry::instance();
    reg.clear_for_test();

    Manifest a;
    a.name = "a";
    Manifest b;
    b.name = "b";
    CHECK(reg.add(a, []() { return std::make_unique<CountingPlugin>("a"); }));
    CHECK(reg.add(b, []() { return std::make_unique<CountingPlugin>("b"); }));

    const PluginRegistry::Snapshot snapshot = reg.snapshot();
    REQUIRE(snapshot.size() == 2);

    // 快照之后注册的新条目对快照不可见（App 构造期冻结的契约）。
    Manifest c;
    c.name = "c";
    CHECK(reg.add(c, []() { return std::make_unique<CountingPlugin>("c"); }));
    CHECK(reg.size() == 3);
    CHECK(snapshot.size() == 2);

    // 注册表清空后，快照里的工厂仍然可用。
    reg.clear_for_test();
    CHECK(reg.create("a") == nullptr);
    REQUIRE(snapshot[0].factory != nullptr);
    auto from_snapshot = snapshot[0].factory();
    REQUIRE(from_snapshot != nullptr);
    CHECK(from_snapshot->manifest().name == "a");
}

TEST_CASE("plugin/registry: 枚举三件套 names/size/empty（注册序）") {
    PluginRegistry& reg = PluginRegistry::instance();
    reg.clear_for_test();
    CHECK(reg.empty());
    CHECK(reg.size() == 0);
    CHECK(reg.names().empty());

    Manifest a;
    a.name = "a";
    Manifest b;
    b.name = "b";
    CHECK(reg.add(a, []() { return std::make_unique<CountingPlugin>("a"); }));
    CHECK(reg.add(b, []() { return std::make_unique<CountingPlugin>("b"); }));

    const std::vector<std::string> names = reg.names();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "a");  // 注册序，与 snapshot 一致（非字典序）
    CHECK(names[1] == "b");
    CHECK(reg.size() == 2);
    CHECK_FALSE(reg.empty());

    reg.clear_for_test();
}

TEST_CASE("plugin/loader: ChartPlugin 注入图表注册表并可创建图表") {
    PluginRegistry::instance().clear_for_test();

    ChartPlugin plug("plugin_chart_x", "0.1.0",
                     []() { return std::make_unique<DummyChart>(); });
    CHECK(plug.manifest().name == "plugin_chart_x");
    CHECK(plug.on_load() == ErrorCode::Ok);

    // on_load 已把工厂注入 chart 注册表。
    termchart::chart::ChartRegistry& charts = termchart::chart::ChartRegistry::instance();
    CHECK(charts.supports("plugin_chart_x"));
    auto via_registry = charts.create("plugin_chart_x");
    REQUIRE(via_registry != nullptr);
    CHECK(dynamic_cast<DummyChart*>(via_registry.get()) != nullptr);

    // 未 on_load 也可直接经插件创建（工厂无状态）。
    auto direct = plug.create_chart();
    REQUIRE(direct != nullptr);
    CHECK(dynamic_cast<DummyChart*>(direct.get()) != nullptr);
}

TEST_CASE("plugin/loader: 静态引导只执行一次，失败条目不重试") {
    PluginRegistry::instance().clear_for_test();

    // 空函数拒绝登记。
    CHECK_FALSE(add_static(BootstrapFn{}));

    bool ok_a = false;
    bool ok_b = false;
    bool failed = false;
    CHECK(add_static([&ok_a]() noexcept {
        ok_a = true;
        return true;
    }));
    CHECK(add_static([&ok_b]() noexcept {
        ok_b = true;
        return true;
    }));
    CHECK(add_static([&failed]() noexcept {
        failed = true;
        return false;
    }));

    // 返回成功条数；失败的条目同样被移除。
    CHECK(load_static_plugins() == 2);
    CHECK(ok_a);
    CHECK(ok_b);
    CHECK(failed);

    // 队列已清空：重复调用不再执行任何引导。
    CHECK(load_static_plugins() == 0);
    CHECK(ok_a);
    CHECK(ok_b);
    CHECK(failed);
}

TEST_CASE("plugin/loader: register_chart_plugin 两边登记且重名拒绝") {
    PluginRegistry::instance().clear_for_test();

    ChartFactory factory = []() { return std::make_unique<DummyChart>(); };
    CHECK(register_chart_plugin("plugin_chart_y", "2.0.0", factory));

    // 插件注册表侧：可按名字 create 出 ChartPlugin。
    CHECK(PluginRegistry::instance().supports("plugin_chart_y"));
    auto created = PluginRegistry::instance().create("plugin_chart_y");
    REQUIRE(created != nullptr);
    auto* plug = dynamic_cast<ChartPlugin*>(created.get());
    REQUIRE(plug != nullptr);
    CHECK(plug->create_chart() != nullptr);

    // 静态加载侧：执行引导后图表注册表获得该工厂。
    CHECK(load_static_plugins() == 1);
    CHECK(termchart::chart::ChartRegistry::instance().supports("plugin_chart_y"));

    // 重名（不区分大小写）拒绝，两边都不登记。
    CHECK_FALSE(register_chart_plugin("PLUGIN_CHART_Y", "3.0.0", factory));
}

TEST_CASE("plugin/dynamic: 打不开的库与空路径返回错误码") {
    // 空路径 → InvalidArgument。
    auto empty = DynamicLibrary::open("");
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error() == ErrorCode::InvalidArgument);

    // 打不开 → PlatformError。
    auto missing = DynamicLibrary::open("no_such_plugin_for_tests.dll");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == ErrorCode::PlatformError);
}

TEST_CASE("plugin/dynamic: 未打开的句柄查找符号返回空，close 幂等") {
    DynamicLibrary lib;
    CHECK_FALSE(lib.is_open());
    CHECK(lib.symbol("termchart_plugin_entry") == nullptr);

    // 未打开时 close 是 no-op（析构路径同源）。
    lib.close();
    CHECK_FALSE(lib.is_open());
}
