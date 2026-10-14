// test_widget.cpp —— widget 模块：基类 / 三主题 / 无状态组件 / 响应式值
//
// 关注契约而非"能画出来"：
//   * Widget 基类：旗标语义（set_focus 尊重 focusable）、默认命中测试；
//   * 三主题：constexpr 可用、槽位语义成立（深/浅对比方向、单色纯灰阶）；
//   * immediate：意图与几何正交、配色映射、UTF-8 截断、退化几何不崩；
//   * reactive：值未变不通知、快照遍历、重入挂起、RAII 退订。
#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/widget/border.hpp"
#include "termchart/widget/container.hpp"
#include "termchart/widget/dock.hpp"
#include "termchart/widget/grid_layout.hpp"
#include "termchart/widget/hbox.hpp"
#include "termchart/widget/immediate_widget.hpp"
#include "termchart/widget/panel.hpp"
#include "termchart/widget/reactive.hpp"
#include "termchart/widget/scroll.hpp"
#include "termchart/widget/style_sheet.hpp"
#include "termchart/widget/styles.hpp"
#include "termchart/widget/theme.hpp"
#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/theme_light.hpp"
#include "termchart/widget/theme_mono.hpp"
#include "termchart/widget/vbox.hpp"
#include "termchart/widget/widget.hpp"

namespace {

using namespace termchart;
using namespace termchart::widget;

// 占满 rect 的"底色格"填充把内容格也刷成 face 色，因此读回中心格的
// style 即可断言配色映射。坐标取 rect 中心（半开区间内）。
Point center_of(Rect r) { return Point{r.x + r.w / 2.0, r.y + r.h / 2.0}; }

// 深浅主题的对比方向：背景亮度必须与文字亮度拉开固定方向。
bool luma_less(Rgb a, Rgb b) noexcept { return a.luma() < b.luma(); }

}  // namespace

// ---------------------------------------------------------------------------
// Widget 基类
// ---------------------------------------------------------------------------

TEST_CASE("widget/base: 默认状态与默认命中测试") {
    Widget w("w1");

    CHECK(w.id() == "w1");
    CHECK(w.visible());
    CHECK(w.enabled());
    CHECK(w.focusable());
    CHECK_FALSE(w.focused());

    w.set_bounds(Rect{2, 2, 4, 3});
    CHECK(w.hit_test(Point{2, 2}));        // 半开区间：左上角在内
    CHECK(w.hit_test(Point{5.9, 4.9}));
    CHECK_FALSE(w.hit_test(Point{6, 4}));  // right/bottom 不含
    CHECK_FALSE(w.hit_test(Point{1, 2}));
}

TEST_CASE("widget/base: 旗标影响命中，set_focus 尊重 focusable") {
    Widget w("w2");
    w.set_bounds(Rect{0, 0, 2, 2});
    const Point inside{0.5, 0.5};

    w.set_visible(false);
    CHECK_FALSE(w.hit_test(inside));
    w.set_visible(true);
    w.set_enabled(false);
    CHECK_FALSE(w.hit_test(inside));
    w.set_enabled(true);
    CHECK(w.hit_test(inside));

    w.set_focusable(false);
    w.set_focus(true);
    CHECK_FALSE(w.focused());  // no-op，不是错误
    w.set_focusable(true);
    w.set_focus(true);
    CHECK(w.focused());
    w.set_focus(false);
    CHECK_FALSE(w.focused());
}

TEST_CASE("widget/base: 子类可覆写命中测试（圆形按钮）") {
    struct RoundButton : Widget {
        explicit RoundButton(Rect b) : Widget("round") { set_bounds(b); }
        bool hit_test(Point p) const noexcept override {
            const Point c = bounds().center();
            const double dx = p.x - c.x;
            const double dy = p.y - c.y;
            return visible() && enabled() && (dx * dx + dy * dy) <= bounds().w * bounds().w / 4.0;
        }
    };

    const RoundButton b(Rect{0, 0, 4, 4});
    CHECK(b.hit_test(Point{0.5, 2}));          // 半径 2：左边缘内
    CHECK_FALSE(b.hit_test(Point{0.1, 0.1}));  // 方形角落不在圆内
}

// ---------------------------------------------------------------------------
// 三主题
// ---------------------------------------------------------------------------

