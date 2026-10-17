// test_gui_stack.cpp —— termchart_gui 平行栈的单元测试
//
// 覆盖 layout / widget_tree / render::style_map / backend_tk（无头）/ native_loop。
// 复用 termchart 的 scene/color/image，不碰终端栈（gui_stack.md）。
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <memory>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/backend_tk/tui_backend.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/chart_widget.hpp"
#include "termchart_gui/widget_tree/checkbox.hpp"
#include "termchart_gui/widget_tree/combo_renderer.hpp"
#include "termchart_gui/widget_tree/gauge_renderer.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/polar_renderer.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
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

TEST_CASE("gui/widget: Checkbox 切换 + 勾选/未勾选像素") {
    Checkbox cb("cb", "Enable");
    cb.set_bounds(Rect{0, 0, 80, 20});
    bool state = false;
    cb.set_on_toggle([&](bool on) { state = on; });

    Event press{};
    press.type = EventType::MousePress;
    press.x = 40.0;
    press.y = 10.0;
    CHECK(cb.on_event(press) == true);
    CHECK(cb.checked() == true);
    CHECK(state == true);

    HeadlessBackend backend(80, 20);
    backend.canvas().clear(Rgb{0, 0, 0});
    cb.paint(backend.canvas());
    const Bitmap on = backend.canvas().snapshot();
    // 勾选内块（box 左上 (2,3) 边长 14，内缩 3 → (5..13, 6..14)）为绿色。
    CHECK(on.at(9, 10, 0) == 90);
    CHECK(on.at(9, 10, 1) == 200);
    CHECK(on.at(9, 10, 2) == 120);

    cb.set_checked(false);
    backend.canvas().clear(Rgb{0, 0, 0});
    cb.paint(backend.canvas());
    const Bitmap off = backend.canvas().snapshot();
    // 未勾选：同格回落为方框底色 (40,40,46)。
    CHECK(off.at(9, 10, 0) == 40);
    CHECK(off.at(9, 10, 2) == 46);
}

TEST_CASE("gui/widget: ProgressBar 按 value 比例填充") {
    ProgressBar pb("pb");
    pb.set_bounds(Rect{0, 0, 100, 10});
    pb.set_value(0.5);

    HeadlessBackend backend(100, 10);
    backend.canvas().clear(Rgb{0, 0, 0});
    pb.paint(backend.canvas());
    const Bitmap bmp = backend.canvas().snapshot();
    // 取中线 y=5（避开 1px 描边）：左半填充 (90,190,240)，右半轨道 (45,45,52)。
    CHECK(bmp.at(10, 5, 0) == 90);
    CHECK(bmp.at(10, 5, 2) == 240);
    CHECK(bmp.at(90, 5, 0) == 45);
    CHECK(bmp.at(90, 5, 2) == 52);
}

TEST_CASE("gui/backend: blit alpha 合成位图") {
    HeadlessBackend backend(20, 10);
    backend.canvas().clear(Rgb{0, 0, 0});
    image::Bitmap src(4, 4, 4);
    src.fill(200, 100, 50, 255);
    backend.canvas().blit(src, 2.0, 2.0);
    const image::Bitmap bmp = backend.canvas().snapshot();
    CHECK(bmp.at(3, 3, 0) == 200);  // blit 区内取到源像素
    CHECK(bmp.at(3, 3, 2) == 50);
    CHECK(bmp.at(0, 0, 0) == 0);    // 区外保持清屏背景
}

TEST_CASE("gui/backend: blit opacity 半透明合成") {
    HeadlessBackend backend(4, 4);
    backend.canvas().clear(Rgb{0, 0, 0});
    image::Bitmap src(2, 2, 4);
    src.fill(200, 100, 50, 255);
    backend.canvas().blit(src, 0.0, 0.0, 0.5);  // 50% 合成到黑底
    const image::Bitmap bmp = backend.canvas().snapshot();
    CHECK(bmp.at(0, 0, 0) >= 95);  // ≈ 200*0.5
    CHECK(bmp.at(0, 0, 0) <= 105);
    CHECK(bmp.at(0, 0, 1) >= 45);  // ≈ 100*0.5
    CHECK(bmp.at(0, 0, 1) <= 55);
}

