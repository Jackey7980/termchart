// tag_cloud.hpp —— 标签云：流式排版
//
// 与 wordcloud 的分工：wordcloud 用螺线做二维自由布局，本图是**一维流式**
// 排版（像文本换行：从左到右，放不下换行）。标签宽度按**显示宽度**计算
// （util/wcwidth.hpp：CJK 宽 2 列）；权重决定顺序（降序）与醒目程度
// （前若干词加粗、按排名分档配色）。
#ifndef TERMCHART_CHART_TEXT_TAG_CLOUD_HPP
#define TERMCHART_CHART_TEXT_TAG_CLOUD_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct TagCloudOptions {
    std::size_t gap = 2;        // 标签之间的空列
    std::size_t bold_top = 3;   // 权重前若干词加粗
};

class TagCloud final : public Chart {
public:
    // 空文本或权重非有限/<= 0 的标签在 draw 时被跳过，不崩溃。
    void add_tag(std::string text, double weight) {
        tags_.push_back(Tag{std::move(text), weight});
    }
    void set_options(const TagCloudOptions& options) { options_ = options; }

    // 最近一次 draw 实际画出的标签数（画布装不下时 < 标签数）。
    [[nodiscard]] std::size_t placed_count() const noexcept { return placed_; }

    void draw(DrawContext ctx) override;

private:
    struct Tag {
        std::string text;
        double weight = 0.0;
    };
    std::vector<Tag> tags_;
    TagCloudOptions options_;
    std::size_t placed_ = 0;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_TEXT_TAG_CLOUD_HPP