TEST_CASE("widget/theme: constexpr 可用且三表互异") {
    static_assert(kDarkTheme.background == Rgb{16, 18, 24});
    constexpr ThemePalette local = kLightTheme;
    static_assert(local.foreground == Rgb{30, 32, 38});

    CHECK(kDarkTheme != kLightTheme);
    CHECK(kLightTheme != kMonoTheme);
    CHECK(kDarkTheme != kMonoTheme);
}

TEST_CASE("widget/theme: 槽位语义成立") {
    // 深/浅主题背景-文字对比方向相反且都拉开距离。
    CHECK(luma_less(kDarkTheme.background, kDarkTheme.foreground));
    CHECK(luma_less(kLightTheme.foreground, kLightTheme.background));
    CHECK(kDarkTheme.background.luma() < 64.0);
    CHECK(kLightTheme.background.luma() > 192.0);

    // 单色主题：所有槽位纯灰阶（r==g==b）。
    for (const Rgb* c : {&kMonoTheme.background, &kMonoTheme.foreground, &kMonoTheme.border,
                         &kMonoTheme.accent, &kMonoTheme.text_dim, &kMonoTheme.success,
                         &kMonoTheme.danger, &kMonoTheme.highlight_bg}) {
        CHECK(c->r == c->g);
        CHECK(c->g == c->b);
    }
}

// ---------------------------------------------------------------------------
// immediate：按钮
// ---------------------------------------------------------------------------

TEST_CASE("widget/immediate/button: 配色映射按旗标切换") {
    const Rect r{1, 1, 10, 3};
    const auto cx = static_cast<std::uint32_t>(center_of(r).x);
    const auto cy = static_cast<std::uint32_t>(center_of(r).y);

    SUBCASE("常态") {
        canvas::Canvas canvas(16, 8);
        (void)button_clicked(canvas, r, "OK", kDarkTheme, ControlState{});
        const Cell& cell = canvas.at(cx, cy);
        CHECK(cell.style.fg() == Color::rgb(kDarkTheme.foreground));
        CHECK(cell.style.bg() == Color::rgb(kDarkTheme.background));
    }
    SUBCASE("按压反白") {
        canvas::Canvas canvas(16, 8);
        ControlState s{};
        s.hovered = true;
        s.pressed = true;
        (void)button_clicked(canvas, r, "OK", kDarkTheme, s);
        const Cell& cell = canvas.at(cx, cy);
        CHECK(cell.style.bg() == Color::rgb(kDarkTheme.accent));
        CHECK(cell.style.fg() == Color::rgb(kDarkTheme.background));
    }
    SUBCASE("悬停") {
        canvas::Canvas canvas(16, 8);
        ControlState s{};
        s.hovered = true;
        (void)button_clicked(canvas, r, "OK", kDarkTheme, s);
        CHECK(canvas.at(cx, cy).style.bg() == Color::rgb(kDarkTheme.highlight_bg));
    }
    SUBCASE("禁用") {
        canvas::Canvas canvas(16, 8);
        ControlState s{};
        s.enabled = false;
        (void)button_clicked(canvas, r, "OK", kDarkTheme, s);
        CHECK(canvas.at(cx, cy).style.fg() == Color::rgb(kDarkTheme.text_dim));
    }
}

TEST_CASE("widget/immediate/button: 意图与几何正交（含退化几何）") {
    const ControlState hit{true, true, false, true};    // hovered, pressed
    const ControlState miss{false, true, false, true};  // 未悬停不算点
    const ControlState off{true, true, false, false};   // 禁用不算点

    canvas::Canvas canvas_hit(16, 8);
    canvas::Canvas canvas_miss(16, 8);
    canvas::Canvas canvas_off(16, 8);
    CHECK(button_clicked(canvas_hit, Rect{1, 1, 10, 3}, "OK", kDarkTheme, hit));
    CHECK_FALSE(button_clicked(canvas_miss, Rect{1, 1, 10, 3}, "OK", kDarkTheme, miss));
    CHECK_FALSE(button_clicked(canvas_off, Rect{1, 1, 10, 3}, "OK", kDarkTheme, off));

    // 空矩形：不绘制、不崩，意图照报（几何只影响绘制——契约）。
    canvas::Canvas canvas(16, 8);
    canvas.fill_rect(Rect{4, 4, 2, 2}, U'x', style_fg(Rgb{1, 2, 3}));
    CHECK(button_clicked(canvas, Rect{}, "OK", kDarkTheme, hit));
    CHECK(canvas.at(4, 4).cp == U'x');  // 画布未被空矩形绘制污染
}

