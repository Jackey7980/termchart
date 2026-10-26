// chart.cpp —— Chart 基类虚表 + 折线/柱状图共享几何辅助
#include "termchart/chart/chart.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "termchart/layout/canvas_sink.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/sink.hpp"
#include "termchart/scale/auto_domain.hpp"
#include "termchart/util/math.hpp"

namespace termchart::chart {

Chart::~Chart() = default;

bool Chart::to_scene(scene::Scene& /*out*/, std::uint32_t /*width*/,
                     std::uint32_t /*height*/) const {
    return false;  // 默认：尚未迁移，调用方回退到 draw(Canvas)
}

void Chart::add_animation(std::string name, AnimationTrack track) {
    // 时长/延迟净化：非有限值在比较里会"全部为假"，静默变成永不完成/立即完成，
    // 所以在入口就归一成有限数，下游不必重复防。
    if (!util::is_finite(track.duration)) {
        track.duration = 0.0;
    }
    if (!util::is_finite(track.delay) || track.delay < 0.0) {
        track.delay = 0.0;
    }
    // 重建状态而不是只改配置：同名重登记是"覆盖"，应该从头再播。
    tracks_.insert_or_assign(std::move(name), AnimationState{track});
}

void Chart::add_animation(std::string name, double duration_seconds, Easing easing,
                          double delay) {
    add_animation(std::move(name), AnimationTrack{duration_seconds, delay, easing});
}

void Chart::clear_animations() noexcept {
    tracks_.clear();
    ticking_ = false;
}

void Chart::tick(double now_seconds) noexcept {
    if (!util::is_finite(now_seconds)) {
        return;
    }
    double dt = 0.0;
    if (!ticking_) {
        clock_origin_ = now_seconds;  // 首帧即起点：省掉"必须先 start"的隐式约定
        ticking_ = true;
    } else {
        dt = now_seconds - clock_now_;
        if (!(dt > 0.0)) {
            dt = 0.0;  // 同一时刻重复 tick / 时钟回拨：不推进（累计 dt 不会被倒退）
        }
    }
    clock_now_ = now_seconds;
    // dt == 0 也会把状态切成"已启动"：首帧 tick 之后进度必须是 0，而不是 1。
    for (auto& entry : tracks_) {
        entry.second.tick(dt);
    }
}

void Chart::restart_animations(double now_seconds) noexcept {
    if (!util::is_finite(now_seconds)) {
        return;
    }
    clock_origin_ = now_seconds;
    clock_now_ = now_seconds;
    ticking_ = true;
    for (auto& entry : tracks_) {
        entry.second.reset();
        entry.second.tick(0.0);  // 立即启动：restart 之后进度应为 0，不是 1
    }
}

double Chart::progress_of(const std::string& name) const noexcept {
    if (tracks_.empty() || !ticking_) {
        return 1.0;  // 没登记 / 没起跑：静态图，行为与加入动画之前一致
    }
    const auto it = tracks_.find(name);
    if (it == tracks_.end()) {
        return 1.0;  // 没登记的组一律全显示（不能因为别人有动画就消失）
    }
    return it->second.progress();
}

bool Chart::animating() const noexcept {
    if (tracks_.empty() || !ticking_) {
        return false;
    }
    for (const auto& entry : tracks_) {
        if (!entry.second.finished()) {
            return true;
        }
    }
    return false;
}

void Chart::animation_groups(std::vector<GroupProgress>& /*out*/) const {
    // 默认：该图种不提供"组 → 进度"映射（to_scene 可能是 false，也可能
    // 是一个不该被动画改写的 Scene）。apply_animation 收到空表即原样返回。
}

void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax,
                 double& ymin, double& ymax) {
    xmin = ymin = std::numeric_limits<double>::infinity();
    xmax = ymax = -std::numeric_limits<double>::infinity();
    for (const auto& s : series) {
        for (const Point& p : s.points) {
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }
    }
}

layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin,
                           double xmin, double xmax, double ymin, double ymax,
                           bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override) {
    return make_area(canvas.width(), canvas.height(), margin, xmin, xmax, ymin, ymax,
                     auto_domain, include_zero, y_override);
}

layout::PlotArea make_area(std::uint32_t width, std::uint32_t height,
                           const layout::Margin& margin, double xmin, double xmax,
                           double ymin, double ymax, bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override) {
    const double w = static_cast<double>(width);
    const double h = static_cast<double>(height);
    if (!auto_domain) {
        return layout::PlotArea(w, h, margin, xmin, xmax, ymin, ymax);
    }
    const scale::Domain xd = scale::nice_domain(xmin, xmax, false);
    if (y_override.has_value()) {
        // Y 用权威域、X 保持原有 nice 规整——调用方只买"轴域不许取极值"这一项，
        // 不该顺带改掉 X 的外观。退化/非有限域当没给（否则比例尺 span=0 → 除零）。
        const double lo = y_override->min;
        const double hi = y_override->max;
        if (std::isfinite(lo) && std::isfinite(hi) && hi > lo) {
            return layout::PlotArea(w, h, margin, xd.min, xd.max, lo, hi);
        }
    }
    const scale::Domain yd = scale::nice_domain(ymin, ymax, include_zero);
    return layout::PlotArea(w, h, margin, xd.min, xd.max, yd.min, yd.max);
}

void draw_legend_for(layout::Sink& sink, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show) {
    if (!show || series.empty()) {
        return;
    }
    std::vector<layout::LegendEntry> entries;
    entries.reserve(series.size());
    for (const auto& s : series) {
        entries.push_back(layout::LegendEntry{s.name, s.color});
    }
    const Rect inner = area.inner_rect();
    const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
    layout::draw_legend(sink, entries, legend_rect, layout::HAlign::Left);
}

void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show) {
    layout::CanvasSink sink(canvas);
    draw_legend_for(sink, series, area, show);
}

}  // namespace termchart::chart
