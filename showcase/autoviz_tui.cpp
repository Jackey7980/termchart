// showcase/autoviz_tui.cpp —— 让 autoviz 自己"选对图"，复用 Panorama 难数据，TUI 渲染。
//
// 不手写任何图表类型：把 6 个相关品种的价格 + 等权指数拼成带时间列的宽表，交给
// termchart::autoviz 全链路（脏数据解析 → 列画像 → 管道路由 → 候选打分 → 选中），
// 再把选中的 chart 通过 api 渲染成 ANSI 一帧。默认非 tty 一次输出（便于重定向/进 ctest）。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "termchart/termchart_full.hpp"
#include "termchart/api/chart.hpp"
#include "autoviz_common.hpp"

namespace {

void usage(const char* prog) {
    std::fprintf(stderr,
        "用法: %s [--ansi FILE] [--svg FILE] [--csv FILE] [--width W] [--height H] [--help]\n"
        "  复用 Panorama 难数据(种子 20261022)，交给 autoviz 自动选图并渲染为终端图。\n"
        "  --ansi FILE   把 ANSI 帧写到文件(默认写到 stdout)\n"
        "  --svg  FILE   额外导出 SVG(矢量)\n"
        "  --csv  FILE   导出喂给 autoviz 的宽表 CSV(核对数据)\n"
        "  --width/--height  渲染尺寸(字符)，默认 120x40\n",
        prog);
}

}  // namespace

int main(int argc, char** argv) {
    std::string ansi_out, svg_out, csv_out;
    std::uint16_t width = 120, height = 40;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--ansi" && i + 1 < argc) ansi_out = argv[++i];
        else if (a == "--svg" && i + 1 < argc) svg_out = argv[++i];
        else if (a == "--csv" && i + 1 < argc) csv_out = argv[++i];
        else if (a == "--width" && i + 1 < argc) width = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--height" && i + 1 < argc) height = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--help" || a == "-h") { usage(argv[0]); return 0; }
        else { std::fprintf(stderr, "未知参数: %s\n", a.c_str()); usage(argv[0]); return 2; }
    }

    const showcase::AvResult r = showcase::run_autoviz();
    if (!r.error.empty() || !r.chart) {
        std::fprintf(stderr, "autoviz 失败: %s\n", r.error.c_str());
        return 1;
    }

    if (!csv_out.empty()) {
        FILE* f = std::fopen(csv_out.c_str(), "wb");
        if (f) { std::fwrite(r.csv.data(), 1, r.csv.size(), f); std::fclose(f); }
    }

    std::fprintf(stderr,
        "autoviz 选中: %s\n  标题: %s\n  管道: %s | 通过否决候选: %zu\n",
        r.name.c_str(), r.title.c_str(), r.pipeline.c_str(), r.viable);

    tc::api::ApiOptions o;
    o.width(width).height(height);

    auto ansi = tc::api::render_chart(*r.chart, o);
    if (!ansi) {
        std::fprintf(stderr, "render_chart 失败 code=%d\n", static_cast<int>(ansi.error()));
        return 1;
    }

    if (!svg_out.empty()) {
        auto svg = tc::api::chart_to_svg(*r.chart, width, height);
        if (svg) {
            FILE* f = std::fopen(svg_out.c_str(), "wb");
            if (f) { std::fwrite(svg->data(), 1, svg->size(), f); std::fclose(f); }
        } else {
            std::fprintf(stderr, "chart_to_svg 失败 code=%d\n", static_cast<int>(svg.error()));
        }
    }

    if (ansi_out.empty()) {
        std::fwrite(ansi->data(), 1, ansi->size(), stdout);
        std::fputc('\n', stdout);
    } else {
        FILE* f = std::fopen(ansi_out.c_str(), "wb");
        if (f) { std::fwrite(ansi->data(), 1, ansi->size(), f); std::fclose(f); }
    }
    return 0;
}
