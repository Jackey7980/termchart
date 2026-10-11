// dataset.cpp —— 数据集实现
#include "termchart/chart/dataset.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace termchart::chart {

void Dataset::add(Series series) {
    series_.push_back(std::move(series));
}

void Dataset::add(std::string name, std::vector<double> values, Color color) {
    Series series;
    series.name = std::move(name);
    series.color = color;
    series.points.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        series.points.push_back(Point{static_cast<double>(i), values[i]});
    }
    series_.push_back(std::move(series));
}

void Dataset::add(std::string name, std::vector<double> xs, std::vector<double> ys,
                  Color color) {
    Series series;
    series.name = std::move(name);
    series.color = color;
    const std::size_t count = std::min(xs.size(), ys.size());
    series.points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        series.points.push_back(Point{xs[i], ys[i]});
    }
    series_.push_back(std::move(series));
}

bool Dataset::empty() const noexcept {
    return series_.empty();
}

std::size_t Dataset::size() const noexcept {
    return series_.size();
}

std::size_t Dataset::point_count() const noexcept {
    std::size_t total = 0;
    for (const Series& series : series_) {
        total += series.points.size();
    }
    return total;
}

DataRange Dataset::bounds(bool include_zero) const noexcept {
    DataRange range;
    bool any = false;
    for (const Series& series : series_) {
        for (const Point& p : series.points) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                continue;  // 非有限值不参与定域（否则整条轴会变成 NaN）
            }
            if (!any) {
                range.xmin = range.xmax = p.x;
                range.ymin = range.ymax = p.y;
                any = true;
                continue;
            }
            range.xmin = std::min(range.xmin, p.x);
            range.xmax = std::max(range.xmax, p.x);
            range.ymin = std::min(range.ymin, p.y);
            range.ymax = std::max(range.ymax, p.y);
        }
    }
    if (!any) {
        return range;
    }
    if (include_zero) {
        range.ymin = std::min(range.ymin, 0.0);
        range.ymax = std::max(range.ymax, 0.0);
    }
    range.valid = true;
    return range;
}

void Dataset::clear() {
    series_.clear();
}

}  // namespace termchart::chart