TEST_CASE("gui/widget: ChartWidget 组合图渲染 + 悬停命中 + 缩放自适应") {
    ChartWidget ch("chart");
    ch.set_bounds(Rect{0, 0, 300, 160});
    ch.set_data({"a", "b", "c", "d"}, {10, 40, 20, 50}, {12, 38, 25, 48});

    HeadlessBackend backend(300, 160);
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());  // 首帧：建几何 + 光栅化缓存 + blit
    const image::Bitmap b1 = backend.canvas().snapshot();

    CHECK(b1.at(2, 2, 0) == 24);  // 左上角=图表深色底（blit 覆盖了控件区）
    int blue = 0;                 // 存在柱蓝色像素 → combo 真的画出来了
    for (std::uint32_t y = 0; y < b1.height(); ++y) {
        for (std::uint32_t x = 0; x < b1.width(); ++x) {
            if (b1.at(x, y, 0) == 59 && b1.at(x, y, 1) == 110 && b1.at(x, y, 2) == 246) {
                ++blue;
            }
        }
    }
    CHECK(blue > 0);

    // 半透明面积盖在柱子上：柱子被覆盖处是 青×蓝 混合色（既非纯柱蓝也非纯面积），
    // 即绿色被青色抬高、且不是纯柱蓝 (59,110,246)。
    int blended = 0;
    for (std::uint32_t y = 0; y < b1.height(); ++y) {
        for (std::uint32_t x = 0; x < b1.width(); ++x) {
            const int r = b1.at(x, y, 0);
            const int gg = b1.at(x, y, 1);
            const int bb = b1.at(x, y, 2);
            if (gg > 111 && gg < 175 && r < 70 && bb > 200) {
                ++blended;
            }
        }
    }
    CHECK(blended > 0);  // 面积真透在柱子上

    Event mv{};  // 悬停：移到类别 1 中心 x（cx1 = 46 + (300-14-46)*1.5/4 = 136）
    mv.type = EventType::MouseMove;
    mv.x = 136.0;
    mv.y = 90.0;
    CHECK(ch.on_event(mv) == true);
    CHECK(ch.hovered_index() == 1);

    ch.paint(backend.canvas());  // 悬停后重绘：base + 信息框
    const image::Bitmap bt = backend.canvas().snapshot();
    int tip = 0;  // 信息框底色 {32,36,46} 出现在数据点旁 → 悬停信息框真的画出来了
    for (std::uint32_t y = 0; y < bt.height(); ++y) {
        for (std::uint32_t x = 0; x < bt.width(); ++x) {
            if (bt.at(x, y, 0) == 30 && bt.at(x, y, 1) == 34 && bt.at(x, y, 2) == 44) {
                ++tip;
            }
        }
    }
    CHECK(tip > 0);

    int cross = 0;  // 悬停十字轴线 (90,100,120) 出现 → axisPointer 真画出来了
    for (std::uint32_t y = 0; y < bt.height(); ++y) {
        for (std::uint32_t x = 0; x < bt.width(); ++x) {
            if (bt.at(x, y, 0) == 90 && bt.at(x, y, 1) == 100 && bt.at(x, y, 2) == 120) {
                ++cross;
            }
        }
    }
    CHECK(cross > 0);

    Event out{};  // 移出绘图区（x < 左边界 46）→ -1
    out.type = EventType::MouseMove;
    out.x = 5.0;
    out.y = 5.0;
    CHECK(ch.on_event(out) == true);
    CHECK(ch.hovered_index() == -1);

    // 缩放自适应 + 过渡动画：放大到 420×240。
    ch.set_bounds(Rect{0, 0, 420, 240});
    HeadlessBackend backend2(420, 240);
    backend2.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend2.canvas());
    CHECK(ch.animating() == true);                   // 尺寸变化触发过渡
    CHECK(ch.plot_right() == doctest::Approx(286.0));  // 触发瞬间仍停在旧框架
    ch.tick(0.016);                                    // 一帧：框架立即吸附到新尺寸
    CHECK(ch.plot_right() == doctest::Approx(406.0));  // 撑满（不留暗边）
    CHECK(ch.animating() == true);                     // 内容(xs/ly)仍在平滑形变
    while (ch.animating()) ch.tick(0.05);              // 推进到静止
    ch.paint(backend2.canvas());
    CHECK(ch.animating() == false);
    CHECK(ch.plot_right() == doctest::Approx(406.0));  // 420-14，到位
    const image::Bitmap b2 = backend2.canvas().snapshot();
    CHECK(b2.width() == 420);
    CHECK(b2.at(2, 2, 0) == 24);
    Event mv2{};  // 新几何下类别 1 中心 x = 46 + (420-14-46)*1.5/4 = 181
    mv2.type = EventType::MouseMove;
    mv2.x = 181.0;
    mv2.y = 120.0;
    CHECK(ch.on_event(mv2) == true);
    CHECK(ch.hovered_index() == 1);
}

