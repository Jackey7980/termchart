// tooltip.cpp —— 悬停数据标签浮层实现（纯绘制 + 还原，无终端/渲染依赖）
#include "termchart/interaction/tooltip.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "termchart/interaction/detail/overlay_cells.hpp"
#include "termchart/layout/label_avoid.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::interaction {
namespace {

// 浮点 → 非负格坐标（与 canvas 落格口径一致）。
std::uint32_t floor_cell(double value) noexcept {
    if (!(value > 0.0)) {
        return 0;
    }
    const double floored = std::floor(value);
    constexpr double kMax = 4294967295.0;
    if (floored >= kMax) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(floored);
}

// 几何比较容差：候选矩形都是"整数量级"，而 probe 的 x 经过 (cand.x - gap) + gap
// 一来一回，可能引入 ~1e-16 的浮点误差；真正的不同候选相位差至少 1 格，故 1e-9 安全。
bool same_rect(const Rect& a, const Rect& b) noexcept {
    constexpr double kEps = 1e-9;
    const auto near = [](double l, double r) {
        return std::fabs(l - r) <= kEps;
    };
    return near(a.x, b.x) && near(a.y, b.y) && near(a.w, b.w) && near(a.h, b.h);
}

// 复用 layout::place_label_avoiding 的既有判定做"该矩形是否可用"的探测：
// 把它当作 probe 的**右**候选（anchor = cand 左上角 - gap），若它可用则函数会
// 在首次迭代就返回它本身。于是重叠检测、gap、半开区间、越界规则全部复用，
// 只有候选位从 label_avoid 的四条边改成了四角。
bool rect_is_free(const Rect& cand, std::uint32_t box_w, std::uint32_t box_h, double gap,
                  const std::vector<Rect>& occupied, const Rect& bounds) {
    const layout::LabelBox probe{Point{cand.x - gap, cand.y}, box_w, box_h};
    const layout::LabelSlot slot =
        layout::place_label_avoiding(probe, occupied, layout::LabelAvoidOptions{gap, bounds});
    return slot.placed && same_rect(slot.rect, cand);
}

struct Fitted {
    std::string text;
    int width = 0;
};

// 按显示宽度把单行裁到 budget 以内：装得下则原样；装不下则留一个省略号位后截断
// （用 prefix_by_width，绝不切在组合记号中间）。省略号本身都放不下时硬截断、不加号。
Fitted fit_line(std::string_view line, int budget, const TooltipOptions& opt) {
    const int width = util::display_width(line, opt.width_options);
    if (width <= budget) {
        return Fitted{std::string(line), width};
    }
    const int ellipsis_w = util::codepoint_width(opt.ellipsis, opt.width_options);
    if (ellipsis_w > budget) {
        const std::size_t n = util::prefix_by_width(line, budget, opt.width_options);
        std::string text(line.substr(0, n));
        return Fitted{text, util::display_width(text, opt.width_options)};
    }
    const std::size_t n = util::prefix_by_width(line, budget - ellipsis_w, opt.width_options);
    std::string text(line.substr(0, n));
    text += util::to_utf8(opt.ellipsis);
    return Fitted{text, util::display_width(text, opt.width_options)};
}

}  // namespace

Tooltip::Tooltip(TooltipOptions options) noexcept : options_(options) {}

void Tooltip::set_options(TooltipOptions options) noexcept { options_ = options; }

const TooltipOptions& Tooltip::options() const noexcept { return options_; }

// 簿记三件套统一走 detail::overlay_cells.hpp（crosshair 与 tooltip 共用唯一实现）。
void Tooltip::reset_captured(std::uint32_t width, std::uint32_t height) {
    captured_w_ = width;
    captured_h_ = height;
    detail::begin_log(patches_, captured_, width, height);
}

void Tooltip::capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y) {
    detail::capture_cell(canvas, patches_, captured_, captured_w_, x, y);
}

bool Tooltip::restore(canvas::Canvas& canvas) {
    const bool restored = detail::restore_cells(canvas, patches_, captured_);
    if (restored) {
        captured_w_ = 0;
        captured_h_ = 0;
    }
    return restored;
}

TooltipPlacement Tooltip::draw(canvas::Canvas& canvas, Point anchor, std::string_view text) {
    return draw(canvas, anchor, std::vector<std::string>{std::string(text)});
}

