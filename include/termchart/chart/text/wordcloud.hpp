// wordcloud.hpp —— 词云：螺旋布局
//
// 布局策略：词按权重降序，从画布中心沿阿基米德螺线找第一个无碰撞的落点。
// 碰撞检测按**显示宽度**逐格判定（util/wcwidth.hpp：中日韩全宽字符占 2 列），
// 因此中文词不会互相覆盖；螺线纵坐标除以 2 补偿字符格约 1:2 的宽高比。
// 布局是字符格粒度的（坐标按格吸附），不做子像素，不做假精度。
// "字号"用属性位与颜色表达：权重最高的若干词加粗、按排名分档配色。
#ifndef TERMCHART_CHART_TEXT_WORDCLOUD_HPP
#define TERMCHART_CHART_TEXT_WORDCLOUD_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct WordcloudOptions {
    std::size_t max_words = 64;   // 最多画的词数
    double spiral_scale = 1.0;    // 螺旋扩张速度（格/弧度），过小则词挤在中心
    std::size_t bold_top = 3;     // 权重前若干词加粗
};

class Wordcloud final : public Chart {
public:
    // 空文本或权重非有限/<= 0 的词在 draw 时被跳过，不崩溃。
    void add_word(std::string text, double weight) {
        words_.push_back(Word{std::move(text), weight});
    }
    void set_options(const WordcloudOptions& options) { options_ = options; }

    // 最近一次 draw 实际放置的词数（画布太小时 < 词数）。
    [[nodiscard]] std::size_t placed_count() const noexcept { return placed_; }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：螺线布局的标签，按 (col,row)+字符+颜色复刻 draw()
    // （字号在矢量 Scene 里以加粗+配色表达，碰撞网格与 draw() 同口径）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    struct Word {
        std::string text;
        double weight = 0.0;
    };
    std::vector<Word> words_;
    WordcloudOptions options_;
    std::size_t placed_ = 0;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_TEXT_WORDCLOUD_HPP
