// sunburst.cpp —— 旭日图实现：极坐标逐格采样（布局自给，不依赖 graph/）
#include "termchart/chart/hierarchical/sunburst.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

struct Seg {
    std::size_t ring = 0;  // 0 = 最内环
    double a0 = 0.0;       // 角向区间 [a0, a1)，单位弧度，[0, 2π)
    double a1 = 0.0;
    Color color = Color::none();
};

void collect(const hier::HierNode& node, double a0, double a1, std::size_t ring,
             bool push_self, std::vector<Seg>& out) {
    if (push_self) {
        out.push_back(Seg{ring, a0, a1, node.color});
    }
    double total = 0.0;
    for (const hier::HierNode& child : node.children) {
        total += child.value > 0.0 ? child.value : 0.0;
    }
    if (total <= 0.0) {
        return;
    }
    double acc = a0;
    for (const hier::HierNode& child : node.children) {
        const double v = child.value > 0.0 ? child.value : 0.0;
        const double span = (a1 - a0) * (v / total);
        collect(child, acc, acc + span, ring + 1, true, out);
        acc += span;
    }
}

std::size_t max_depth(const hier::HierNode& node) {
    std::size_t d = 0;
    for (const hier::HierNode& child : node.children) {
        d = std::max(d, max_depth(child));
    }
    return d + 1;
}

}  // namespace

void Sunburst::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void Sunburst::draw(DrawContext ctx) {
    if (!has_root_ || root_.value <= 0.0) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const double w = static_cast<double>(canvas.width()) - m.left - m.right;
    const double h = static_cast<double>(canvas.height()) - m.top - m.bottom;
    if (w < 3.0 || h < 3.0) {
        return;
    }
    const double cx = m.left + w / 2.0;
    const double cy = m.top + h / 2.0;

    std::vector<Seg> segs;
    collect(root_, 0.0, 6.28318530717958647692, options.show_root ? 0 : 1,
            options.show_root, segs);
    if (segs.empty()) {
        return;
    }

    // 修正终端 1:2 宽高比：横向坐标先除以 2 再算极径，环宽按"行"计。
    const double rx = w / 2.0 / 2.0;
    const double ry = h / 2.0;
    const double r_max = std::sqrt(rx * rx + ry * ry);
    const std::size_t rings = max_depth(root_);
    const double ring_w = r_max / static_cast<double>(rings);

    for (double y = m.top; y < m.top + h; y += 1.0) {
        for (double x = m.left; x < m.left + w; x += 1.0) {
            const double u = (x + 0.5 - cx) / 2.0;
            const double v = y + 0.5 - cy;
            const double r = std::sqrt(u * u + v * v);
            if (r > r_max) {
                continue;
            }
            std::size_t ring = static_cast<std::size_t>(r / ring_w);
            if (ring >= rings) {
                ring = rings - 1;
            }
            double a = std::atan2(v, u);
            if (a < 0.0) {
                a += 6.28318530717958647692;
            }
            for (const Seg& seg : segs) {
                if (seg.ring != ring || a < seg.a0 || a >= seg.a1) {
                    continue;
                }
                canvas.put(Point{x, y}, kBarChar, Style{}.set_fg(seg.color));
                break;
            }
        }
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, Rect{m.left, m.top, w, h});
    }
}

}  // namespace termchart::chart
