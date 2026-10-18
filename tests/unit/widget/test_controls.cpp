// test_controls.cpp —— widget 即时控件：下拉框 / 文本框 / 单选组
//
// 咬的是**契约**而非外观：
//   * 意图只由输入决定，与几何正交（退化几何不崩、不改返回值）；
//   * 编辑语义：光标按码点移动、退格/删除删整个码点、控制字符不进文本框；
//   * 单选/下拉的命中查询与绘制窗口同一口径（点哪行确认哪行）；
//   * 禁用态不产意图。
#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/widget/controls.hpp"
#include "termchart/widget/theme_dark.hpp"

namespace {

using namespace termchart;
using termchart::widget::ControlState;
using termchart::widget::DropdownInput;
using termchart::widget::DropdownIntent;
using termchart::widget::DropdownResult;
using termchart::widget::kDarkTheme;
using termchart::widget::kNoSelection;
using termchart::widget::TextBoxInput;
using termchart::widget::TextBoxResult;
using termchart::widget::ThemePalette;
using termchart::widget::dropdown;
using termchart::widget::dropdown_row_at;
using termchart::widget::radio_group_clicked;
using termchart::widget::radio_row_at;
using termchart::widget::textbox;

const ThemePalette kTheme = kDarkTheme;

TEST_CASE("widget/dropdown: 意图——按下展开、键盘移动、回车确认、Esc 取消") {
    canvas::Canvas c(24, 8);
    const std::vector<std::string> items{"alpha", "beta", "gamma"};

    // 折叠态：hovered+pressed → Toggle。
    ControlState s;
    s.hovered = true;
    s.pressed = true;
    const DropdownResult open =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, false, 0, kTheme, s);
    CHECK(open.intent == DropdownIntent::Toggle);

    // 展开态：无输入 → None（没有输入就没有意图）。
    s.pressed = false;
    const DropdownResult idle =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, true, 0, kTheme, s, DropdownInput{});
    CHECK(idle.intent == DropdownIntent::None);

    DropdownInput input;
    input.move_delta = 1;
    const DropdownResult moved =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, true, 0, kTheme, s, input);
    CHECK(moved.intent == DropdownIntent::MoveDown);
    CHECK(moved.index == 1);

    input = DropdownInput{};
    input.move_delta = -1;
    const DropdownResult up =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, true, 0, kTheme, s, input);
    CHECK(up.intent == DropdownIntent::MoveUp);
    CHECK(up.index == 0);  // 已在顶部：夹住

    input = DropdownInput{};
    input.commit = true;
    const DropdownResult commit =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, true, 2, kTheme, s, input);
    CHECK(commit.intent == DropdownIntent::Commit);
    CHECK(commit.index == 2);

    input = DropdownInput{};
    input.cancel = true;
    const DropdownResult cancel =
        dropdown(c, Rect{0, 0, 12, 4}, items, 0, true, 2, kTheme, s, input);
    CHECK(cancel.intent == DropdownIntent::Cancel);
}

TEST_CASE("widget/dropdown: 点击行确认（窗口映射与绘制同口径）") {
    canvas::Canvas c(24, 8);
    const std::vector<std::string> items{"a", "b", "c", "d", "e"};
    ControlState s;
    s.hovered = true;
    s.pressed = true;

    // rect.h = 3 → 可视 2 行；active=4 时窗口首项 = 4 - 1 = 3。
    DropdownInput click;
    click.has_click = true;
    click.click_y = 1;  // 第 1 行 = 窗口首项
    click.click_x = 2;
    const DropdownResult r1 =
        dropdown(c, Rect{0, 0, 12, 3}, items, 0, true, 4, kTheme, s, click);
    CHECK(r1.intent == DropdownIntent::Commit);
    CHECK(r1.index == 3);  // 第 1 可视行对应绝对下标 3

    click.click_y = 2;  // 第 2 行 = 窗口第二项
    const DropdownResult r2 =
        dropdown(c, Rect{0, 0, 12, 3}, items, 0, true, 4, kTheme, s, click);
    CHECK(r2.intent == DropdownIntent::Commit);
    CHECK(r2.index == 4);

    // 点到折叠行（row=0）：收起。
    click.click_y = 0;
    const DropdownResult r3 =
        dropdown(c, Rect{0, 0, 12, 3}, items, 0, true, 4, kTheme, s, click);
    CHECK(r3.intent == DropdownIntent::Toggle);
}

