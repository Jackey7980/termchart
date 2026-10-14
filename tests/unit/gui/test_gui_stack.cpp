// test_gui_stack.cpp —— termchart_gui 平行栈的单元测试
//
// 覆盖 layout / widget_tree / render::style_map / backend_tk（无头）/ native_loop。
// 复用 termchart 的 scene/color/image，不碰终端栈（gui_stack.md）。
#include <doctest.h>

#include <cstdint>
#include <memory>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/backend_tk/tui_backend.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/slider.hpp"

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

TEST_CASE("gui/widget: Button 命中测试 + 点击回调 + 按下态") {
    Button btn("btn", "OK");
    btn.set_bounds(Rect{10, 10, 80, 20});
    int clicks = 0;
    btn.set_on_click([&] { ++clicks; });

    Event press{};
    press.type = EventType::MousePress;
    press.x = 50.0;
    press.y = 20.0;
    CHECK(btn.on_event(press) == true);
    CHECK(clicks == 1);
    CHECK(btn.pressed() == true);

    Event miss{};
    miss.type = EventType::MousePress;
    miss.x = 300.0;
    miss.y = 300.0;
    CHECK(btn.on_event(miss) == false);
    CHECK(clicks == 1);

    Event rel{};
    rel.type = EventType::MouseRelease;
    CHECK(btn.on_event(rel) == true);
    CHECK(btn.pressed() == false);

    // 未捕获按下时释放不算消费。
    CHECK(btn.on_event(rel) == false);
}

TEST_CASE("gui/widget: Slider 拖拽归一化取值（含钳制）") {
    Slider s("s");
    s.set_bounds(Rect{0, 0, 100, 20});
    double last = -1.0;
    s.set_on_change([&](double v) { last = v; });

    Event press{};
    press.type = EventType::MousePress;
    press.x = 25.0;
    press.y = 10.0;
    CHECK(s.on_event(press) == true);
    CHECK(s.value() == doctest::Approx(0.25));
    CHECK(s.dragging() == true);

    Event move{};
    move.type = EventType::MouseMove;
    move.x = 75.0;
    move.y = 10.0;
    CHECK(s.on_event(move) == true);
    CHECK(s.value() == doctest::Approx(0.75));
    CHECK(last == doctest::Approx(0.75));

    Event beyond{};  // 越界钳制到 1.0
    beyond.type = EventType::MouseMove;
    beyond.x = 300.0;
    beyond.y = 10.0;
    CHECK(s.on_event(beyond) == true);
    CHECK(s.value() == doctest::Approx(1.0));

    Event rel{};
    rel.type = EventType::MouseRelease;
    CHECK(s.on_event(rel) == true);
    CHECK(s.dragging() == false);

    // 非拖拽态的 MouseMove 不消费。
    Event idle{};
    idle.type = EventType::MouseMove;
    idle.x = 10.0;
    idle.y = 10.0;
    CHECK(s.on_event(idle) == false);
}

TEST_CASE("gui/widget: Label draw_text 经点阵字体栅格化出真像素") {
    HeadlessBackend backend(120, 40);
    backend.canvas().clear(Rgb{0, 0, 0});
    Label lbl("l", "Hi", Rgb{255, 255, 255}, Rgb{0, 0, 0});
    lbl.set_bounds(Rect{0, 0, 60, 20});
    lbl.paint(backend.canvas());

    const Bitmap bmp = backend.canvas().snapshot();
    int lit = 0;
    for (std::uint32_t y = 0; y < bmp.height(); ++y) {
        for (std::uint32_t x = 0; x < bmp.width(); ++x) {
            if (bmp.at(x, y, 0) > 0) {
                ++lit;
            }
        }
    }
    CHECK(lit > 0);  // "Hi" 的白色点阵字形落到位图
}

TEST_CASE("gui/backend: TUI halfblock 转换含真彩 SGR 与上半块字形") {
    image::Bitmap bmp(2, 2, 4);
    bmp.fill_rect(0, 0, 2, 1, 255, 0, 0, 255);  // 上一行：红
    bmp.fill_rect(0, 1, 2, 1, 0, 0, 255, 255);  // 下一行：蓝
    const std::string s = termchart_gui::bitmap_to_halfblock(bmp, ColorDepth::TrueColor);
    CHECK(s.find("38;2;255;0;0") != std::string::npos);   // 前景=上一行
    CHECK(s.find("48;2;0;0;255") != std::string::npos);   // 背景=下一行
    CHECK(s.find("\xe2\x96\x80") != std::string::npos);   // ▀ 上半块
    CHECK(s.find("\x1b[0m\n") != std::string::npos);      // 行尾 reset + 换行
}

TEST_CASE("gui/widget: Button/Slider paint 落真彩色像素") {
    HeadlessBackend backend(120, 60);
    backend.canvas().clear(Rgb{0, 0, 0});

    Button btn("b", "x");
    btn.set_bounds(Rect{0, 0, 60, 20});  // 未按下：base = {50,50,58}
    btn.paint(backend.canvas());

    Slider sl("s");
    sl.set_bounds(Rect{0, 30, 100, 20});  // value=0 → 拇指在左端
    sl.set_value(0.0);
    sl.paint(backend.canvas());

    const Bitmap bmp = backend.canvas().snapshot();
    // 按钮内部（避 1px 描边）为按钮底色。
    CHECK(bmp.at(30, 10, 0) == 50);
    CHECK(bmp.at(30, 10, 1) == 50);
    CHECK(bmp.at(30, 10, 2) == 58);
    // 滑块拇指（thumb_w=10, thumb_h=16, 竖向居中于 y≈40）为拇指色。
    CHECK(bmp.at(4, 40, 0) == 120);
    CHECK(bmp.at(4, 40, 1) == 184);
    CHECK(bmp.at(4, 40, 2) == 236);
}
