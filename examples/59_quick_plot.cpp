// 59_quick_plot.cpp —— matplotlib 式「一行出图」（api::plot / bar / scatter）
//
// 直击新手痛点：过去为选一个 backend 要注册、调接口、查文档；现在 `api::plot(x, y)`
// 一行即见图——backend 选择、颜色降级、坐标轴/网格/标题全藏在壳里，新手入口零配置。
// 内部走 CellBuffer → Canvas → chart → Renderer → stdout（ANSI），不注册任何 backend。
#include <vector>

#include "termchart/api/quick.hpp"

int main() {
    using namespace termchart;

    const std::vector<double> x{0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<double> y;
    y.reserve(x.size());
    for (const double xi : x) {
        y.push_back(xi * xi);  // y = x^2
    }

    api::PlotOpts opts;
    opts.width = 60;
    opts.height = 16;
    opts.title = "y = x^2 (quick::plot)";

    // 一行折线：无需注册 backend、无需选字体。
    if (api::plot(x, y, opts) != ErrorCode::Ok) {
        return 1;
    }
    api::flush_stdout();

    // 一行柱状 + 一行散点：同一壳，三种图。
    if (api::bar({3, 5, 2, 8, 6}, {"a", "b", "c", "d", "e"}) != ErrorCode::Ok) {
        return 1;
    }
    if (api::scatter(x, y) != ErrorCode::Ok) {
        return 1;
    }
    api::flush_stdout();
    return 0;
}
