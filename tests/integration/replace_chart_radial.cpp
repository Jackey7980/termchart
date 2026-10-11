// replace_chart_radial.cpp —— 可扩展性实验 B（ROADMAP 阶段 3 契约验证点）
//
// 命题：**新增一个图表类型不需要改库任何一行**——实现 chart::Chart 接口，
// 经 ChartRegistry 登记即可被 create("radial") 取用。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/chart/registry.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

// 径向图：每个值一条从中心向外、长度正比于数值的辐条。
class RadialChart final : public chart::Chart {
public:
    void set_values(std::vector<double> values) { values_ = std::move(values); }

    void draw(chart::DrawContext ctx) override {
        const double cx = static_cast<double>(ctx.canvas.width()) / 2.0;
        const double cy = static_cast<double>(ctx.canvas.height()) / 2.0;
        const double rmax = std::min(cx, cy) - 1.0;
        const std::size_t n = values_.size();
        if (n == 0) {
            return;
        }
        constexpr double kTwoPi = 6.28318530717958647692;
        const Style style = Style{}.set_fg(Color::ansi16(5));
        for (std::size_t i = 0; i < n; ++i) {
            const double angle = kTwoPi * static_cast<double>(i) / static_cast<double>(n);
            const double len = values_[i] * rmax;
            ctx.canvas.draw_line(Point{cx, cy},
                                 Point{cx + len * std::cos(angle),
                                       cy + len * std::sin(angle)},
                                 U'*', style);
        }
    }

private:
    std::vector<double> values_;
};

}  // namespace

TEST_CASE("replace/chart: 自定义图表经注册表接入，核心零改动") {
    chart::ChartRegistry& registry = chart::ChartRegistry::instance();
    registry.register_chart("radial", [] { return std::make_unique<RadialChart>(); });

    CHECK(registry.supports("radial"));
    std::unique_ptr<chart::Chart> created = registry.create("radial");
    REQUIRE(created != nullptr);
    auto* radial = dynamic_cast<RadialChart*>(created.get());
    REQUIRE(radial != nullptr);

    radial->set_values({1.0, 0.5, 0.75});
    canvas::Canvas canvas(21, 21);
    radial->draw({canvas});

    std::size_t nonblank = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++nonblank;
            }
        }
    }
    CHECK(nonblank > 0);
}
