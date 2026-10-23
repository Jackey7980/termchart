// test_chart_registry.cpp —— 图表注册表
#include <doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/registry.hpp"

namespace {

using namespace termchart::chart;

class DummyChart : public Chart {
public:
    void draw(DrawContext) override {}
};

}  // namespace

TEST_CASE("chart/registry: 注册并工厂创建") {
    ChartRegistry& reg = ChartRegistry::instance();
    CHECK(&reg == &ChartRegistry::instance());

    reg.register_chart("dummy", []() { return std::make_unique<DummyChart>(); });
    CHECK(reg.supports("dummy"));
    auto c = reg.create("dummy");
    CHECK(c != nullptr);
    CHECK(dynamic_cast<DummyChart*>(c.get()) != nullptr);

    CHECK_FALSE(reg.supports("nope"));
    CHECK(reg.create("nope") == nullptr);
}

TEST_CASE("chart/registry: 枚举三件套 names/size/empty") {
    ChartRegistry& reg = ChartRegistry::instance();
    reg.register_chart("zz-dummy-a", []() { return std::make_unique<DummyChart>(); });
    reg.register_chart("zz-dummy-b", []() { return std::make_unique<DummyChart>(); });
    reg.register_chart("zz-dummy-b", []() { return std::make_unique<DummyChart>(); });  // 重名覆盖

    const std::vector<std::string> names = reg.names();
    CHECK(std::is_sorted(names.begin(), names.end()));  // 字典序，确定性输出
    CHECK(std::find(names.begin(), names.end(), "zz-dummy-a") != names.end());
    CHECK(std::find(names.begin(), names.end(), "zz-dummy-b") != names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());  // 重名不重复计

    CHECK(reg.size() == names.size());  // size 与 names 口径一致
    CHECK_FALSE(reg.empty());
    CHECK(reg.supports("zz-dummy-a"));
}
