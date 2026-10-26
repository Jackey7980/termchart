// pack.cpp —— 圆堆积实现：√value 半径 + 行式贪心排布（不依赖 graph/）
#include "termchart/chart/hierarchical/pack.hpp"
#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

void collect_leaves(const hier::HierNode& node, std::vector<const hier::HierNode*>& out) {
    if (node.is_leaf()) {
        if (node.value > 0.0) {
            out.push_back(&node);
        }
        return;
    }
    for (const hier::HierNode& child : node.children) {
        collect_leaves(child, out);
    }
}

}  // namespace

void PackChart::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void PackChart::draw(DrawContext ctx) {
    if (!has_root_) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    if (inner.w < 4.0 || inner.h < 3.0) {
        return;
    }

    std::vector<const hier::HierNode*> leaves;
    collect_leaves(root_, leaves);
    if (leaves.empty()) {
        return;
    }
    double vmax = 0.0;
    for (const hier::HierNode* leaf : leaves) {
        vmax = std::max(vmax, leaf->value);
    }

    // 行式贪心：最大半径由画布行高决定；横向尺寸 = 2 × 竖向半径（1:2 宽高比）。
    const double r_max = (inner.h - 1.0) / 2.0;
    const double gap = options.gap > 0.0 ? options.gap : 0.0;
    double cursor = inner.x;
    double cy = inner.y + r_max;
    for (const hier::HierNode* leaf : leaves) {
        const double r = r_max * std::sqrt(leaf->value / vmax);
        const double half_w = 2.0 * r;
        if (cursor + 2.0 * half_w + gap > inner.right() && cursor > inner.x) {
            cursor = inner.x;
            cy += 2.0 * r_max + gap;
        }
        const double cx = cursor + half_w;
        cursor += 2.0 * half_w + gap;

        const Style st = Style{}.set_fg(leaf->color);
        const double y0 = std::floor(cy - r - 1.0);
        const double y1 = std::ceil(cy + r + 1.0);
        const double x0 = std::floor(cx - half_w - 1.0);
        const double x1 = std::ceil(cx + half_w + 1.0);
        for (double y = y0; y <= y1; y += 1.0) {
            for (double x = x0; x <= x1; x += 1.0) {
                const double u = (x + 0.5 - cx) / 2.0;
                const double v = y + 0.5 - cy;
                const double rr = std::sqrt(u * u + v * v);
                if (std::fabs(rr - r) <= 0.5) {
                    canvas.put(Point{x, y}, kMarkerChar, st);
                } else if (rr < r - 0.5) {
                    canvas.put(Point{x, y}, U'\u00B7', st);  // · 内部
                }
            }
        }
        if (r >= 1.5 && !leaf->name.empty()) {
            canvas.put(Point{cx - 0.5, cy - 0.5}, hier::first_codepoint(leaf->name), st);
        }
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

bool PackChart::to_scene(scene::Scene& out, std::uint32_t width,
                         std::uint32_t height) const {
    if (!has_root_) {
        return true;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(width) - m.left - m.right,
                     static_cast<double>(height) - m.top - m.bottom};
    if (inner.w < 4.0 || inner.h < 3.0) {
        return true;
    }

    std::vector<const hier::HierNode*> leaves;
    collect_leaves(root_, leaves);
    if (leaves.empty()) {
        return true;
    }
    double vmax = 0.0;
    for (const hier::HierNode* leaf : leaves) {
        vmax = std::max(vmax, leaf->value);
    }

    // 行式贪心：最大半径由画布行高决定；横向尺寸 = 2 × 竖向半径（1:2 宽高比）。
    const double r_max = (inner.h - 1.0) / 2.0;
    const double gap = options.gap > 0.0 ? options.gap : 0.0;
    double cursor = inner.x;
    double cy = inner.y + r_max;
    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (const hier::HierNode* leaf : leaves) {
        const double r = r_max * std::sqrt(leaf->value / vmax);
        const double half_w = 2.0 * r;
        if (cursor + 2.0 * half_w + gap > inner.right() && cursor > inner.x) {
            cursor = inner.x;
            cy += 2.0 * r_max + gap;
        }
        const double cx = cursor + half_w;
        cursor += 2.0 * half_w + gap;

        const Style st = Style{}.set_fg(leaf->color);
        const double y0 = std::floor(cy - r - 1.0);
        const double y1 = std::ceil(cy + r + 1.0);
        const double x0 = std::floor(cx - half_w - 1.0);
        const double x1 = std::ceil(cx + half_w + 1.0);
        for (double y = y0; y <= y1; y += 1.0) {
            for (double x = x0; x <= x1; x += 1.0) {
                const double u = (x + 0.5 - cx) / 2.0;
                const double v = y + 0.5 - cy;
                const double rr = std::sqrt(u * u + v * v);
                if (std::fabs(rr - r) <= 0.5) {
                    sink.put(Point{x, y}, kMarkerChar, st);
                } else if (rr < r - 0.5) {
                    sink.put(Point{x, y}, U'\u00B7', st);  // · 内部
                }
            }
        }
        if (r >= 1.5 && !leaf->name.empty()) {
            sink.put(Point{cx - 0.5, cy - 0.5}, hier::first_codepoint(leaf->name), st);
        }
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
