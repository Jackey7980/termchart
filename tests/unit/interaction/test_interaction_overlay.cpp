// test_interaction_overlay.cpp —— 悬停覆盖层：crosshair（十字准线）与 tooltip（浮层）
//
// 覆盖（对应任务的 6 条不变量）：
//   1. clear() 精确还原：铺数据字符（含宽字符）→ draw → 画布有变化 → clear →
//      画布逐格逐属性等于绘制前（"悬浮一次不该永久污染画布"）。
//   2. 不越绘图区：准线端点格恰好落在区内（半开区间），区外格未被改写。
//   3. 不毁数据：准线穿过数据字符格时，该格**保留原字符**（默认口径只改样式）。
//   4. tooltip 翻边：靠近右边界 → 出现在左侧；靠近上边界 → 出现在下方；永不覆盖锚点。
//   5. 宽字符 / 超长文本：CJK 按 2 列计宽、浮层不越界；超长行被截断并带省略号。
//   6. 退化：空文本 / 锚点在画布外 / 绘图区 1 格 → 不画且不崩。
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/interaction/crosshair.hpp"
#include "termchart/interaction/tooltip.hpp"

namespace {

using namespace termchart;
using namespace termchart::canvas;
using namespace termchart::interaction;

bool same_cell(const Cell& a, const Cell& b) noexcept {
    return a.cp == b.cp && a.style == b.style;
}

// 在整幅画布一角铺一点带样式的数据，制造"原始内容"。
void prime(Canvas& canvas) {
    canvas.draw_text(Point{1, 1}, "A日B", Style{}.set_fg(Color::ansi16(2)));
    canvas.draw_text(Point{2, 3}, "xy", Style{}.set_fg(Color::ansi16(4)));
}

}  // namespace

TEST_CASE("interaction/crosshair: clear() 精确还原（含宽字符数据）") {
    Canvas canvas(20, 8);
    prime(canvas);
    const CellBuffer before = canvas.to_buffer();

    CrosshairOptions options;
    options.plot_rect = Rect{0, 0, 20, 8};
    Crosshair crosshair(options);

    CHECK_FALSE(crosshair.active());
    REQUIRE(crosshair.draw(canvas, Point{5, 4}));
    CHECK(crosshair.active());
    CHECK(crosshair.patch_count() > 0);
    CHECK(canvas.to_buffer() != before);  // 确实改了画布

    CHECK(crosshair.clear(canvas));
    CHECK(canvas.to_buffer() == before);  // 逐格逐属性还原
    CHECK_FALSE(crosshair.active());

    // 已还原后再 clear：no-op。
    CHECK_FALSE(crosshair.clear(canvas));
    CHECK(canvas.to_buffer() == before);
}

TEST_CASE("interaction/crosshair: 重复 draw 幂等，不累积污染") {
    Canvas canvas(20, 8);
    prime(canvas);
    const CellBuffer before = canvas.to_buffer();

    CrosshairOptions options;
    options.plot_rect = Rect{0, 0, 20, 8};
    Crosshair crosshair(options);

    REQUIRE(crosshair.draw(canvas, Point{3, 3}));
    const std::size_t first = crosshair.patch_count();
    REQUIRE(crosshair.draw(canvas, Point{9, 6}));  // 换点重绘：先还原旧、再画新
    CHECK(crosshair.patch_count() == first);       // 覆盖格数不随次数增长

    CHECK(crosshair.clear(canvas));
    CHECK(canvas.to_buffer() == before);
}

TEST_CASE("interaction/crosshair: 准线严格裁剪在绘图区内（半开区间）") {
    Canvas canvas(20, 10);
    const CellBuffer before = canvas.to_buffer();

    CrosshairOptions options;
    options.plot_rect = Rect{2, 1, 10, 6};  // x∈[2,12) y∈[1,7)
    Crosshair crosshair(options);
    REQUIRE(crosshair.draw(canvas, Point{6, 3}));

    // 水平线所在行 = 3：端点格恰好是 x=2 与 x=11；x=1 / x=12 属区外。
    CHECK_FALSE(same_cell(canvas.at(2, 3), before.at(2, 3)));
    CHECK_FALSE(same_cell(canvas.at(11, 3), before.at(11, 3)));
    CHECK(same_cell(canvas.at(1, 3), before.at(1, 3)));
    CHECK(same_cell(canvas.at(12, 3), before.at(12, 3)));

    // 垂直线所在列 = 6：端点格 y=1 与 y=6；y=0 / y=7 属区外。
    CHECK_FALSE(same_cell(canvas.at(6, 1), before.at(6, 1)));
    CHECK_FALSE(same_cell(canvas.at(6, 6), before.at(6, 6)));
    CHECK(same_cell(canvas.at(6, 0), before.at(6, 0)));
    CHECK(same_cell(canvas.at(6, 7), before.at(6, 7)));
}

