// corr.cpp —— 相关矩阵实现：Pearson r 量化 5 档字符 + 符号前景色
#include "termchart/chart/statistical/corr.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

#include "termchart/chart/scene_sink.hpp"

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

// |r| → 字符档位：'·'/'░'/'▒'/'▓'/'█'，阈值 0.15/0.40/0.65/0.85。
char32_t level_char(double r) noexcept {
    const double a = r < 0.0 ? -r : r;
    if (a < 0.15) {
        return U'\u00B7';  // ·
    }
    if (a < 0.40) {
        return U'\u2591';  // ░
    }
    if (a < 0.65) {
        return U'\u2592';  // ▒
    }
    if (a < 0.85) {
        return U'\u2593';  // ▓
    }
    return U'\u2588';  // █
}

// 符号色：近零暗灰（档位字符已足够弱），正青、负红。
Color sign_color(double r) noexcept {
    const double a = r < 0.0 ? -r : r;
    if (a < 0.15) {
        return default_color(DefaultColor::Muted);
    }
    return r > 0.0 ? default_color(DefaultColor::Cyan) : default_color(DefaultColor::Warn);
}

// 按码点截断（UTF-8）；残缺序列直接丢弃，避免画出非法字节。
std::string_view clip_utf8(std::string_view s, std::size_t max_chars) noexcept {
    std::size_t chars = 0;
    std::size_t i = 0;
    while (i < s.size() && chars < max_chars) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if (c >= 0x80) {
            len = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        }
        if (i + len > s.size()) {
            break;
        }
        i += len;
        ++chars;
    }
    return s.substr(0, i);
}

}  // namespace

double CorrMatrixChart::pearson(const std::vector<double>& a,
                                const std::vector<double>& b) noexcept {
    const std::size_t n = std::min(a.size(), b.size());
    if (n == 0) {
        return 0.0;
    }
    double sum_a = 0.0;
    double sum_b = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum_a += a[i];
        sum_b += b[i];
    }
    const double mean_a = sum_a / static_cast<double>(n);
    const double mean_b = sum_b / static_cast<double>(n);
    double cov = 0.0;
    double var_a = 0.0;
    double var_b = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double da = a[i] - mean_a;
        const double db = b[i] - mean_b;
        cov += da * db;
        var_a += da * da;
        var_b += db * db;
    }
    if (var_a <= 0.0 || var_b <= 0.0) {
        return 0.0;  // 常数列无相关
    }
    return cov / std::sqrt(var_a * var_b);
}

void CorrMatrixChart::set_matrix(std::vector<std::string> names,
                                 std::vector<std::vector<double>> columns) {
    names_ = std::move(names);
    columns_ = std::move(columns);
    const std::size_t n = columns_.size();
    r_.assign(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            r_[i][j] = (i == j) ? 1.0 : pearson(columns_[i], columns_[j]);
        }
    }
}

void CorrMatrixChart::draw(DrawContext ctx) {
    const std::size_t n = r_.size();
    if (n == 0) {
        return;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{
        m.left, m.top,
        static_cast<double>(ctx.canvas.width()) - m.left - m.right,
        static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 1.0 || inner.h < 1.0) {
        return;
    }
    const double nw = inner.w / static_cast<double>(n);
    const double nh = inner.h / static_cast<double>(n);
    const double cw = (nw >= options_.cell_w) ? options_.cell_w : (nw >= 1.0 ? nw : 1.0);
    const double ch = (nh >= options_.cell_h) ? options_.cell_h : (nh >= 1.0 ? nh : 1.0);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            const double r = r_[i][j];
            ctx.canvas.fill_rect(Rect{inner.x + static_cast<double>(j) * cw,
                                      inner.y + static_cast<double>(i) * ch, cw, ch},
                                 level_char(r), Style{}.set_fg(sign_color(r)));
        }
    }

    // 行名写在左侧留白区（截到 margin_left - 1 格，至少留 1 格间隔）。
    if (!names_.empty()) {
        const std::size_t max_chars =
            static_cast<std::size_t>(m.left >= 1.0 ? static_cast<std::size_t>(m.left) - 1 : 0);
        if (max_chars > 0) {
            for (std::size_t i = 0; i < n && i < names_.size(); ++i) {
                if (names_[i].empty()) {
                    continue;
                }
                ctx.canvas.draw_text(
                    Point{0.0, inner.y + static_cast<double>(i) * ch},
                    clip_utf8(names_[i], max_chars), Style{}.set_fg(default_color(DefaultColor::Series)));
            }
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool CorrMatrixChart::to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const {
    const std::size_t n = r_.size();
    if (n == 0) {
        return true;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{
        m.left, m.top,
        static_cast<double>(width) - m.left - m.right,
        static_cast<double>(height) - m.top - m.bottom};
    if (inner.w < 1.0 || inner.h < 1.0) {
        return true;
    }
    const double nw = inner.w / static_cast<double>(n);
    const double nh = inner.h / static_cast<double>(n);
    const double cw = (nw >= options_.cell_w) ? options_.cell_w : (nw >= 1.0 ? nw : 1.0);
    const double ch = (nh >= options_.cell_h) ? options_.cell_h : (nh >= 1.0 ? nh : 1.0);

    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            const double r = r_[i][j];
            b.fill(sign_color(r));
            b.rect(Rect{inner.x + static_cast<double>(j) * cw,
                        inner.y + static_cast<double>(i) * ch, cw, ch});
        }
    }
    b.end_group(0);

    // 行名写在左侧留白区（截到 margin_left - 1 格）。
    if (!names_.empty()) {
        const std::size_t max_chars =
            static_cast<std::size_t>(m.left >= 1.0 ? static_cast<std::size_t>(m.left) - 1 : 0);
        if (max_chars > 0) {
            for (std::size_t i = 0; i < n && i < names_.size(); ++i) {
                if (names_[i].empty()) {
                    continue;
                }
                sink.text(Point{0.0, inner.y + static_cast<double>(i) * ch},
                          clip_utf8(names_[i], max_chars),
                          Style{}.set_fg(default_color(DefaultColor::Series)));
            }
        }
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