TEST_CASE("gui/widget: ChartWidget request_snap 拖动中吸附到位（不等 tick）") {
    ChartWidget ch("chart");
    ch.set_bounds(Rect{0, 0, 300, 160});
    ch.set_data({"a", "b", "c", "d"}, {10, 40, 20, 50}, {12, 38, 25, 48});
    HeadlessBackend backend(300, 160);
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());
    CHECK(ch.animating() == false);
    // 模拟拖动中：改尺寸 + request_snap + 立即 paint → 直接吸附，不走过渡。
    ch.set_bounds(Rect{0, 0, 420, 240});
    ch.request_snap();
    ch.paint(backend.canvas());
    CHECK(ch.plot_right() == doctest::Approx(406.0));  // 几何立即吸附（不走过渡）
    CHECK(ch.animating() == true);                     // 但变化触发交互高光 → 需 tick 衰减
    while (ch.animating()) ch.tick(0.05);
    CHECK(ch.animating() == false);                    // 高光衰减后归假
    CHECK(ch.plot_right() == doctest::Approx(406.0));  // 几何仍吸附
}

TEST_CASE("gui/widget: ChartWidget 拖动冻结抽稀、松手重算") {
    ChartWidget ch("chart");
    ch.set_bounds(Rect{0, 0, 600, 380});
    ch.set_data({"6-4", "6-7", "6-10", "6-13", "6-16", "6-19", "6-22", "6-25", "6-28", "7-1",
                 "7-2"},
                {120, 200, 150, 260, 210, 300, 240, 280, 190, 230, 260},
                {130, 210, 160, 250, 220, 310, 245, 290, 200, 240, 270});
    HeadlessBackend backend(600, 380);
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());
    CHECK(ch.label_step() == 1);  // 宽：11 个标签都放得下，全显示

    // 拖到很窄 + dragging：步长冻结（数量不变、只重排），不因宽度骤减而反复增删。
    ch.set_bounds(Rect{0, 0, 200, 160});
    ch.set_dragging(true);
    ch.request_snap();
    ch.paint(backend.canvas());
    CHECK(ch.label_step() == 1);  // 拖动中：冻结

    // 松手（set_dragging false）：按窄宽度重算 → step>1（抽稀，增删数据显示）。
    ch.set_dragging(false);
    ch.paint(backend.canvas());
    CHECK(ch.label_step() > 1);
}

TEST_CASE("gui/widget: ChartWidget 低清→高清交叉淡入") {
    ChartWidget ch("chart");
    ch.set_bounds(Rect{0, 0, 400, 300});
    ch.set_data({"a", "b", "c", "d"}, {10, 40, 20, 50}, {12, 38, 25, 48});
    HeadlessBackend backend(1000, 800);
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());
    CHECK(ch.animating() == false);  // 首帧落定（400×300，d=1 不降分辨率，无淡入）

    // 拖大到 1000×800（d=2 降分辨率）→ 低清快帧。
    ch.set_bounds(Rect{0, 0, 1000, 800});
    ch.set_dragging(true);
    ch.request_snap();
    ch.paint(backend.canvas());
    // 松手：重算成高清帧；因前帧是降分辨率低清 → 触发交叉淡入。
    ch.set_dragging(false);
    ch.paint(backend.canvas());
    CHECK(ch.animating() == true);  // 淡入进行中
    while (ch.animating()) ch.tick(0.05);
    CHECK(ch.animating() == false);  // 淡入收敛
}

TEST_CASE("gui/widget: ChartWidget set_data 数据更新走平滑过渡（非硬切）") {
    ChartWidget ch("chart");
    ch.set_bounds(Rect{0, 0, 300, 160});
    ch.set_data({"a", "b", "c", "d"}, {10, 40, 20, 50}, {12, 38, 25, 48});
    HeadlessBackend backend(300, 160);
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());
    CHECK(ch.animating() == false);  // 首帧落定

    // 同尺寸换数据 → 折线点位置变 → 应触发过渡（而非瞬间跳）。
    ch.set_data({"a", "b", "c", "d"}, {50, 10, 40, 20}, {48, 12, 38, 25});
    backend.canvas().clear(Rgb{0, 0, 0});
    ch.paint(backend.canvas());
    CHECK(ch.animating() == true);
    while (ch.animating()) ch.tick(0.05);
    ch.paint(backend.canvas());
    CHECK(ch.animating() == false);  // 收敛到新数据

    const image::Bitmap bmp = backend.canvas().snapshot();
    CHECK(bmp.at(2, 2, 0) == 24);  // 仍正常出图
}