TEST_CASE("interaction/crosshair: 默认口径保留数据字符，只改样式") {
    Canvas canvas(21, 8);
    // 在准线将经过的格子放数据字符（含宽字符与续接格）。
    canvas.put(10, 4, U'Q', Style{}.set_fg(Color::ansi16(3)));
    canvas.draw_text(Point{12, 4}, "日", Style{}.set_fg(Color::ansi16(3)));

    CrosshairOptions options;
    options.plot_rect = Rect{0, 0, 21, 8};
    Crosshair crosshair(options);
    REQUIRE(crosshair.draw(canvas, Point{5, 4}));

    // 横线 row=4 穿过这些数据格：cp 必须一字不动。
    CHECK(canvas.at(10, 4).cp == U'Q');
    CHECK(canvas.at(12, 4).cp == U'日');
    CHECK(canvas.at(13, 4).cp == kContinuationCp);  // 宽字符右半格仍是续接格
    // 样式确实被叠加（背景换成 line_style 的暗底，前景保持数据原色）。
    CHECK(canvas.at(10, 4).style.bg() == Color::ansi16(8));
    CHECK(canvas.at(10, 4).style.fg() == Color::ansi16(3));
    CHECK(canvas.at(12, 4).style.bg() == Color::ansi16(8));

    CHECK(crosshair.clear(canvas));
    CHECK(canvas.at(10, 4).cp == U'Q');
    CHECK(canvas.at(10, 4).style == Style{}.set_fg(Color::ansi16(3)));
}

TEST_CASE("interaction/crosshair: 覆盖口径可切换为连接字符") {
    Canvas canvas(12, 6);
    CrosshairOptions options;
    options.plot_rect = Rect{0, 0, 12, 6};
    options.preserve_data = false;
    Crosshair crosshair(options);
    REQUIRE(crosshair.draw(canvas, Point{4, 3}));

    CHECK(canvas.at(4, 3).cp == U'┼');  // 交叉点
    CHECK(canvas.at(0, 3).cp == U'─');  // 横线
    CHECK(canvas.at(4, 0).cp == U'│');  // 竖线
}

TEST_CASE("interaction/crosshair: 轴侧读数（show_readout）写在绘图区下方") {
    Canvas canvas(24, 8);
    const CellBuffer before = canvas.to_buffer();

    CrosshairOptions options;
    options.plot_rect = Rect{1, 1, 12, 5};  // 下边框下一行 = y=6
    options.show_readout = true;
    Crosshair crosshair(options);
    REQUIRE(crosshair.draw(canvas, Point{4, 3}, "x=4 y=3"));

    // 读数落在 y=6（绘图区外），首格是 'x'；绘图区内每行未被越写。
    CHECK(canvas.at(4, 6).cp == U'x');
    CHECK(same_cell(canvas.at(0, 1), before.at(0, 1)));   // 绘图区左侧之外
    CHECK(same_cell(canvas.at(14, 1), before.at(14, 1)));  // 绘图区右侧之外

    CHECK(crosshair.clear(canvas));
    CHECK(canvas.to_buffer() == before);
}

