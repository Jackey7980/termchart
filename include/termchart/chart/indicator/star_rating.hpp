// star_rating.hpp —— 星级评分
//
// 满星 ★ / 半星 ◐（评分四舍五入到半星）/ 空星 ☆，一行排开。
// 评分收敛到 [0, max_stars]，非有限按 0。
#ifndef TERMCHART_CHART_INDICATOR_STAR_RATING_HPP
#define TERMCHART_CHART_INDICATOR_STAR_RATING_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct StarRatingOptions {
    int max_stars = 5;                      // 收敛到 [1, 20]
    Color star_color = default_color(DefaultColor::Amber);    // 金黄
    bool dim_empty = true;                  // 空星是否用暗色
};

class StarRating final : public Chart {
public:
    void set_rating(double rating) { rating_ = rating; }
    void set_options(const StarRatingOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：★/◐/☆ 逐格以 TextOp 落笔，整体包在组 0。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    double rating_ = 0.0;
    StarRatingOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_INDICATOR_STAR_RATING_HPP
