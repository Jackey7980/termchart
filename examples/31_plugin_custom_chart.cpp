// 31_plugin_custom_chart.cpp —— 注册自定义图表：自定义 Chart 子类注册进注册表
//
// 消费 plugin/（Plugin 基类 + PluginRegistry + Manifest）与 chart/registry.hpp
// （图表注册表）。同一套"注册工厂、按名创建"的统一模式（docs/design/registry.md）：
//   * 自定义 SparkBarChart : chart::Chart，以工厂注册进 ChartRegistry；
//   * 同名插件（Manifest + Plugin 子类）注册进 PluginRegistry，走 find/create/on_load；
//   * 两条注册表都 create 出新实例再绘制（工厂语义，生命周期归调用方）。
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/registry.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/plugin/manifest.hpp"
#include "termchart/plugin/plugin.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

// 自定义图表：柱状微图（数据确定性生成，无随机、无时钟）。
class SparkBarChart final : public chart::Chart {
public:
    SparkBarChart() {
        set_title("sparkbar - custom chart via registry");
        values_.reserve(kBars);
        for (std::size_t i = 0; i < kBars; ++i) {
            values_.push_back(0.5 + 0.45 * std::sin(static_cast<double>(i) * 0.9));
        }
    }

    void draw(chart::DrawContext ctx) override {
        canvas::Canvas& c = ctx.canvas;
        const double w = static_cast<double>(c.width());
        const double h = static_cast<double>(c.height());
        c.draw_text(Point{0.0, 0.0}, title(), Style{}.set_fg(Color::ansi16(7)));
        const double bar_w = (w - 2.0) / static_cast<double>(values_.size());
        const Style bar = Style{}.set_fg(Color::ansi16(6));
        for (std::size_t i = 0; i < values_.size(); ++i) {
            const double bh = values_[i] * (h - 4.0);
            if (bh < 1.0) {
                continue;
            }
            c.fill_rect(Rect{1.0 + static_cast<double>(i) * bar_w, h - 2.0 - bh, bar_w - 1.0, bh},
                        U'\u2588', bar);
        }
    }

private:
    static constexpr std::size_t kBars = 14;
    std::vector<double> values_;
};

// 插件壳：给自定义图表一份 manifest，宿主据此认识它。
class SparkBarPlugin final : public plugin::Plugin {
public:
    [[nodiscard]] const plugin::Manifest& manifest() const noexcept override { return manifest_; }

private:
    plugin::Manifest manifest_{"sparkbar", "1.0.0", "examples", "registers the sparkbar chart"};
};

}  // namespace

int main() {
    // 注册：工厂语义（create() 每次出新实例）。本示例无 App，直接注册即可。
    chart::ChartRegistry::instance().register_chart(
        "sparkbar", [] { return std::make_unique<SparkBarChart>(); });

    plugin::Manifest manifest;
    manifest.name = "sparkbar";
    manifest.version = "1.0.0";
    manifest.author = "examples";
    manifest.description = "custom chart plugin";
    bool ok = plugin::PluginRegistry::instance().add(
        manifest, [] { return std::make_unique<SparkBarPlugin>(); });

    // 名字比较不区分大小写；find 命中 → create + on_load（加载由宿主驱动）。
    const plugin::Manifest* found = plugin::PluginRegistry::instance().find("SPARKBAR");
    std::unique_ptr<plugin::Plugin> plug = plugin::PluginRegistry::instance().create("sparkbar");
    if (found == nullptr || plug == nullptr || !plugin::manifest_valid(*found) ||
        plug->on_load() != ErrorCode::Ok) {
        ok = false;
    }

    std::unique_ptr<chart::Chart> chart = chart::ChartRegistry::instance().create("sparkbar");
    if (chart == nullptr || !chart::ChartRegistry::instance().supports("sparkbar")) {
        ok = false;
    }

    if (ok) {
        canvas::Canvas canvas(64, 14);
        chart->draw({canvas});
        canvas.draw_text(Point{0.0, 13.0}, plugin::describe(manifest),
                         Style{}.set_fg(Color::ansi16(8)));
        if (api::write_stdout("\n== registered plugin: " + plugin::describe(manifest) +
                              " ==\n") != ErrorCode::Ok) {
            ok = false;
        }
        if (api::write_stdout(render::Renderer{}.render_full(canvas.to_buffer())) !=
            ErrorCode::Ok) {
            ok = false;
        }
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