TEST_CASE("interaction/tooltip: 锚点靠近右边界 → 浮层翻到左侧；靠近上边界 → 翻到下方") {
    Canvas canvas(30, 12);
    TooltipOptions options;
    options.bounds = Rect{0, 0, 30, 12};
    Tooltip tooltip(options);

    // 靠近右边界：应出现在锚点左侧。
    const TooltipPlacement right_side = tooltip.draw(canvas, Point{28, 6}, "hi");
    REQUIRE(right_side.placed);
    CHECK(right_side.rect.right() <= 28.0);  // 完全在锚点左侧
    CHECK_FALSE(right_side.rect.contains(Point{28, 6}));
    CHECK(tooltip.clear(canvas));

    // 靠近上边界：右上/左上装不下 → 翻到下方。
    const TooltipPlacement top_side = tooltip.draw(canvas, Point{5, 0}, "hi");
    REQUIRE(top_side.placed);
    CHECK(top_side.rect.y > 0.0);  // 完全在锚点下方
    CHECK_FALSE(top_side.rect.contains(Point{5, 0}));
    CHECK(tooltip.clear(canvas));
}

TEST_CASE("interaction/tooltip: 永不覆盖锚点（多锚点扫描）") {
    Canvas canvas(30, 12);
    TooltipOptions options;
    options.bounds = Rect{0, 0, 30, 12};
    Tooltip tooltip(options);

    const Point anchors[] = {{0, 0},   {15, 0},  {29, 0},  {0, 6},  {29, 6},
                             {0, 11},  {15, 11}, {29, 11}, {15, 6}, {27, 10}};
    for (const Point& anchor : anchors) {
        CAPTURE(anchor.x);
        CAPTURE(anchor.y);
        const TooltipPlacement slot = tooltip.draw(canvas, anchor, "abc");
        REQUIRE(slot.placed);
        CHECK_FALSE(slot.rect.contains(anchor));
        CHECK(slot.rect.x >= 0.0);
        CHECK(slot.rect.y >= 0.0);
        CHECK(slot.rect.right() <= 30.0);
        CHECK(slot.rect.bottom() <= 12.0);
        CHECK(tooltip.clear(canvas));
    }
}

TEST_CASE("interaction/tooltip: clear() 精确还原") {
    Canvas canvas(30, 12);
    prime(canvas);
    const CellBuffer before = canvas.to_buffer();

    TooltipOptions options;
    options.bounds = Rect{0, 0, 30, 12};
    Tooltip tooltip(options);
    REQUIRE(tooltip.draw(canvas, Point{10, 6}, "tip").placed);
    CHECK(canvas.to_buffer() != before);
    CHECK(tooltip.clear(canvas));
    CHECK(canvas.to_buffer() == before);
    CHECK_FALSE(tooltip.clear(canvas));
}

TEST_CASE("interaction/tooltip: 宽字符按 2 列计宽，浮层不越界") {
    Canvas canvas(40, 10);
    TooltipOptions options;
    options.bounds = Rect{0, 0, 40, 10};
    Tooltip tooltip(options);

    const std::string cjk = "中文字符";  // 显示宽 = 8
    const TooltipPlacement slot = tooltip.draw(canvas, Point{2, 2}, cjk);
    REQUIRE(slot.placed);
    // 外框 = 内容 8 列 + 左右边框 2 列。
    CHECK(slot.rect.w == 10.0);
    CHECK(slot.rect.right() <= 40.0);
    CHECK(slot.rect.bottom() <= 10.0);

    // 首行内容起始格是 '中'，其右邻是宽字符续接格。
    const auto x0 = static_cast<std::uint32_t>(slot.rect.x);
    const auto y0 = static_cast<std::uint32_t>(slot.rect.y);
    CHECK(canvas.at(x0 + 1, y0 + 1).cp == U'中');
    CHECK(canvas.at(x0 + 2, y0 + 1).cp == kContinuationCp);
    CHECK(canvas.at(x0, y0).cp == U'┌');
    CHECK(canvas.at(x0 + 9, y0).cp == U'┐');
    CHECK(tooltip.clear(canvas));
}

TEST_CASE("interaction/tooltip: 超长文本按显示宽度截断并加省略号") {
    Canvas canvas(40, 10);
    TooltipOptions options;
    options.bounds = Rect{0, 0, 40, 10};
    options.max_width = 6;  // 内容上限 6 列
    Tooltip tooltip(options);

    const std::string long_text = "abcdefghijkl";  // 明显超限
    const TooltipPlacement slot = tooltip.draw(canvas, Point{3, 3}, long_text);
    REQUIRE(slot.placed);
    CHECK(slot.rect.w == 8.0);  // 6 + 左右边框

    // 内区第一行应出现省略号且字符数受控。
    const auto x0 = static_cast<std::uint32_t>(slot.rect.x);
    const auto y0 = static_cast<std::uint32_t>(slot.rect.y);
    bool has_ellipsis = false;
    for (std::uint32_t dx = 1; dx + 1 < static_cast<std::uint32_t>(slot.rect.w); ++dx) {
        if (canvas.at(x0 + dx, y0 + 1).cp == U'…') {
            has_ellipsis = true;
        }
    }
    CHECK(has_ellipsis);
    // 右边框位置正确，没被撑破。
    CHECK(canvas.at(x0 + 7, y0 + 1).cp == U'│');
    CHECK(tooltip.clear(canvas));
}