TEST_CASE("widget/dropdown: 禁用/空表/退化几何") {
    canvas::Canvas c(24, 8);
    ControlState off;  // enabled = false
    off.enabled = false;
    off.hovered = true;
    off.pressed = true;
    const DropdownResult disabled =
        dropdown(c, Rect{0, 0, 12, 4}, std::vector<std::string>{"x"}, 0, false, 0,
                 kTheme, off);
    CHECK(disabled.intent == DropdownIntent::None);  // 禁用：不产意图

    const DropdownResult empty =
        dropdown(c, Rect{0, 0, 12, 4}, {}, 0, false, 0, kTheme, off);
    CHECK(empty.intent == DropdownIntent::None);

    // 退化几何：不绘制、不崩；意图仍按输入给（与几何正交的文件头契约）。
    ControlState on;
    on.hovered = true;
    on.pressed = true;
    const DropdownResult degenerate =
        dropdown(c, Rect{}, std::vector<std::string>{"x"}, 0, false, 0, kTheme, on);
    CHECK(degenerate.intent == DropdownIntent::Toggle);
}

TEST_CASE("widget/textbox: 插入/退格/删除/光标移动按码点") {
    canvas::Canvas c(24, 8);
    std::string text = "ab";
    TextBoxInput in;
    in.caret = 2;
    in.inserted = "\u4e2d";  // "中"：3 字节 1 码点
    const TextBoxResult r1 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, in);
    CHECK(r1.changed);
    CHECK(text == "ab\u4e2d");
    CHECK(r1.caret == 5);  // 'a','b',3 字节

    // 退格删除整个码点（不是 1 字节）。
    TextBoxInput back;
    back.caret = 5;
    back.erase_back = 1;
    const TextBoxResult r2 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, back);
    CHECK(r2.changed);
    CHECK(text == "ab");
    CHECK(r2.caret == 2);

    // Delete 删光标处码点。
    TextBoxInput fwd;
    fwd.caret = 0;
    fwd.erase_forward = 1;
    const TextBoxResult r3 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, fwd);
    CHECK(r3.changed);
    CHECK(text == "b");

    // 中间插入。
    TextBoxInput ins;
    ins.caret = 0;
    ins.inserted = "X";
    const TextBoxResult r4 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, ins);
    CHECK(text == "Xb");
    CHECK(r4.caret == 1);

    // 控制字符被逐元素丢弃（BEL 与 ESC 进不去，但其后可打印字符不受牵连）。
    TextBoxInput ctrl;
    ctrl.caret = 2;
    ctrl.inserted = std::string_view{"\x07\x1b"};
    const TextBoxResult r5 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, ctrl);
    CHECK_FALSE(r5.changed);
    CHECK(text == "Xb");
}

TEST_CASE("widget/textbox: 回车上报 submitted，禁用态不编辑") {
    canvas::Canvas c(24, 8);
    std::string text = "hi";
    TextBoxInput in;
    in.caret = 2;
    in.submit = true;
    const TextBoxResult r = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, ControlState{}, in);
    CHECK(r.submitted);
    CHECK_FALSE(r.changed);
    CHECK(text == "hi");

    ControlState off;
    off.enabled = false;
    TextBoxInput edit;
    edit.caret = 2;
    edit.inserted = "!";
    const TextBoxResult r2 = textbox(c, Rect{0, 0, 20, 1}, text, kTheme, off, edit);
    CHECK_FALSE(r2.changed);
    CHECK(text == "hi");  // 禁用态文本不动
}

TEST_CASE("widget/radio: 命中查询、点击返回行号、禁用不点") {
    canvas::Canvas c(24, 8);
    const std::vector<std::string> items{"one", "two", "three"};
    const Rect r{2, 1, 14, 3};

    CHECK(radio_row_at(r, 3, Point{3.0, 1.0}) == 0);
    CHECK(radio_row_at(r, 3, Point{3.0, 3.0}) == 2);
    CHECK(radio_row_at(r, 3, Point{3.0, 4.0}) == kNoSelection);    // 行越界
    CHECK(radio_row_at(r, 3, Point{100.0, 1.0}) == kNoSelection);  // 列越界
    CHECK(radio_row_at(r, 3, Point{-1.0, 1.0}) == kNoSelection);   // 负坐标

    const std::size_t clicked = radio_group_clicked(c, r, items, 1,
                                                    /*hovered_row=*/2, /*pressed=*/true,
                                                    /*enabled=*/true, /*focused=*/false,
                                                    kTheme);
    CHECK(clicked == 2);

    const std::size_t none =
        radio_group_clicked(c, r, items, 1, kNoSelection, true, true, false, kTheme);
    CHECK(none == kNoSelection);  // 没有 hover 行：不产点击

    const std::size_t disabled =
        radio_group_clicked(c, r, items, 1, 2, true, false, false, kTheme);
    CHECK(disabled == kNoSelection);
}

}  // namespace
