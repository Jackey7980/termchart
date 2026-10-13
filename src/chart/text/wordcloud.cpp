// wordcloud.cpp —— 词云实现：阿基米德螺线 + 逐格占用碰撞检测
#include "termchart/chart/text/wordcloud.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "termchart/core/attr.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr double kSpiralStep = 0.35;  // 螺线参数步进（弧度）

// 内容区 = 画布 - 四边留白；有标题时顶行让给标题。
Rect content_area(const DrawContext& ctx, const Chart& chart) {
    const double w = static_cast<double>(ctx.canvas.width());
    const double h = static_cast<double>(ctx.canvas.height());
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, w - chart.margin_left() - chart.margin_right()),
              std::max(0.0, h - chart.margin_top() - chart.margin_bottom())};
    if (ctx.draw_title && !chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(ctx.canvas, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

// 词色分档：排名越靠前越醒目。
Color color_for_rank(std::size_t rank) noexcept {
    static const Color kPalette[] = {
        default_color(DefaultColor::Cyan), default_color(DefaultColor::Accent), default_color(DefaultColor::Amber),
        Color::ansi16(5), default_color(DefaultColor::Neutral), default_color(DefaultColor::Series),
    };
    return kPalette[rank % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

// 占用网格一行内 [col, col+width) 是否全空。
bool free_at(const std::vector<char>& occupied, std::size_t stride, std::size_t row,
             std::size_t col, std::size_t width) noexcept {
    for (std::size_t dx = 0; dx < width; ++dx) {
        if (occupied[row * stride + col + dx] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

void Wordcloud::draw(DrawContext ctx) {
    placed_ = 0;

    std::vector<Word> words;
    words.reserve(words_.size());
    for (const Word& w : words_) {
        if (!w.text.empty() && w.weight > 0.0 && std::isfinite(w.weight)) {
            words.push_back(w);
        }
    }
    std::sort(words.begin(), words.end(),
              [](const Word& a, const Word& b) { return a.weight > b.weight; });
    if (words.size() > options_.max_words) {
        words.resize(options_.max_words);
    }
    if (words.empty()) {
        return;
    }

    const Rect area = content_area(ctx, *this);
    const auto grid_w = static_cast<std::size_t>(area.right());
    const auto grid_h = static_cast<std::size_t>(area.bottom());
    if (grid_w == 0 || grid_h == 0) {
        return;
    }
    std::vector<char> occupied(grid_w * grid_h, 0);

    const double cx = area.x + area.w / 2.0;
    const double cy = area.y + area.h / 2.0;
    const double max_r = std::max(area.w, area.h) * 2.0;  // 螺线半径预算（视觉单位）

    for (std::size_t rank = 0; rank < words.size(); ++rank) {
        const Word& w = words[rank];
        const int word_w = util::display_width(w.text);
        if (word_w <= 0) {
            continue;
        }
        const auto cw = static_cast<std::size_t>(word_w);
        if (cw > grid_w) {
            continue;  // 单词独占不了画布：整词放弃，不折行
        }

        Style style = Style{}.set_fg(color_for_rank(rank));
        if (rank < options_.bold_top) {
            style.set_attr(Attr::Bold);
        }

        // 阿基米德螺线：r = spiral_scale * t；y 除以 2 补偿字符格 1:2 宽高比，
        // 落点左移半词宽使词以中心对称放置；逐格查占用，命中即写并登记。
        bool placed = false;
        for (double t = 0.0; options_.spiral_scale * t <= max_r; t += kSpiralStep) {
            const double r = options_.spiral_scale * t;
            const double px = cx + r * std::cos(t) - static_cast<double>(cw) / 2.0;
            const double py = cy + r * std::sin(t) / 2.0;

            const long long row = std::llround(py);
            const long long col = std::llround(px);
            if (row < 0 || row >= static_cast<long long>(grid_h)) {
                continue;
            }
            if (col < 0 ||
                col + static_cast<long long>(cw) > static_cast<long long>(grid_w)) {
                continue;
            }
            const auto urow = static_cast<std::size_t>(row);
            const auto ucol = static_cast<std::size_t>(col);
            if (!free_at(occupied, grid_w, urow, ucol, cw)) {
                continue;
            }

            ctx.canvas.draw_text(Point{static_cast<double>(ucol),
                                       static_cast<double>(urow)},
                                 w.text, style);
            for (std::size_t dx = 0; dx < cw; ++dx) {
                occupied[urow * grid_w + ucol + dx] = 1;
            }
            ++placed_;
            placed = true;
            break;
        }
        if (!placed && rank == 0) {
            // 画布小到中心都放不下最高权重词：直接放弃整图，避免空转。
            break;
        }
    }
}

}  // namespace termchart::chart
