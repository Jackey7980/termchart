// test_layout_extras.cpp —— facet 划分、自动留白、标签避让、参考线/高亮/标注
#include <doctest.h>

#include <cstddef>
#include <limits>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/annotation.hpp"
#include "termchart/layout/auto_margin.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/facet.hpp"
#include "termchart/layout/label_avoid.hpp"

namespace {

using namespace termchart;
using namespace termchart::layout;

bool same_rect(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

}  // namespace

TEST_CASE("layout/facet: 行优先划分，余数逐格 +1，间距扣掉") {
    const Rect area{0.0, 0.0, 11.0, 7.0};
    FacetOptions options;
    options.rows = 2;
    options.cols = 3;
    options.gap_x = 1.0;
    options.gap_y = 1.0;

    const std::vector<Rect> cells = split_rect(area, options);
    REQUIRE(cells.size() == 6);

    // 宽：(11 - 2 个间距) / 3 = 3；高：(7 - 1) / 2 = 3
    CHECK(cells[0].w == doctest::Approx(3.0));
    CHECK(cells[0].h == doctest::Approx(3.0));
    CHECK(cells[0].x == doctest::Approx(0.0));
    CHECK(cells[0].y == doctest::Approx(0.0));

    // 行优先：第 2 列在 x = 0 + 3 + 1；第 2 行在 y = 0 + 3 + 1
    CHECK(cells[1].x == doctest::Approx(4.0));
    CHECK(cells[3].y == doctest::Approx(4.0));
    CHECK(same_rect(facet_cell(area, options, 1, 2), cells[5]));

    // 余数分配：宽 10、3 列、无间距 → 4/3/3
    FacetOptions tight;
    tight.rows = 1;
    tight.cols = 3;
    tight.gap_x = 0.0;
    const std::vector<Rect> widths = split_rect(Rect{0.0, 0.0, 10.0, 4.0}, tight);
    REQUIRE(widths.size() == 3);
    CHECK(widths[0].w == doctest::Approx(4.0));
    CHECK(widths[1].w == doctest::Approx(3.0));
    CHECK(widths[2].w == doctest::Approx(3.0));
}

TEST_CASE("layout/facet: 放不下的格子是空矩形，越界索引也返回空") {
    FacetOptions options;
    options.rows = 1;
    options.cols = 3;
    options.gap_x = 1.0;
    // 可用宽度 2 - 2 = 0：三格都放不下
    const std::vector<Rect> cells = split_rect(Rect{0.0, 0.0, 2.0, 4.0}, options);
    REQUIRE(cells.size() == 3);
    for (const Rect& cell : cells) {
        CHECK(cell.w == doctest::Approx(0.0));
    }

    const FacetOptions single;
    CHECK(facet_cell(Rect{0.0, 0.0, 4.0, 4.0}, single, 1, 0).w == doctest::Approx(0.0));
    CHECK(split_rect(Rect{0.0, 0.0, 4.0, 4.0}, FacetOptions{0, 2, 0.0, 0.0}).empty());
}

TEST_CASE("layout/auto_margin: 由标签宽度反推四边留白") {
    MarginInputs inputs;
    inputs.y_tick_label_width = 4;
    inputs.y_tick_label_gap = 1;
    inputs.x_tick_label_height = 1;
    inputs.x_tick_label_gap = 1;
    inputs.title_lines = 1;
    inputs.padding = 1.0;

    const Margin margin = compute_margins(inputs);
    CHECK(margin.left == doctest::Approx(6.0));    // 1 + 4 + 1
    CHECK(margin.bottom == doctest::Approx(3.0));  // 1 + 1 + 1
    CHECK(margin.top == doctest::Approx(2.0));     // 1 + 1
    CHECK(margin.right == doctest::Approx(1.0));   // 只有 padding
}

TEST_CASE("layout/auto_margin: 右侧取\"最后一个 x 标签的一半\"与图例的较大者") {
    MarginInputs inputs;
    inputs.x_tick_label_width = 6;  // 一半 = 3
    CHECK(compute_margins(inputs).right == doctest::Approx(4.0));  // 1 + 3

    inputs.legend_width = 8;
    inputs.legend_gap = 1;
    CHECK(compute_margins(inputs).right == doctest::Approx(10.0));  // 1 + 9（图例更大）
}

TEST_CASE("layout/auto_margin: 从标签文本算，宽字符按 2 列") {
    const Margin margin = compute_margins_for({"0", "1000", "-500"}, {"\u7532", "\u4e59"});
    CHECK(margin.left == doctest::Approx(6.0));    // "1000" = 4 列 → 1 + 4 + 1
    CHECK(margin.right == doctest::Approx(2.0));   // 宽字符 2 列 → 一半 1 → 1 + 1
    CHECK(margin.bottom == doctest::Approx(3.0));  // 标签行 1 + 空隙 1 + padding 1
    CHECK(margin.top == doctest::Approx(1.0));     // 只有 padding

    // 没有 x 标签时不预留底部标签行
    const Margin bare = compute_margins_for({}, {});
    CHECK(bare.bottom == doctest::Approx(1.0));
}

TEST_CASE("layout/label_avoid: 候选顺序右→左→上→下") {
    const LabelBox box{Point{5.0, 5.0}, 3, 1};

    const LabelSlot right = place_label_avoiding(box, {});
    REQUIRE(right.placed);
    CHECK(right.rect.x == doctest::Approx(6.0));  // 锚点右侧 gap = 1
    CHECK(right.rect.y == doctest::Approx(5.0));

    const std::vector<Rect> blocked_right{Rect{6.0, 5.0, 3.0, 1.0}};
    const LabelSlot left = place_label_avoiding(box, blocked_right);
    REQUIRE(left.placed);
    CHECK(left.rect.x == doctest::Approx(1.0));  // 5 - 1 - 3

    std::vector<Rect> blocked{blocked_right};
    blocked.push_back(left.rect);
    const LabelSlot up = place_label_avoiding(box, blocked);
    REQUIRE(up.placed);
    CHECK(up.rect.y == doctest::Approx(3.0));  // 5 - 1 - 1

    blocked.push_back(up.rect);
    const LabelSlot down = place_label_avoiding(box, blocked);
    REQUIRE(down.placed);
    CHECK(down.rect.y == doctest::Approx(6.0));

    blocked.push_back(down.rect);
    const LabelSlot none = place_label_avoiding(box, blocked);
    CHECK_FALSE(none.placed);  // 四处都被占：调用方应放弃绘制
}

TEST_CASE("layout/label_avoid: 越出左/上边界不算可用") {
    const LabelBox box{Point{0.0, 0.0}, 2, 1};
    const std::vector<Rect> blocked{Rect{1.0, 0.0, 2.0, 1.0}};  // 挡住右侧

    const LabelSlot slot = place_label_avoiding(box, blocked);
    REQUIRE(slot.placed);
    CHECK(slot.rect.y == doctest::Approx(1.0));  // 左/上都越界，只能落到下方
    CHECK(slot.rect.x == doctest::Approx(0.0));
}

TEST_CASE("layout/label_avoid: bounds 限制与批量放置互不重叠") {
    LabelAvoidOptions options;
    options.bounds = Rect{0.0, 0.0, 4.0, 2.0};
    const LabelSlot slot = place_label_avoiding(LabelBox{Point{3.0, 1.0}, 2, 1}, {}, options);
    REQUIRE(slot.placed);
    CHECK(slot.rect.x == doctest::Approx(0.0));  // 右侧越出 bounds，退到左侧

    const std::vector<LabelBox> labels{{Point{0.0, 0.0}, 2, 1},
                                       {Point{0.0, 0.0}, 2, 1},
                                       {Point{0.0, 0.0}, 2, 1}};
    const std::vector<LabelSlot> slots = place_labels(labels, {});
    REQUIRE(slots.size() == 3);
    CHECK(slots[0].placed);
    CHECK(slots[1].placed);
    CHECK_FALSE(slots[2].placed);  // 三处都用完：第三个放弃

    if (slots[0].placed && slots[1].placed) {
        CHECK_FALSE(overlaps(slots[0].rect, slots[1].rect));
    }
}

TEST_CASE("layout/annotation: 参考线跨整个绘图区，域外不画") {
    canvas::Canvas canvas(20, 11);
    const PlotArea area(20.0, 11.0, Margin{1.0, 1.0, 3.0, 6.0}, 0.0, 10.0, 0.0, 10.0);

    ReferenceLine line;
    line.orientation = Orientation::Horizontal;
    line.value = 5.0;
    REQUIRE(draw_reference_line(canvas, area, line));

    // 该行上每一列（绘图区内）都是参考线字符
    const Rect inner = area.inner_rect();
    const double row = area.y_scale().forward(5.0);
    bool all_line = true;
    for (double x = inner.x; x < inner.right(); x += 1.0) {
        if (canvas.at(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(row)).cp !=
            line.cp) {
            all_line = false;
        }
    }
    CHECK(all_line);

    line.value = 1000.0;  // 远在数据域之外
    CHECK_FALSE(draw_reference_line(canvas, area, line));

    ReferenceLine vertical;
    vertical.orientation = Orientation::Vertical;
    vertical.value = 2.0;
    CHECK(draw_reference_line(canvas, area, vertical));
}

TEST_CASE("layout/annotation: 区域高亮只改背景色，保留原有字符") {
    canvas::Canvas canvas(12, 8);
    const PlotArea area(12.0, 8.0, Margin{1.0, 1.0, 1.0, 1.0}, 0.0, 10.0, 0.0, 10.0);
    canvas.put(4, 3, U'x');  // 数据内容

    HighlightBand band;
    band.orientation = Orientation::Vertical;
    band.from = 2.0;
    band.to = 4.0;
    band.color = Color::ansi16(4);
    const std::size_t touched = draw_highlight_band(canvas, area, band);

    CHECK(touched > 0);
    CHECK(canvas.at(4, 3).cp == U'x');                       // 字符没被涂掉
    CHECK(canvas.at(4, 3).style.bg() == Color::ansi16(4));   // 背景被改成高亮色

    // fill_char 非 0 时才填字符
    HighlightBand filled;
    filled.orientation = Orientation::Horizontal;
    filled.from = 0.0;
    filled.to = 10.0;
    filled.fill_char = U'#';
    CHECK(draw_highlight_band(canvas, area, filled) > 0);
    CHECK(canvas.at(4, 3).cp == U'#');
}

TEST_CASE("layout/annotation: 标注放不下就不画") {
    canvas::Canvas roomy(30, 10);
    const PlotArea area(30.0, 10.0, Margin{1.0, 1.0, 1.0, 1.0}, 0.0, 10.0, 0.0, 10.0);
    Annotation annotation;
    annotation.at = Point{5.0, 5.0};
    annotation.text = "peak";
    CHECK(draw_annotation(roomy, area, annotation));

    // 绘图区只有 2 列宽，放不下 8 列的标注：返回 false 且画布保持空白
    canvas::Canvas cramped(8, 6);
    const PlotArea tiny(8.0, 6.0, Margin{1.0, 1.0, 1.0, 6.0}, 0.0, 10.0, 0.0, 10.0);
    Annotation long_text;
    long_text.at = Point{5.0, 5.0};
    long_text.text = "verylonglabel";
    CHECK_FALSE(draw_annotation(cramped, tiny, long_text));

    std::size_t nonblank = 0;
    for (std::uint32_t y = 0; y < cramped.height(); ++y) {
        for (std::uint32_t x = 0; x < cramped.width(); ++x) {
            if (cramped.at(x, y).cp != U' ') {
                ++nonblank;
            }
        }
    }
    CHECK(nonblank == 0);
}
