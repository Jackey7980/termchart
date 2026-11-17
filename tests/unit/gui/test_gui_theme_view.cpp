// test_gui_theme_view.cpp —— termchart_gui 主题系统与 ViewWindow（Pan/Zoom）契约
//
// 验证：① 默认主题像素等价于改动前的硬色（既有 gui 像素测试零破坏），且可一键切换；
// ② ViewWindow 能限制 K 线 / 组合图的可见区间：compute_geom 只把可见蜡烛的 x 放进
//   绘图区，不可见蜡烛停到区外，hit_index 只在可见区间命中。
#include <cmath>
#include <string>
#include <vector>

#include <doctest.h>

#include "termchart_gui/theme.hpp"
#include "termchart_gui/widget_tree/combo_renderer.hpp"
#include "termchart_gui/widget_tree/hsi_kline_renderer.hpp"

using namespace termchart_gui;
using namespace termchart;

namespace {
std::vector<HsiKLineRenderer::Candle> make_candles(int n) {
    std::vector<HsiKLineRenderer::Candle> v;
    double p = 25000.0;
    for (int i = 0; i < n; ++i) {
        const double o = p;
        const double c = p + static_cast<double>(i % 3 - 1) * 120.0;
        const double hi = std::max(o, c) + 60.0;
        const double lo = std::min(o, c) - 60.0;
        v.push_back({o, hi, lo, c});
        p = c;
    }
    return v;
}
std::vector<std::string> make_labels(int n) {
    std::vector<std::string> v;
    for (int i = 0; i < n; ++i) v.push_back("2026-09-" + std::to_string(1 + (i % 28)));
    return v;
}
}  // namespace

TEST_CASE("gui theme default equals legacy dark + switchable") {
    set_theme(DefaultDark());
    CHECK(current_theme().background == Rgb{24, 26, 32});
    HsiKLineRenderer h;
    h.set_data(make_labels(10), make_candles(10));
    CHECK(h.background() == Rgb{24, 26, 32});

    set_theme(GitHubDark());
    CHECK(current_theme().background == Rgb{13, 17, 23});
    CHECK(h.background() == Rgb{13, 17, 23});  // 渲染器随当前主题取色

    set_theme(AppleLight());
    CHECK(current_theme().background == Rgb{255, 255, 255});
    set_theme(DefaultDark());  // 复位，避免影响其它用例
}

TEST_CASE("gui kline view window restricts visible range") {
    HsiKLineRenderer h;
    const int N = 25;
    h.set_data(make_labels(N), make_candles(N));
    const int W = 800, Hh = 500;

    auto g0 = h.compute_geom(W, Hh);
    CHECK(g0.n == N);
    int vis0 = 0;
    for (int i = 0; i < N; ++i)
        if ((g0.xs[i] >= g0.pl - 0.5 && g0.xs[i] <= g0.pr + 0.5)) ++vis0;
    CHECK(vis0 == N);  // 默认全可见

    h.set_view(ViewWindow{5, 15});  // 可见 [5,15) → 10 根
    auto g1 = h.compute_geom(W, Hh);
    CHECK(g1.n == N);
    CHECK((g1.xs[5] >= g1.pl && g1.xs[5] <= g1.pr));
    CHECK((g1.xs[14] >= g1.pl && g1.xs[14] <= g1.pr));
    CHECK(std::abs(g1.xs[4] - (g1.pl - 1.0)) < 1e-6);    // 不可见停在绘图区外
    CHECK(std::abs(g1.xs[15] - (g1.pr + 1.0)) < 1e-6);

    const double midx = (g1.xs[7] + g1.xs[8]) / 2.0;
    const int hit = h.hit_index(g1, midx, (g1.pt + g1.pb) / 2.0);
    CHECK((hit >= 5 && hit < 15));  // 命中只在可见区间
    CHECK(h.hit_index(g1, g1.pl - 5.0, (g1.pt + g1.pb) / 2.0) == -1);  // 区外不命中

    h.set_view(ViewWindow{0, -1});  // 复位
}

TEST_CASE("gui combo view window restricts visible range") {
    ComboRenderer c;
    const int N = 20;
    std::vector<std::string> labels;
    std::vector<double> bars, line;
    for (int i = 0; i < N; ++i) {
        labels.push_back(std::to_string(i));
        bars.push_back(10.0 + i);
        line.push_back(5.0 + i * 0.5);
    }
    c.set_combo_data(labels, bars, line);
    auto g0 = c.compute_geom(800, 500);
    CHECK(g0.n == N);

    c.set_view(ViewWindow{0, 10});  // 可见 [0,10) → 10 根
    auto g1 = c.compute_geom(800, 500);
    CHECK(g1.n == N);
    CHECK((g1.xs[0] >= g1.pl && g1.xs[0] <= g1.pr));
    CHECK((g1.xs[9] >= g1.pl && g1.xs[9] <= g1.pr));
    CHECK(std::abs(g1.xs[10] - (g1.pr + 1.0)) < 1e-6);
    const int hit = c.hit_index(g1, (g1.xs[3] + g1.xs[4]) / 2.0, (g1.pt + g1.pb) / 2.0);
    CHECK((hit >= 0 && hit < 10));

    c.set_view(ViewWindow{0, -1});
}
