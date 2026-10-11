// test_chart_registry.cpp —— 图表注册表
#include <doctest.h>

#include <memory>

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