TEST_CASE("widget/immediate/button: 标签居中、按码点截断") {
    const Rect r{0, 0, 6, 1};  // 内边距 1 → 可用 4 格
    canvas::Canvas canvas(8, 1);

    (void)button_clicked(canvas, r, "AB", kDarkTheme, ControlState{});
    // 2 码点居中于 4 格：起点 x = 0+1+(4-2)/2 = 2
    CHECK(canvas.at(2, 0).cp == U'A');
    CHECK(canvas.at(3, 0).cp == U'B');
    CHECK(canvas.at(1, 0).cp == U' ');  // 内边距仍是填充格

    canvas.clear();
    (void)button_clicked(canvas, r, "ABCDEFG", kDarkTheme, ControlState{});
    // 截断到 4 码点，超出的 EFG 不得落在矩形之外。
    CHECK(canvas.at(1, 0).cp == U'A');
    CHECK(canvas.at(4, 0).cp == U'D');
    CHECK(canvas.at(5, 0).cp == U' ');
    CHECK(canvas.at(6, 0).cp == U' ');
    CHECK(canvas.at(7, 0).cp == U' ');
}

// ---------------------------------------------------------------------------
// immediate：复选框
// ---------------------------------------------------------------------------

TEST_CASE("widget/immediate/checkbox: 选中态字符与切换意图") {
    const Rect r{0, 0, 8, 1};

    canvas::Canvas canvas(12, 1);
    (void)checkbox_toggled(canvas, r, "opt", true, kDarkTheme, ControlState{});
    CHECK(canvas.at(0, 0).cp == U'[');
    CHECK(canvas.at(1, 0).cp == U'x');
    CHECK(canvas.at(2, 0).cp == U']');
    CHECK(canvas.at(3, 0).cp == U' ');
    CHECK(canvas.at(4, 0).cp == U'o');
    CHECK(canvas.at(1, 0).style.fg() == Color::rgb(kDarkTheme.accent));  // 选中标记

    canvas.clear();
    (void)checkbox_toggled(canvas, r, "opt", false, kDarkTheme, ControlState{});
    CHECK(canvas.at(1, 0).cp == U' ');
    CHECK(canvas.at(4, 0).cp == U'o');

    // 意图：与按钮同一套旗标规则；调用方据此翻转 checked。
    const ControlState hit{true, true, false, true};
    const ControlState no_hover{true, false, false, true};
    CHECK(checkbox_toggled(canvas, r, "opt", true, kDarkTheme, hit));
    CHECK_FALSE(checkbox_toggled(canvas, r, "opt", true, kDarkTheme, no_hover));
}

TEST_CASE("widget/immediate/checkbox: 退化几何不绘制不崩") {
    canvas::Canvas canvas(8, 2);
    const ControlState hit{true, true, false, true};

    // 宽度 < 3：框都放不下，整体跳过绘制，意图照报。
    CHECK(checkbox_toggled(canvas, Rect{0, 0, 2, 1}, "opt", true, kDarkTheme, hit));
    CHECK(canvas.at(0, 0).cp == U' ');

    // 高度 < 1 同理。
    CHECK(checkbox_toggled(canvas, Rect{0, 0, 8, 0}, "opt", true, kDarkTheme, hit));
    CHECK(canvas.at(0, 0).cp == U' ');
    CHECK(canvas.at(0, 1).cp == U' ');
}

// ---------------------------------------------------------------------------
// reactive
// ---------------------------------------------------------------------------

TEST_CASE("widget/reactive: 变更通知携带 new/old，值未变不通知") {
    Observable<int> obs{1};

    std::vector<std::pair<int, int>> seen;  // (new, old)
    Subscription sub =
        obs.subscribe([&seen](const int& nv, const int& ov) { seen.emplace_back(nv, ov); });
    REQUIRE(obs.listener_count() == 1);

    obs.set(2);
    obs.set(2);  // 值未变：不通知
    obs.set(5);

    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == std::make_pair(2, 1));
    CHECK(seen[1] == std::make_pair(5, 2));
    CHECK(obs.get() == 5);
}

