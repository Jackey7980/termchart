// hover_chart.cpp —— HoverLayer 实现：命中 → 浮层图元 → 一次性重放
#include "termchart/gui/hover_chart.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/font.hpp"
#include "termchart/interaction/hit_test.hpp"
#include "termchart/scene/builder.hpp"

namespace termchart::gui {

namespace {

constexpr double kTooltipGap = 12.0;    // 浮层与锚点的间距（Scene 单位）
constexpr double kTooltipPad = 8.0;     // 文本到浮层内缘的留白
constexpr double kTooltipBorderW = 1.5; // 浮层边框线宽
constexpr Rgb kTooltipBg{28, 30, 34};
constexpr Rgb kTooltipBorder{120, 130, 140};
constexpr Rgb kTooltipText{232, 232, 232};

// 按 '\n' 切行。空串得到"一空行"而不是零行：浮层退化为一个小空框，
// 比"命中却什么都不显示"更容易被发现（可见性是底线）。
std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    lines.push_back(current);
    return lines;
}

}  // namespace

HoverLayer::HoverLayer(scene::Scene base,
                       std::vector<chart::HitRegion> regions,
                       std::vector<std::string> labels,
                       RenderOptions options)
    : base_(std::move(base)),
      regions_(std::move(regions)),
      labels_(std::move(labels)),
      options_(std::move(options)) {}

image::Bitmap HoverLayer::render_base() const {
    return SceneRenderer(options_).render(base_);
}

std::string HoverLayer::hit_label(double x, double y) const {
    const chart::HitRegion* region = interaction::topmost_hit(regions_, x, y);
    if (region == nullptr) {
        return std::string();
    }
    const std::uint32_t payload = region->payload();
    if (payload < 1U || payload - 1U >= labels_.size()) {
        return std::string();  // payload 0（或越界）：只有几何语义，不出浮层
    }
    return labels_[payload - 1U];
}

image::Bitmap HoverLayer::render_at(double x, double y) const {
    const std::string label = hit_label(x, y);
    if (label.empty()) {
        return render_base();
    }

    const std::vector<std::string> lines = split_lines(label);
    const double cell_w = options_.text_cell_w;
    const double cell_h = options_.text_cell_h;

    // 尺寸按字体度量算（宽字符占 2 列的口径由 gui::measure_text 保证）。
    double max_line_w = 0.0;
    for (const std::string& line : lines) {
        max_line_w = std::max(max_line_w, measure_text(line, cell_w, cell_h).width);
    }
    const double box_w = max_line_w + 2.0 * kTooltipPad;
    const double box_h = static_cast<double>(lines.size()) * cell_h + 2.0 * kTooltipPad;

    // 终端侧 tooltip 走四角 place_label_avoiding（那里有"格占用集"要避让）；
    // 像素侧浮层是不透明叠加，没有占用集，只有一条硬约束：别出画布。
    double bx = x + kTooltipGap;
    double by = y - box_h - kTooltipGap;
    if (bx + box_w > static_cast<double>(options_.width)) {
        bx = x - kTooltipGap - box_w;  // 右侧放不下 → 翻到左侧
    }
    if (by < 0.0) {
        by = y + kTooltipGap;  // 上方放不下 → 翻到下方
    }
    bx = std::clamp(bx, 0.0, std::max(0.0, static_cast<double>(options_.width) - box_w));
    by = std::clamp(by, 0.0, std::max(0.0, static_cast<double>(options_.height) - box_h));

    // 底图副本 + 浮层图元。渲染器零改动——它只是又消费了一份 Scene。
    scene::Scene frame = base_;
    scene::SceneBuilder b(frame);
    b.fill(Color::rgb(kTooltipBg));
    b.rect(Rect{bx, by, box_w, box_h});
    b.stroke(Color::rgb(kTooltipBorder), kTooltipBorderW);
    b.rect(Rect{bx, by, box_w, box_h});

    Style text_style;
    text_style.set_fg(Color::rgb(kTooltipText));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        b.text(Point{bx + kTooltipPad,
                     by + kTooltipPad + static_cast<double>(i) * cell_h},
               lines[i], text_style);
    }
    return SceneRenderer(options_).render(frame);
}

}  // namespace termchart::gui
