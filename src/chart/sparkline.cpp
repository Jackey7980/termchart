// sparkline.cpp —— 迷你折线实现
#include "termchart/chart/sparkline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/primitives.hpp"
#include "termchart/scale/linear.hpp"

namespace termchart::chart {

void Sparkline::set_values(std::vector<double> values) {
    values_ = std::move(values);
}

std::size_t Sparkline::draw(canvas::Canvas& canvas) const {
    if (values_.size() < 2 || canvas.width() == 0 || canvas.height() == 0) {
        return 0;
    }

    double ymin = 0.0;
    double ymax = 0.0;
    bool any = false;
    for (const double value : values_) {
        if (!std::isfinite(value)) {
            continue;  // 非有限值不参与定域，避免整条线变 NaN
        }
        if (!any) {
            ymin = ymax = value;
            any = true;
            continue;
        }
        ymin = std::min(ymin, value);
        ymax = std::max(ymax, value);
    }
    if (!any) {
        return 0;
    }
    if (zero_baseline_) {
        ymin = std::min(ymin, 0.0);
        ymax = std::max(ymax, 0.0);
    }

    const double last_x = static_cast<double>(values_.size() - 1);
    // x 铺满列，y 反转（格坐标向下增长）。
    const scale::LinearScale x_scale(0.0, last_x, 0.0,
                                     static_cast<double>(canvas.width()) - 1.0);
    const scale::LinearScale y_scale(ymin, ymax,
                                     static_cast<double>(canvas.height()) - 1.0, 0.0);
    const Style style = Style{}.set_fg(color_);

    std::size_t written = 0;
    const auto width = static_cast<double>(canvas.width());
    const auto height = static_cast<double>(canvas.height());
    for (std::size_t i = 1; i < values_.size(); ++i) {
        const double x0 = static_cast<double>(i - 1);
        const double x1 = static_cast<double>(i);
        if (!std::isfinite(values_[i - 1]) || !std::isfinite(values_[i])) {
            continue;  // 断点：不连线，但两端各点的标记仍会画
        }
        const Point a = Point{x_scale.forward(x0), y_scale.forward(values_[i - 1])};
        const Point b = Point{x_scale.forward(x1), y_scale.forward(values_[i])};
        const std::vector<Point> points = canvas::line_points(a, b);
        Point previous{-1.0, -1.0};
        for (const Point& p : points) {
            if (p.x == previous.x && p.y == previous.y) {
                continue;  // 取整重复点不重复计数
            }
            previous = p;
            if (p.x >= 0.0 && p.y >= 0.0 && p.x < width && p.y < height) {
                ++written;
            }
            canvas.put(p, kLineChar, style);
        }
    }

    if (marker_) {
        // 末点单独标出来：迷你图最需要读的就是"现在在哪儿"。
        const double last_value = values_.back();
        if (std::isfinite(last_value)) {
            const Point last{x_scale.forward(last_x), y_scale.forward(last_value)};
            if (last.x >= 0.0 && last.y >= 0.0 && last.x < width && last.y < height) {
                ++written;
            }
            canvas.put(last, kMarkerChar, style);
        }
    }
    return written;
}

}  // namespace termchart::chart