TEST_CASE("widget/reactive: Subscription RAII 退订（析构/release/移动/覆盖）") {
    Observable<std::string> obs{"a"};
    int calls = 0;

    {
        Subscription sub = obs.subscribe(
            [&calls](const std::string&, const std::string&) { ++calls; });
        obs.set("b");
        CHECK(calls == 1);
        CHECK(sub.valid());
        sub.release();  // 显式退订，幂等
        sub.release();
        CHECK_FALSE(sub.valid());
        obs.set("c");
        CHECK(calls == 1);
    }  // 已 release 的句柄析构不再退订

    {
        Subscription sub = obs.subscribe(
            [&calls](const std::string&, const std::string&) { ++calls; });
        Subscription moved = std::move(sub);  // 所有权转移：原句柄失效
        CHECK_FALSE(sub.valid());
        CHECK(moved.valid());
        obs.set("d");
        CHECK(calls == 2);
        Subscription overwritten = std::move(moved);  // 被覆盖的句柄自动退订
        overwritten = obs.subscribe([](const std::string&, const std::string&) {});
        obs.set("e");
        CHECK(calls == 2);  // moved 的回调已随覆盖退订
        CHECK(obs.listener_count() == 1);
    }
}

TEST_CASE("widget/reactive: 通知中退订安全（快照遍历）") {
    Observable<int> obs{0};
    int b_calls = 0;

    Subscription a = obs.subscribe([](const int&, const int&) {});  // A：占位
    Subscription b = obs.subscribe([&a, &b_calls](const int&, const int&) {
        ++b_calls;
        a.release();  // B 的回调里退订 A：不影响本轮快照
    });

    obs.set(1);  // 快照里有 A、B；B 退订 A
    CHECK(b_calls == 1);
    CHECK_FALSE(a.valid());
    CHECK(obs.listener_count() == 1);

    obs.set(2);  // A 已不在表里，B 照常收到
    CHECK(b_calls == 2);
}

TEST_CASE("widget/reactive: 回调内重入 set 挂起到本轮结束（最后写入者胜）") {
    Observable<int> obs{0};
    std::vector<int> order;

    Subscription a = obs.subscribe([&obs, &order](const int& nv, const int&) {
        order.push_back(nv);
        if (nv == 1) {
            obs.set(2);  // 重入：登记挂起，本轮结束后补发
            obs.set(3);  // 再重入：覆盖挂起值（最后写入者胜）
        }
    });

    obs.set(1);
    // 第一轮通知 (0→1)，回调里登记挂起；本轮走完后补发一次 (1→3)。
    // 中间的 2 从未通知（被 3 覆盖），值也未落在 obs 上。
    REQUIRE(order.size() == 2);
    CHECK(order[0] == 1);
    CHECK(order[1] == 3);
    CHECK(obs.get() == 3);
}

TEST_CASE("widget/reactive: 多监听者按订阅顺序收到通知") {
    Observable<int> obs{0};
    std::vector<int> order;

    Subscription first = obs.subscribe([&order](const int& nv, const int&) {
        order.push_back(nv * 10);
    });
    Subscription second = obs.subscribe([&order](const int& nv, const int&) {
        order.push_back(nv * 100);
    });
    (void)first;
    (void)second;

    obs.set(7);
    REQUIRE(order.size() == 2);
    CHECK(order[0] == 70);   // 订阅顺序
    CHECK(order[1] == 700);
}

// ---------------------------------------------------------------------------
// 容器 / 布局
// ---------------------------------------------------------------------------

TEST_CASE("widget/container: add/remove/children + 去重") {
    VBox box("box");
    Widget a("a");
    Widget b("b");
    box.add(a);
    box.add(b);
    box.add(b);  // 重复添加被忽略
    REQUIRE(box.children().size() == 2);

    CHECK(box.remove(b));
    CHECK_FALSE(box.remove(b));  // 已移除：no-op
    CHECK(box.children().size() == 1);

    box.clear();
    CHECK(box.children().empty());
}

