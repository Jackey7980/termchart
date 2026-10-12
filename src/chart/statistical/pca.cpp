// pca.cpp —— 主成分投影实现：协方差矩阵 + 幂迭代（PC1 → 降幂 → PC2）
#include "termchart/chart/statistical/pca.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 幂迭代：返回单位主特征向量（确定性初值全 1 / √d）。矩阵奇异时提前停。
std::vector<double> power_iteration(const std::vector<std::vector<double>>& m,
                                    int max_iter, double tol) {
    const std::size_t d = m.size();
    std::vector<double> v(d, 1.0 / std::sqrt(static_cast<double>(d)));
    if (d == 0) {
        return v;
    }
    for (int iter = 0; iter < max_iter; ++iter) {
        std::vector<double> w(d, 0.0);
        for (std::size_t i = 0; i < d; ++i) {
            double acc = 0.0;
            for (std::size_t j = 0; j < d; ++j) {
                acc += m[i][j] * v[j];
            }
            w[i] = acc;
        }
        double norm = 0.0;
        for (double x : w) {
            norm += x * x;
        }
        norm = std::sqrt(norm);
        if (!(norm > 1e-300)) {
            break;  // 奇异/零矩阵：保持初值
        }
        double delta = 0.0;
        for (std::size_t i = 0; i < d; ++i) {
            const double nv = w[i] / norm;
            delta = std::max(delta, std::fabs(nv - v[i]));
            v[i] = nv;
        }
        if (delta < tol) {
            break;
        }
    }
    return v;
}

double quadratic_form(const std::vector<std::vector<double>>& m,
                      const std::vector<double>& v) noexcept {
    const std::size_t d = v.size();
    double acc = 0.0;
    for (std::size_t i = 0; i < d; ++i) {
        double row = 0.0;
        for (std::size_t j = 0; j < d; ++j) {
            row += m[i][j] * v[j];
        }
        acc += row * v[i];
    }
    return acc;
}

}  // namespace

void PcaChart::set_data(std::vector<std::vector<double>> rows) {
    rows_ = std::move(rows);
    fit();
}

void PcaChart::fit() {
    projected_.clear();
    ratio_.clear();
    // 有效行：非空、维度一致（以首有效行为准）、全有限。
    std::vector<std::vector<double>> rows;
    std::size_t d = 0;
    for (const auto& r : rows_) {
        bool ok = !r.empty();
        for (double x : r) {
            if (!std::isfinite(x)) {
                ok = false;
            }
        }
        if (!ok) {
            continue;
        }
        if (d == 0) {
            d = r.size();
        }
        if (r.size() == d) {
            rows.push_back(r);
        }
    }
    const std::size_t n = rows.size();
    if (n < 2 || d == 0) {
        return;
    }

    // 列中心化（就地）。
    std::vector<double> mean(d, 0.0);
    for (const auto& r : rows) {
        for (std::size_t j = 0; j < d; ++j) {
            mean[j] += r[j];
        }
    }
    for (std::size_t j = 0; j < d; ++j) {
        mean[j] /= static_cast<double>(n);
    }
    for (auto& r : rows) {
        for (std::size_t j = 0; j < d; ++j) {
            r[j] -= mean[j];
        }
    }

    // 协方差矩阵（除以 n）。
    std::vector<std::vector<double>> cov(d, std::vector<double>(d, 0.0));
    for (const auto& r : rows) {
        for (std::size_t i = 0; i < d; ++i) {
            for (std::size_t j = 0; j < d; ++j) {
                cov[i][j] += r[i] * r[j];
            }
        }
    }
    for (auto& row : cov) {
        for (double& x : row) {
            x /= static_cast<double>(n);
        }
    }

    const std::vector<double> v1 = power_iteration(cov, options_.max_iter, options_.tol);
    const double lambda1 = quadratic_form(cov, v1);
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            cov[i][j] -= lambda1 * v1[i] * v1[j];  // 降幂
        }
    }
    const std::vector<double> v2 = power_iteration(cov, options_.max_iter, options_.tol);
    const double lambda2 = quadratic_form(cov, v2);

    projected_.reserve(n);
    for (const auto& r : rows) {
        double p1 = 0.0;
        double p2 = 0.0;
        for (std::size_t j = 0; j < d; ++j) {
            p1 += r[j] * v1[j];
            p2 += r[j] * v2[j];
        }
        projected_.push_back(Point{p1, p2});
    }

    double trace = 0.0;
    for (std::size_t i = 0; i < d; ++i) {
        trace += cov[i][i] + lambda1 * v1[i] * v1[i];  // 还原降幂前的对角
    }
    if (trace > 0.0) {
        ratio_.push_back(std::max(lambda1, 0.0) / trace);
        ratio_.push_back(std::max(lambda2, 0.0) / trace);
    } else {
        ratio_.assign(2, 0.0);  // 全体样本重合：解释方差无意义
    }
}

void PcaChart::draw(DrawContext ctx) {
    if (projected_.empty()) {
        return;
    }
    double xmin = projected_.front().x;
    double xmax = xmin;
    double ymin = projected_.front().y;
    double ymax = ymin;
    for (const Point& p : projected_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    // 投影退化（单点/共线）给单位跨度，保证比例尺非零宽。
    if (!(xmax - xmin > 1e-12)) {
        xmin -= 1.0;
        xmax += 1.0;
    }
    if (!(ymax - ymin > 1e-12)) {
        ymin -= 1.0;
        ymax += 1.0;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "PC1", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "PC2", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    for (const Point& p : projected_) {
        ctx.canvas.put(area_.cell_of(p.x, p.y), kMarkerChar, st);
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
