// semantic.cpp —— 一票否决规则表
#include "termchart/autoviz/semantic.hpp"

#include <string>

#include "termchart/autoviz/candidate.hpp"

namespace termchart::autoviz {
namespace {

// 点编码类图种（散点/气泡/折线/平行坐标）：x 或颜色任一为分类就可能退化成
// "分类×分类"——位置编码分类没有业务意义，一律否决。
bool is_point_encoded(const std::string& name) {
    return name == chart_name::kScatter || name == chart_name::kBubble ||
           name == chart_name::kLine || name == chart_name::kParallel;
}

}  // namespace

std::vector<std::string> semantic_check(const ChartCandidate& c) {
    std::vector<std::string> out;
    if (!c.build) {
        out.emplace_back("no_builder: 候选没有建图闭包（路由 bug）");
        return out;
    }

    // ── 饼图：负值 / 分片数 ──────────────────────────────────────────────
    if (c.name == chart_name::kPie) {
        if (c.has_negative_values) {
            out.emplace_back("pie_negative: 饼图含负值（角度无定义）");
        }
        if (c.category_count > 8) {
            out.emplace_back("pie_too_many_slices: 饼图分片超过 8（扇区不可读）");
        }
        if (c.category_count < 2) {
            out.emplace_back("pie_too_few_slices: 饼图不足两个分片");
        }
    }

    // ── 词云：去重词数 ───────────────────────────────────────────────────
    if (c.name == chart_name::kWordcloud && c.category_count < 3) {
        out.emplace_back("wordcloud_too_few_words: 去重词数不足 3");
    }

    // ── 散点/气泡：点数 ──────────────────────────────────────────────────
    if ((c.name == chart_name::kScatter || c.name == chart_name::kBubble) && c.point_count < 3) {
        out.emplace_back("scatter_too_few_points: 散点不足 3 点（趋势不可读）");
    }

    // ── 折线：点数 + 时间轴有序 ──────────────────────────────────────────
    if (c.name == chart_name::kLine) {
        if (c.point_count < 2) {
            out.emplace_back("line_too_few_points: 折线不足 2 点");
        }
        if (c.x_is_temporal && !c.temporal_sorted) {
            out.emplace_back("temporal_unsorted: 时间轴未排序（折线会乱穿）");
        }
    }

    // ── 平行坐标：轴数 ───────────────────────────────────────────────────
    if (c.name == chart_name::kParallel && c.axis_count < 3) {
        out.emplace_back("parallel_too_few_axes: 平行坐标轴数不足 3（与散点无异）");
    }

    // ── 直方图：输入无差异 ───────────────────────────────────────────────
    if (c.name == chart_name::kHist && c.value_constant) {
        out.emplace_back("hist_constant: 输入无差异（单值直方图是一根棍）");
    }
    if (c.name == chart_name::kHist && c.point_count < 2) {
        out.emplace_back("hist_too_few_samples: 样本不足 2 个");
    }

    // ── 分类×分类（点编码图种）───────────────────────────────────────────
    if (is_point_encoded(c.name) && c.x_is_categorical && c.color_is_categorical) {
        out.emplace_back("categorical_x_categorical: 分类×分类无业务意义");
    }

    // ── 颜色通道基数资格 ─────────────────────────────────────────────────
    if (c.color_is_categorical) {
        if (c.color_tier == data::CardinalityTier::High) {
            out.emplace_back("high_cardinality_color: 高基数列不可作颜色通道（仅可去重计数）");
        } else if (c.color_tier == data::CardinalityTier::Mid) {
            out.emplace_back("mid_cardinality_color: 中基数列仅可 facet 不可着色");
        }
    }

    // ── 地理去重（管道内已强制，这里是安全网）────────────────────────────
    if (!c.geo_deduped) {
        out.emplace_back("geo_not_deduped: 地理数据未去重（上报次数会画成地点数量）");
    }

    return out;
}

}  // namespace termchart::autoviz