TEST_CASE("widget/container: 递归命中穿透多级容器") {
    VBox outer("outer");
    outer.set_bounds(Rect{0, 0, 5, 5});
    VBox inner("inner");
    inner.set_bounds(Rect{4, 4, 3, 3});  // 延伸到 outer 边界之外
    Widget leaf("leaf");
    leaf.set_bounds(Rect{5, 5, 1, 1});
    inner.add(leaf);
    outer.add(inner);

    // (5.5,5.5) 落在 outer 之外、leaf 之内：只有递归命中能做到（自身 bounds 不含）。
    CHECK(outer.hit_test(Point{5.5, 5.5}));
    CHECK_FALSE(outer.hit_test(Point{50, 50}));  // 整树之外
}

TEST_CASE("widget/container/vbox: 堆叠 + spacing + Stretch 填满宽") {
    VBox box("box");
    box.set_bounds(Rect{0, 0, 10, 20});
    box.set_padding(1);
    box.set_spacing(2);
    box.set_alignment(Alignment::Stretch);
    Widget a("a");
    a.set_bounds(Rect{0, 0, 0, 3});
    Widget b("b");
    b.set_bounds(Rect{0, 0, 0, 4});
    box.add(a);
    box.add(b);
    box.layout();

    CHECK(a.bounds() == Rect{1, 1, 8, 3});   // 内容宽 = 10-2*1 = 8
    CHECK(b.bounds() == Rect{1, 6, 8, 4});   // y = 1 + 3 + 2
}

TEST_CASE("widget/container/hbox: 水平堆叠镜像于 vbox") {
    HBox box("box");
    box.set_bounds(Rect{0, 0, 20, 10});
    box.set_padding(1);
    box.set_spacing(2);
    box.set_alignment(Alignment::Stretch);
    Widget a("a");
    a.set_bounds(Rect{0, 0, 3, 0});
    Widget b("b");
    b.set_bounds(Rect{0, 0, 4, 0});
    box.add(a);
    box.add(b);
    box.layout();

    CHECK(a.bounds() == Rect{1, 1, 3, 8});   // 内容高 = 10-2 = 8
    CHECK(b.bounds() == Rect{6, 1, 4, 8});   // x = 1 + 3 + 2
}

TEST_CASE("widget/container/grid: 2x2 居中等分") {
    GridLayout g("g");
    g.set_bounds(Rect{0, 0, 10, 10});
    g.set_columns(2);
    g.set_spacing(0);
    g.set_padding(0);
    Widget a("a"), b("b"), c("c"), d("d");
    a.set_bounds(Rect{0, 0, 1, 1});
    b.set_bounds(Rect{0, 0, 1, 1});
    c.set_bounds(Rect{0, 0, 1, 1});
    d.set_bounds(Rect{0, 0, 1, 1});
    g.add(a);
    g.add(b);
    g.add(c);
    g.add(d);
    g.layout();

    CHECK(a.bounds() == Rect{2, 2, 1, 1});    // cell(0,0) 居中于 5x5
    CHECK(b.bounds() == Rect{7, 2, 1, 1});    // cell(1,0)
    CHECK(c.bounds() == Rect{2, 7, 1, 1});    // cell(0,1)
    CHECK(d.bounds() == Rect{7, 7, 1, 1});    // cell(1,1)
}

TEST_CASE("widget/container/dock: 五区切分顺序 Top→Bottom→Left→Right→Center") {
    Dock d("d");
    d.set_bounds(Rect{0, 0, 20, 20});
    Widget top("top"), bot("bot"), left("left"), right("right"), center("center");
    top.set_bounds(Rect{0, 0, 0, 2});
    bot.set_bounds(Rect{0, 0, 0, 3});
    left.set_bounds(Rect{0, 0, 4, 0});
    right.set_bounds(Rect{0, 0, 5, 0});
    center.set_bounds(Rect{0, 0, 1, 1});
    d.add(DockRegion::Top, top);
    d.add(DockRegion::Bottom, bot);
    d.add(DockRegion::Left, left);
    d.add(DockRegion::Right, right);
    d.add(DockRegion::Center, center);
    d.layout();

    CHECK(top.bounds() == Rect{0, 0, 20, 2});
    CHECK(bot.bounds() == Rect{0, 17, 20, 3});
    CHECK(left.bounds() == Rect{0, 2, 4, 15});
    CHECK(right.bounds() == Rect{15, 2, 5, 15});
    CHECK(center.bounds() == Rect{4, 2, 11, 15});
}

