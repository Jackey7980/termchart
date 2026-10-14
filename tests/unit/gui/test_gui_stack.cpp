// test_gui_stack.cpp —— termchart_gui 平行栈的单元测试
//
// 覆盖 layout / widget_tree / render::style_map / backend_tk（无头）/ native_loop。
// 复用 termchart 的 scene/color/image，不碰终端栈（gui_stack.md）。
#include <doctest.h>

#include <memory>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart_gui/gui.hpp"

using namespace termchart_gui;
using namespace termchart;

TEST_CASE("gui/layout/flex: column 自然尺寸 = 主轴求和 / 交叉取最大") {
    FlexLayout fl;
    fl.direction = FlexDirection::Column;
    std::vector<LayoutItem> items(2);
    items[0].content = Size{10, 20};  // w=10, h=20
    items[1].content = Size{10, 20};
    const Size m = fl.measure(Rect{0, 0, 100, 100}, items);
    CHECK(m.w == doctest::Approx(10));   // 交叉取最大宽
    CHECK(m.h == doctest::Approx(40));   // 主轴高求和
}

TEST_CASE("gui/widget_tree: relayout 全局坐标 + 命中测试") {
    Container root("root");
    root.set_bounds(Rect{10, 10, 100, 100});
    Widget& a = root.emplace_child<Widget>("a");
    a.set_size(20, 20);
    Widget& b = root.emplace_child<Widget>("b");
    b.set_size(20, 20);
    // 默认纵向 flex（gap=0，cross_align=Stretch）：两个 Fill 子项在 main 轴均分 100 高，
    // 交叉轴拉伸到容器宽 100 → 各 50 高、上下排布。
    root.relayout();
    CHECK(a.global_bounds() == Rect{10, 10, 100, 50});
    CHECK(b.global_bounds() == Rect{10, 60, 100, 50});
    CHECK(root.hit_test(Point{15, 15}) == true);
    CHECK(root.hit_test(Point{500, 500}) == false);
}

TEST_CASE("gui/widget_tree: grid 重排等分") {
    Container root("root");
    root.set_bounds(Rect{10, 10, 100, 100});
    auto grid = std::make_unique<GridLayout>();
    grid->columns = 2;
    root.set_layout(std::move(grid));
    Widget& a = root.emplace_child<Widget>("a");
    a.set_size(10, 10);
    Widget& b = root.emplace_child<Widget>("b");
    b.set_size(10, 10);
    Widget& c = root.emplace_child<Widget>("c");
    c.set_size(10, 10);
    Widget& d = root.emplace_child<Widget>("d");
    d.set_size(10, 10);
    root.relayout();
    // cell 50×50，行优先。
    CHECK(a.global_bounds() == Rect{10, 10, 50, 50});
    CHECK(b.global_bounds() == Rect{60, 10, 50, 50});
    CHECK(c.global_bounds() == Rect{10, 60, 50, 50});
    CHECK(d.global_bounds() == Rect{60, 60, 50, 50});
}

TEST_CASE("gui/style_map: core::Style → GuiStyle，颜色量化一致") {
    Style s;
    s.set_fg(Color::rgb(255, 0, 0));
    s.set_attr(Attr::Bold);
    const GuiStyle g = gui_style_from(s, ColorDepth::TrueColor);
    CHECK(g.fg == Rgb{255, 0, 0});
    CHECK(g.bg_transparent == true);  // 未设背景色 → 与下层混合
    CHECK(g.bold == true);
    CHECK(g.underline == false);
}

TEST_CASE("gui/backend: 无头 BitmapCanvas 绘制 + 快照") {
    HeadlessBackend backend(40, 20);
    backend.canvas().clear(Rgb{0, 0, 0});
    backend.canvas().fill_rect(Rect{0, 0, 10, 10}, Rgb{255, 0, 0});
    const Bitmap bmp = backend.canvas().snapshot();
    CHECK(bmp.width() == 40);
    CHECK(bmp.height() == 20);
    CHECK(bmp.at(0, 0, 0) == 255);
    CHECK(bmp.at(15, 15, 0) == 0);  // 矩形外保持背景
}

TEST_CASE("gui/native_loop: TimerQueue 到期周期触发") {
    TimerQueue q;
    int fired = 0;
    [[maybe_unused]] auto handle = q.add(0.1, [&] { ++fired; });
    q.fire_elapsed(0.05);
    CHECK(fired == 0);
    q.fire_elapsed(0.15);
    CHECK(fired == 1);
    q.fire_elapsed(0.25);
    CHECK(fired == 2);  // 周期定时器再次触发
}

TEST_CASE("gui/native_loop: HeadlessEventLoop post/poll") {
    HeadlessEventLoop loop;
    loop.post(tk_event_map::mouse_press(5.0, 5.0));
    Event e;
    REQUIRE(loop.poll(e));
    CHECK(e.type == EventType::MousePress);
    CHECK(e.x == 5.0);
    CHECK_FALSE(loop.poll(e));
}

TEST_CASE("gui/widget_tree: reconcile 按 id 差异（新增）") {
    Container old_t("root");
    old_t.emplace_child<Widget>("a").set_size(10, 10);
    Container new_t("root");
    new_t.emplace_child<Widget>("a").set_size(10, 10);
    new_t.emplace_child<Widget>("b").set_size(10, 10);

    const auto ops = reconcile(old_t, new_t);
    bool has_add_b = false;
    bool has_remove = false;
    for (const auto& op : ops) {
        if (op.kind == ReconcileKind::Add && op.id == "b") {
            has_add_b = true;
        }
        if (op.kind == ReconcileKind::Remove) {
            has_remove = true;
        }
    }
    CHECK(has_add_b);
    CHECK_FALSE(has_remove);
}