TEST_CASE("interaction/tooltip: 多行浮层与右边界对齐") {
    Canvas canvas(40, 12);
    TooltipOptions options;
    options.bounds = Rect{0, 0, 40, 12};
    Tooltip tooltip(options);

    const std::vector<std::string> lines{"one", "longer line"};
    const TooltipPlacement slot = tooltip.draw(canvas, Point{3, 3}, lines);
    REQUIRE(slot.placed);
    CHECK(slot.rect.w == 13.0);  // 11 + 2
    CHECK(slot.rect.h == 4.0);   // 2 行 + 上下边框

    const auto x0 = static_cast<std::uint32_t>(slot.rect.x);
    const auto y0 = static_cast<std::uint32_t>(slot.rect.y);
    CHECK(canvas.at(x0, y0).cp == U'┌');
    CHECK(canvas.at(x0, y0 + 3).cp == U'└');
    CHECK(canvas.at(x0, y0 + 1).cp == U'│');
    CHECK(canvas.at(x0 + 1, y0 + 1).cp == U'o');
    // 短行 "one" 之后被补成空格，右边框仍对齐在第 12 列。
    CHECK(canvas.at(x0 + 12, y0 + 1).cp == U'│');
    CHECK(canvas.at(x0 + 12, y0 + 2).cp == U'│');
    CHECK(tooltip.clear(canvas));
}

TEST_CASE("interaction/overlay: 退化输入明确不画且不崩") {
    // (a) 绘图区只有 1 格：十字退化成点，明确不画。
    {
        Canvas canvas(10, 6);
        const CellBuffer before = canvas.to_buffer();
        CrosshairOptions options;
        options.plot_rect = Rect{2, 1, 1, 1};
        Crosshair crosshair(options);
        CHECK_FALSE(crosshair.draw(canvas, Point{2, 1}));
        CHECK(canvas.to_buffer() == before);
    }
    // (b) 锚点在绘图区（画布）之外：不画。
    {
        Canvas canvas(10, 6);
        const CellBuffer before = canvas.to_buffer();
        CrosshairOptions options;
        options.plot_rect = Rect{0, 0, 10, 6};
        Crosshair crosshair(options);
        CHECK_FALSE(crosshair.draw(canvas, Point{99, 99}));
        CHECK(canvas.to_buffer() == before);
    }
    // (c) tooltip 空文本：不画。
    {
        Canvas canvas(20, 8);
        const CellBuffer before = canvas.to_buffer();
        TooltipOptions options;
        options.bounds = Rect{0, 0, 20, 8};
        Tooltip tooltip(options);
        CHECK_FALSE(tooltip.draw(canvas, Point{5, 4}, "").placed);
        CHECK(canvas.to_buffer() == before);
    }
    // (d) tooltip 锚点在画布外：不画。
    {
        Canvas canvas(20, 8);
        const CellBuffer before = canvas.to_buffer();
        TooltipOptions options;
        options.bounds = Rect{0, 0, 20, 8};
        Tooltip tooltip(options);
        CHECK_FALSE(tooltip.draw(canvas, Point{99, 99}, "hi").placed);
        CHECK(canvas.to_buffer() == before);
    }
    // (e) tooltip 允许区域过小、放不下浮层：不画。
    {
        Canvas canvas(20, 8);
        const CellBuffer before = canvas.to_buffer();
        TooltipOptions options;
        options.bounds = Rect{0, 0, 2, 2};  // 连外框都装不下
        Tooltip tooltip(options);
        CHECK_FALSE(tooltip.draw(canvas, Point{1, 1}, "wide text").placed);
        CHECK(canvas.to_buffer() == before);
    }
}