TEST_CASE("widget/container/panel: content_rect 减边框 + 子铺进内容区") {
    Panel p("p");
    p.set_bounds(Rect{0, 0, 10, 10});
    p.set_border_style(BorderStyle::Single);
    CHECK(p.content_rect() == Rect{1, 1, 8, 8});
    p.set_border_style(BorderStyle::Double);
    CHECK(p.content_rect() == Rect{2, 2, 6, 6});
    p.set_border_style(BorderStyle::Single);

    VBox inner("inner");
    p.add(inner);
    p.layout();
    CHECK(inner.bounds() == Rect{1, 1, 8, 8});
}

TEST_CASE("widget/container/scroll: offset 夹取 + 子按 offset 平移") {
    ScrollArea s("s");
    s.set_bounds(Rect{0, 0, 10, 10});
    s.set_content_size(30, 30);
    s.set_offset(100, 100);  // 超出 → 夹到 (20,20)
    CHECK(s.offset().first == 20.0);
    CHECK(s.offset().second == 20.0);
    s.set_offset(-5, -5);  // 负 → 夹到 (0,0)
    CHECK(s.offset().first == 0.0);
    CHECK(s.offset().second == 0.0);

    // 内容小于视口时不滚动
    s.set_content_size(5, 5);
    s.set_offset(2, 2);
    CHECK(s.offset().first == 0.0);
    CHECK(s.offset().second == 0.0);

    s.set_content_size(30, 30);
    Widget child("child");
    child.set_bounds(Rect{5, 5, 2, 2});
    s.add(child);
    s.set_offset(3, 4);
    s.layout();
    CHECK(child.bounds() == Rect{2, 1, 2, 2});  // 坐标减 offset
}

TEST_CASE("widget/container: relayout 递归拍平整树") {
    Panel p("p");
    p.set_bounds(Rect{0, 0, 20, 20});
    VBox inner("inner");
    inner.set_padding(0);
    inner.set_spacing(0);
    inner.set_alignment(Alignment::Stretch);
    Widget a("a");
    a.set_bounds(Rect{0, 0, 0, 5});
    inner.add(a);
    p.add(inner);

    p.relayout();  // Panel::layout 把 inner 铺进内容区，再递归 inner.layout
    CHECK(inner.bounds() == Rect{1, 1, 18, 18});
    CHECK(a.bounds() == Rect{1, 1, 18, 5});
}

// ---------------------------------------------------------------------------
// 主题聚合 / 样式
// ---------------------------------------------------------------------------

TEST_CASE("widget/theme_agg: theme_by_name 取表，未知名回落 dark") {
    CHECK(theme_by_name("dark").name == "dark");
    CHECK(theme_by_name("light").palette == kLightTheme);
    CHECK(theme_by_name("mono").palette == kMonoTheme);
    CHECK(theme_by_name("nope").name == "dark");
}

TEST_CASE("widget/styles: 五态构造 + 按状态取样式") {
    const WidgetStyle s = button_style(kDarkTheme);
    CHECK(style_for(s, WidgetState::Normal).bg() == Color::rgb(kDarkTheme.background));
    CHECK(style_for(s, WidgetState::Active).bg() == Color::rgb(kDarkTheme.accent));
    CHECK(style_for(s, WidgetState::Disabled).fg() == Color::rgb(kDarkTheme.text_dim));
    CHECK(style_for(s, WidgetState::Hover).bg() == Color::rgb(kDarkTheme.highlight_bg));
}

TEST_CASE("widget/style_sheet: 类查表、覆盖与回落") {
    StyleSheet ss;
    ss.add_class("primary", button_style(kDarkTheme));
    REQUIRE(ss.find("primary") != nullptr);
    CHECK(ss.style_of("missing", label_style(kLightTheme)).normal.fg() ==
          Color::rgb(kLightTheme.foreground));
    CHECK_FALSE(ss.empty());

    ss.add_class("primary", label_style(kLightTheme));  // 覆盖
    CHECK(ss.style_of("primary").normal.fg() == Color::rgb(kLightTheme.foreground));
}
