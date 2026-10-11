// dataset.hpp —— 数据集（系列集合 + 轴名 + 范围查询）
//
// `Dataset` 是"画图前的那份数据"：若干具名系列 + 两个轴名，并提供图表需要的
// 范围查询（自动定义域、是否含零）。它**不认识画布**，因此 GUI 侧、导出路径与
// 未来的 DSL 都能复用它作为中间表示 —— 这也是它独立成文件（而不是塞进 chart.hpp）
// 的原因。
#ifndef TERMCHART_CHART_DATASET_HPP
#define TERMCHART_CHART_DATASET_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/series.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct DataRange {
    double xmin = 0.0;
    double xmax = 0.0;
    double ymin = 0.0;
    double ymax = 0.0;
    bool valid = false;  // 没有任何点时 false，其余字段无意义
};

class Dataset {
public:
    Dataset() = default;

    TERMCHART_API void add(Series series);
    // 便捷：只有 y 值时 x 取 0..n-1（最常用的调用形态）。
    TERMCHART_API void add(std::string name, std::vector<double> values,
                           Color color = Color::ansi16(7));
    // 便捷：x 与 y 等长，长度不等时按较短者截断（不抛异常）。
    TERMCHART_API void add(std::string name, std::vector<double> xs,
                           std::vector<double> ys, Color color = Color::ansi16(7));

    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;   // 系列数
    [[nodiscard]] TERMCHART_API std::size_t point_count() const noexcept;  // 点数合计

    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }
    [[nodiscard]] std::vector<Series>& series() noexcept { return series_; }

    void set_x_label(std::string label) { x_label_ = std::move(label); }
    void set_y_label(std::string label) { y_label_ = std::move(label); }
    [[nodiscard]] const std::string& x_label() const noexcept { return x_label_; }
    [[nodiscard]] const std::string& y_label() const noexcept { return y_label_; }

    // 数据范围；include_zero 为真时把 0 纳入 y 域（柱状图需要）。
    [[nodiscard]] TERMCHART_API DataRange bounds(bool include_zero = false) const noexcept;

    TERMCHART_API void clear();

private:
    std::vector<Series> series_;
    std::string x_label_;
    std::string y_label_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_DATASET_HPP