TooltipPlacement Tooltip::draw(canvas::Canvas& canvas, Point anchor,
                               const std::vector<std::string>& lines) {
    restore(canvas);  // 幂等：先还原上一次浮层，再按新内容重绘。

    const Rect canvas_rect{0.0, 0.0, static_cast<double>(canvas.width()),
                           static_cast<double>(canvas.height())};
    // 锚点必须在画布内；bounds 提供时还要落在允许区域内。退化 → 明确不画。
    if (!canvas_rect.contains(anchor)) {
        return TooltipPlacement{};
    }
    if (!options_.bounds.empty() && !options_.bounds.contains(anchor)) {
        return TooltipPlacement{};
    }
    if (lines.empty()) {
        return TooltipPlacement{};
    }

    // 预算：max_width==0（或大到 int 装不下）视为不限宽。
    int budget = std::numeric_limits<int>::max();
    if (options_.max_width != 0 && options_.max_width <= 0x7fffffffu) {
        budget = static_cast<int>(options_.max_width);
    }

    std::vector<Fitted> fitted;
    fitted.reserve(lines.size());
    int content_w = 0;
    for (const std::string& line : lines) {
        fitted.push_back(fit_line(line, budget, options_));
        if (fitted.back().width > content_w) {
            content_w = fitted.back().width;
        }
    }
    if (content_w <= 0) {
        return TooltipPlacement{};  // 全零宽（空文本等）：没有可画内容。
    }

    const auto box_w = static_cast<std::uint32_t>(content_w + 2);
    const auto box_h = static_cast<std::uint32_t>(fitted.size() + 2);
    const double gap = options_.gap >= 0.0 ? options_.gap : 0.0;
    const double w = static_cast<double>(box_w);
    const double h = static_cast<double>(box_h);
    const double ax = std::floor(anchor.x);
    const double ay = std::floor(anchor.y);

    // 角位候选：右上 → 左上 → 右下 → 左下（"翻边"由此自然产生）。
    const Rect candidates[4] = {
        Rect{ax + gap, ay - gap - h, w, h},      // 右上
        Rect{ax - gap - w, ay - gap - h, w, h},  // 左上
        Rect{ax + gap, ay + gap, w, h},          // 右下
        Rect{ax - gap - w, ay + gap, w, h},      // 左下
    };
    // 锚点格恒计入占用：即使 gap == 0，浮层也绝不覆盖锚点。
    const std::vector<Rect> occupied{Rect{ax, ay, 1.0, 1.0}};

    Rect chosen{};
    bool placed = false;
    for (const Rect& candidate : candidates) {
        if (rect_is_free(candidate, box_w, box_h, gap, occupied, options_.bounds)) {
            chosen = candidate;
            placed = true;
            break;
        }
    }
    if (!placed) {
        return TooltipPlacement{};  // 四角都放不下：放弃绘制，而不是硬塞。
    }

    reset_captured(canvas.width(), canvas.height());
    const std::uint32_t x0 = floor_cell(chosen.x);
    const std::uint32_t y0 = floor_cell(chosen.y);

    // 先整框快照（含边框格与宽字符续接格），再落笔 —— 浮层是不透明替换。
    for (std::uint32_t dy = 0; dy < box_h; ++dy) {
        for (std::uint32_t dx = 0; dx < box_w; ++dx) {
            capture(canvas, x0 + dx, y0 + dy);
        }
    }

    // 底板：整框铺空格 + box_style（顺带把内部补白对齐成空格）。
    canvas.fill_rect(Rect{static_cast<double>(x0), static_cast<double>(y0), w, h}, U' ',
                     options_.box_style);

    // 边框：四角 + 上下横线 + 左右竖线。
    for (std::uint32_t dx = 0; dx < box_w; ++dx) {
        const char32_t top = (dx == 0)                 ? options_.corner_tl
                             : (dx == box_w - 1)       ? options_.corner_tr
                                                       : options_.border_h;
        const char32_t bottom = (dx == 0)              ? options_.corner_bl
                                : (dx == box_w - 1)    ? options_.corner_br
                                                       : options_.border_h;
        canvas.put(x0 + dx, y0, top, options_.border_style);
        canvas.put(x0 + dx, y0 + box_h - 1, bottom, options_.border_style);
    }
    for (std::uint32_t dy = 1; dy + 1 < box_h; ++dy) {
        canvas.put(x0, y0 + dy, options_.border_v, options_.border_style);
        canvas.put(x0 + box_w - 1, y0 + dy, options_.border_v, options_.border_style);
    }

    // 文本：逐行落在内区左上角（底板已把余下列补成空格，右边框对齐）。
    for (std::size_t i = 0; i < fitted.size(); ++i) {
        canvas.draw_text(Point{static_cast<double>(x0 + 1),
                               static_cast<double>(y0 + 1 + static_cast<std::uint32_t>(i))},
                         fitted[i].text, options_.box_style);
    }

    return TooltipPlacement{chosen, true};
}

bool Tooltip::clear(canvas::Canvas& canvas) { return restore(canvas); }

bool Tooltip::visible() const noexcept { return !patches_.empty(); }

std::size_t Tooltip::patch_count() const noexcept { return patches_.size(); }

}  // namespace termchart::interaction