// 天气看板用的"多系列"ComboRenderer：验证 ① 24h 温度（平滑折线+面积+最高温高亮）
// 与 ② 7 天（降水柱+最高/最低折线）都能经 ChartWidget 管线渲染出关键颜色像素。
TEST_CASE("gui/widget: ComboRenderer 多系列（天气看板）渲染") {
    const std::vector<std::string> hlab(24, "00");
    std::vector<double> htemp(24), hprec(24);
    for (int i = 0; i < 24; ++i) {
        htemp[static_cast<std::size_t>(i)] = 10.0 + 8.0 * std::sin(i / 3.0);
        hprec[static_cast<std::size_t>(i)] = static_cast<double>((i * 37) % 100);
    }

    // —— 上半屏：24h 温度（折线[0]+面积 + 折线[1]降水% + 最高温高亮）——
    auto temp_r = std::make_shared<ComboRenderer>();
    temp_r->set_series(hlab, {}, {htemp, hprec}, {"温度°C", "降水%"});
    temp_r->set_line_colors({{255, 138, 76}, {90, 170, 230}});
    temp_r->set_area(0, {255, 138, 76});
    temp_r->set_marker(0, {255, 210, 120});
    temp_r->set_show_bars(false);
    temp_r->set_title("24 小时温度 (°C)");

    ChartWidget temp_chart("temp");
    temp_chart.set_bounds(Rect{0, 0, 600, 300});
    temp_chart.set_renderer(temp_r);
    temp_chart.invalidate();

    HeadlessBackend b1(600, 300);
    b1.canvas().clear(Rgb{0, 0, 0});
    temp_chart.paint(b1.canvas());
    const image::Bitmap t = b1.canvas().snapshot();

    int orange = 0;  // 温度线/面积色 {255,138,76} 应出现
    int marker = 0;  // 最高温高亮 {255,210,120} 应出现
    for (std::uint32_t y = 0; y < t.height(); ++y) {
        for (std::uint32_t x = 0; x < t.width(); ++x) {
            if (t.at(x, y, 0) == 255 && t.at(x, y, 1) == 138 && t.at(x, y, 2) == 76) ++orange;
            if (t.at(x, y, 0) == 255 && t.at(x, y, 1) == 210 && t.at(x, y, 2) == 120) ++marker;
        }
    }
    CHECK(orange > 0);  // 温度折线/面积真的画出来了
    CHECK(marker > 0);  // 最高温高亮点真的画出来了

    // —— 下半屏：7 天 降水概率柱 + 最高/最低温折线 ——
    const std::vector<std::string> dlab = {"10-17", "10-18", "10-19", "10-20",
                                           "10-21", "10-22", "10-23"};
    const std::vector<double> dmax = {15.9, 16.7, 17.6, 16.2, 14.6, 14.4, 15.0};
    const std::vector<double> dmin = {12.7, 10.1, 7.0, 7.3, 4.2, 3.3, 4.7};
    const std::vector<double> dprec = {100, 5, 0, 22, 0, 2, 4};

    auto fc_r = std::make_shared<ComboRenderer>();
    fc_r->set_series(dlab, dprec, {dmax, dmin}, {"最高", "最低"});
    fc_r->set_line_colors({{255, 120, 90}, {80, 160, 255}});
    fc_r->set_bar_color({70, 130, 230});
    fc_r->set_bar_name("降水%");
    fc_r->set_area(-1, {0, 0, 0});  // 关闭面积
    fc_r->set_show_bars(true);
    fc_r->set_right_axis(true, {0, 1});  // 温度折线走右轴（双 Y 轴路径）
    fc_r->set_title("未来 7 天 最高/最低温 与 降水概率");

    ChartWidget fc_chart("fc");
    fc_chart.set_bounds(Rect{0, 0, 600, 300});
    fc_chart.set_renderer(fc_r);
    fc_chart.invalidate();

    HeadlessBackend b2(600, 300);
    b2.canvas().clear(Rgb{0, 0, 0});
    fc_chart.paint(b2.canvas());
    const image::Bitmap f = b2.canvas().snapshot();

    int bar = 0;    // 降水柱 {70,130,230}
    int hi = 0;     // 最高温线 {255,120,90}
    int lo = 0;     // 最低温线 {80,160,255}
    for (std::uint32_t y = 0; y < f.height(); ++y) {
        for (std::uint32_t x = 0; x < f.width(); ++x) {
            if (f.at(x, y, 0) == 70 && f.at(x, y, 1) == 130 && f.at(x, y, 2) == 230) ++bar;
            if (f.at(x, y, 0) == 255 && f.at(x, y, 1) == 120 && f.at(x, y, 2) == 90) ++hi;
            if (f.at(x, y, 0) == 80 && f.at(x, y, 1) == 160 && f.at(x, y, 2) == 255) ++lo;
        }
    }
    CHECK(bar > 0);   // 降水柱真的画出来了
    CHECK(hi > 0);    // 最高温折线真的画出来了
    CHECK(lo > 0);    // 最低温折线真的画出来了

    // 关闭面积后不应再出现 teal 面积色 {40,170,200}。
    int teal = 0;
    for (std::uint32_t y = 0; y < f.height(); ++y) {
        for (std::uint32_t x = 0; x < f.width(); ++x) {
            if (f.at(x, y, 0) == 40 && f.at(x, y, 1) == 170 && f.at(x, y, 2) == 200) ++teal;
        }
    }
    CHECK(teal == 0);  // 7 天图未启用面积 → 无青色面积层
}

