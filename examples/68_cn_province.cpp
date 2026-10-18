// examples/68_cn_province.cpp —— 中国省级地图：盲文高清 + 分级着色（墨卡托投影）
//
// 用法：68_cn_province [geojson] [选项]
//   --style=braille|quadrant|halfblock   子像素样式（默认 braille，2×4 = 8× 分辨率）
//   --cols=N --rows=M                    强制字符格尺寸（默认跟随终端）
//   --png[=PATH]                         导出位图（默认 cn_province.png，1600×1200）
//   --png-size=WxH                       位图尺寸
//   --color=auto|truecolor|none          颜色深度（重定向到文件时用 truecolor）
//   --list                               列出数据里的省级区域名（用于 set_value 对齐）
//
// 与 67 相同的两条出口：终端走 GeoMap::rasterize（子像素 + 逐格取主导区域色），
// 文件走 GeoMap::render_bitmap（真彩位图）。旧的 ASCII 降级路径会把所有非 ASCII
// 码点换成 '?' 并丢掉 Style，等于"分级着色"根本没上屏。
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include "termchart/api/print.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/image/png_encode.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"

namespace {

using termchart::chart::GeoMap;
using termchart::chart::GeoProjection;
using termchart::chart::GeoRasterOptions;
using termchart::chart::GeoRasterStyle;

bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

GeoRasterStyle parse_style(std::string_view v) {
    if (v == "quadrant") return GeoRasterStyle::Quadrant;
    if (v == "halfblock") return GeoRasterStyle::HalfBlock;
    return GeoRasterStyle::Braille;
}

void attach_demo_values(GeoMap& map) {
    // 演示数据：虚构的 GDP 指数（仅示意分级着色，不是真实统计值）。
    // 键就是 assets/maps/cn.json 里的 name（英文省名）——可用 --list 核对。
    static constexpr std::pair<const char*, double> kValues[] = {
        {"Guangdong", 120.0}, {"Jiangsu", 95.0},   {"Shandong", 80.0},
        {"Zhejiang", 70.0},   {"Henan", 55.0},     {"Sichuan", 50.0},
        {"Hubei", 45.0},      {"Fujian", 42.0},    {"Hunan", 40.0},
        {"Anhui", 38.0},      {"Hebei", 38.0},     {"Shanghai", 35.0},
        {"Beijing", 33.0},    {"Heilongjiang", 30.0}, {"Taiwan", 30.0},
        {"Liaoning", 28.0},   {"Hong Kong", 26.0}, {"Shaanxi", 25.0},
        {"Jiangxi", 24.0},    {"Chongqing", 22.0}, {"Tianjin", 21.0},
        {"Yunnan", 20.0},     {"Guangxi", 20.0},   {"Inner Mongolia", 18.0},
        {"Shanxi", 16.0},     {"Xinjiang", 15.0},  {"Jilin", 14.0},
        {"Guizhou", 12.0},    {"Tibet", 10.0},     {"Gansu", 9.0},
        {"Hainan", 8.0},      {"Macao", 6.0},      {"Ningxia", 5.0},
        {"Qinghai", 4.0},
    };
    for (const auto& [name, value] : kValues) map.set_value(name, value);
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "assets/maps/cn.json";
    std::string png_path;
    bool want_png = false;
    bool list_names = false;
    std::uint32_t cols = 0, rows = 0;
    std::uint32_t png_w = 1600, png_h = 1200;
    GeoRasterStyle style = GeoRasterStyle::Braille;
    enum class ColorMode { Auto, TrueColor, None } color_mode = ColorMode::Auto;

    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--png") {
            want_png = true;
        } else if (starts_with(a, "--png=")) {
            want_png = true;
            png_path = std::string(a.substr(6));
        } else if (a == "--list") {
            list_names = true;
        } else if (starts_with(a, "--style=")) {
            style = parse_style(a.substr(8));
        } else if (starts_with(a, "--color=")) {
            const auto v = a.substr(8);
            if (v == "truecolor") color_mode = ColorMode::TrueColor;
            else if (v == "none") color_mode = ColorMode::None;
            else color_mode = ColorMode::Auto;
        } else if (starts_with(a, "--cols=")) {
            cols = static_cast<std::uint32_t>(std::strtoul(std::string(a.substr(7)).c_str(),
                                                           nullptr, 10));
        } else if (starts_with(a, "--rows=")) {
            rows = static_cast<std::uint32_t>(std::strtoul(std::string(a.substr(7)).c_str(),
                                                           nullptr, 10));
        } else if (starts_with(a, "--png-size=")) {
            const std::string v(a.substr(11));
            const auto x = v.find('x');
            if (x != std::string::npos) {
                png_w = static_cast<std::uint32_t>(std::strtoul(v.substr(0, x).c_str(),
                                                                nullptr, 10));
                png_h = static_cast<std::uint32_t>(std::strtoul(v.substr(x + 1).c_str(),
                                                                nullptr, 10));
            }
        } else if (!starts_with(a, "--")) {
            path = std::string(a);
        }
    }

    GeoMap map;
    std::string err;
    if (!map.load_geojson_file(path, err)) {
        std::fprintf(stderr, "load failed: %s\n", err.c_str());
        return 1;
    }
    termchart::chart::GeoMapOptions mopts;
    mopts.projection = GeoProjection::WebMercator;
    map.set_options(mopts);
    attach_demo_values(map);
    map.set_title("China Provinces (Web Mercator)");

    if (list_names) {
        for (const auto& name : map.feature_names()) std::printf("%s\n", name.c_str());
        return 0;
    }

    // ── 位图出口 ────────────────────────────────────────────────────────────
    if (want_png) {
        termchart::chart::GeoBitmapOptions bopts;
        const termchart::image::Bitmap bmp = map.render_bitmap(png_w, png_h, bopts);
        const std::string png = termchart::image::encode_png(bmp);
        if (png.empty()) {
            std::fprintf(stderr, "png encode failed\n");
            return 1;
        }
        const std::string out = png_path.empty() ? std::string("cn_province.png") : png_path;
        std::ofstream f(out, std::ios::binary);
        f.write(png.data(), static_cast<std::streamsize>(png.size()));
        if (!f) {
            std::fprintf(stderr, "write failed: %s\n", out.c_str());
            return 1;
        }
        std::printf("wrote %s (%ux%u, %zu bytes)\n", out.c_str(), png_w, png_h, png.size());
        return 0;
    }

    // ── 终端出口 ────────────────────────────────────────────────────────────
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    if (cols == 0 || rows == 0) {
        const termchart::term::TerminalSize sz = terminal.size();
        const std::uint32_t tcols = sz.cols > 20 ? sz.cols : 100u;
        const std::uint32_t trows = sz.rows > 10 ? sz.rows : 32u;
        if (cols == 0) cols = tcols;
        if (rows == 0) rows = trows > 10 ? trows - 2 : 30u;
    }

    GeoRasterOptions ropts;
    ropts.style = style;
    termchart::CellBuffer buf = map.rasterize(cols, rows, ropts);

    const std::uint32_t spw = cols * termchart::chart::geo_raster_subpixel_width(style);
    const std::uint32_t sph = rows * termchart::chart::geo_raster_subpixel_height(style);
    std::printf("\x1b[1;38;2;86;197;154m%s · %s · %ux%u cells = %ux%u subpixels\x1b[0m\n",
                map.title().c_str(), termchart::chart::geo_raster_style_name(style),
                cols, rows, spw, sph);

    termchart::ColorDepth depth = terminal.caps().color_depth;
    if (color_mode == ColorMode::TrueColor) depth = termchart::ColorDepth::TrueColor;
    else if (color_mode == ColorMode::None) depth = termchart::ColorDepth::None;

    termchart::render::Renderer renderer(depth);
    const std::string bytes = renderer.render_full(buf);
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