// AQI 仪表盘（GaugeRenderer）：270° 分段色带 + 指针/轴帽。验证彩色分段与指针真的画出来。
TEST_CASE("gui/widget: GaugeRenderer 仪表盘渲染") {
    auto g = std::make_shared<GaugeRenderer>();
    g->set_value(165.0);  // 中度污染，落在 151-200 红区
    g->set_max(300.0);
    g->set_title("AQI");
    ChartWidget gw("aqi");
    gw.set_bounds(Rect{0, 0, 240, 240});
    gw.set_renderer(g);
    gw.invalidate();

    HeadlessBackend b(240, 240);
    b.canvas().clear(Rgb{0, 0, 0});
    gw.paint(b.canvas());
    const image::Bitmap bm = b.canvas().snapshot();

    int red = 0;  // 红区分段（151-200）{230,90,80}
    int white = 0;  // 指针/轴帽 {238,240,246}
    for (std::uint32_t y = 0; y < bm.height(); ++y) {
        for (std::uint32_t x = 0; x < bm.width(); ++x) {
            if (bm.at(x, y, 0) > 200 && bm.at(x, y, 1) < 120 && bm.at(x, y, 2) < 120) ++red;
            if (bm.at(x, y, 0) == 238 && bm.at(x, y, 1) == 240 && bm.at(x, y, 2) == 246) ++white;
        }
    }
    CHECK(red > 0);    // 彩色分段弧带真的画出来了
    CHECK(white > 0);  // 指针/轴帽真的画出来了
}

// 风玫瑰（PolarRenderer）：同心环 + 方位辐条 + 24h 风数据折线/点。验证数据线真的画出来。
TEST_CASE("gui/widget: PolarRenderer 风玫瑰渲染") {
    auto p = std::make_shared<PolarRenderer>();
    std::vector<Point> wp;
    wp.reserve(24);
    for (int i = 0; i < 24; ++i) {
        wp.push_back(Point{static_cast<double>((i * 15) % 360),
                           10.0 + 8.0 * std::sin(static_cast<double>(i) / 3.0)});
    }
    p->set_points(std::move(wp));
    p->set_title("24h Wind");
    ChartWidget pw("wind");
    pw.set_bounds(Rect{0, 0, 240, 240});
    pw.set_renderer(p);
    pw.invalidate();

    HeadlessBackend b(240, 240);
    b.canvas().clear(Rgb{0, 0, 0});
    pw.paint(b.canvas());
    const image::Bitmap bm = b.canvas().snapshot();

    int blue = 0;  // 数据折线 {90,170,230}
    for (std::uint32_t y = 0; y < bm.height(); ++y) {
        for (std::uint32_t x = 0; x < bm.width(); ++x) {
            if (bm.at(x, y, 0) == 90 && bm.at(x, y, 1) == 170 && bm.at(x, y, 2) == 230) ++blue;
        }
    }
    CHECK(blue > 0);  // 风玫瑰数据折线/点真的画出来了
}
