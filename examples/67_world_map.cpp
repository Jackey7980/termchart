// examples/67_world_map.cpp —— 世界地图：盲文高清 + 分级着色
//
// 用法：67_world_map [geojson] [选项]
//   --style=braille|quadrant|halfblock   子像素样式（默认 braille，2×4 = 8× 分辨率）
//   --cols=N --rows=M                    强制字符格尺寸（默认跟随终端）
//   --png[=PATH]                         导出位图（默认 world_map.png，1920×1080）
//   --png-size=WxH                       位图尺寸
//   --sixel                              直接在终端内联真彩位图（需终端支持）
//   --list                               列出数据里的区域名（用于 set_value 对齐）
//
// 为什么不再手工拼 ASCII：旧版把 CellBuffer 的每个非 ASCII 码点替换成 '?'、并把
// Style 整个丢掉，于是"世界地图 + 分级着色"实际输出的是无颜色的问号网格。
// 现在走 GeoMap::rasterize（子像素 + 逐格取主导区域色）与 render_bitmap（真彩位图），
// 两者与 draw() 共用同一份投影几何。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/protocol/sixel.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"

namespace {

using termchart::chart::GeoMap;
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
    // 演示数据：虚构值。真实用法是 set_value(区域名, 值)。
    map.set_value("China", 95.0);
    map.set_value("India", 88.0);
    map.set_value("United States of America", 78.0);
    map.set_value("Brazil", 65.0);
    map.set_value("Russia", 50.0);
    map.set_value("Australia", 42.0);
    map.set_value("France", 30.0);
    map.set_value("Germany", 28.0);
    map.set_value("Japan", 22.0);
    map.set_value("South Africa", 15.0);
    map.set_value("Egypt", 12.0);
    map.set_value("Canada", 10.0);
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "assets/maps/world.json";
    std::string png_path;
    bool        want_png = false;
    bool        want_sixel = false;
    bool        list_names = false;
    std::uint32_t cols = 0, rows = 0;
    std::uint32_t png_w = 1920, png_h = 1080;
    GeoRasterStyle style = GeoRasterStyle::Braille;
    // auto：跟随终端能力（重定向到文件时退化成无色，daemon-safe 的既定行为）。
    // truecolor：强制发 24 位 SGR——分级着色是这张图的主要信息，重定向看文件时需要。
    enum class ColorMode { Auto, TrueColor, None } color_mode = ColorMode::Auto;

    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--png") {
            want_png = true;
        } else if (starts_with(a, "--png=")) {
            want_png = true;
            png_path = std::string(a.substr(6));
        } else if (a == "--sixel") {
            want_sixel = true;
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
    attach_demo_values(map);
    map.set_title("World (Equirectangular)");

    if (list_names) {
        for (const auto& name : map.feature_names()) std::printf("%s\n", name.c_str());
        return 0;
    }

    // ── 位图出口：GUI / 文件 / sixel 共用 ────────────────────────────────────
    if (want_png || want_sixel) {
        termchart::chart::GeoBitmapOptions bopts;
        const termchart::image::Bitmap bmp = map.render_bitmap(png_w, png_h, bopts);

        if (want_sixel) {
            termchart::protocol::SixelProtocol sixel;
            const std::string bytes = sixel.encode(bmp);
            if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
            (void)termchart::api::flush_stdout();
            return 0;
        }

        const std::string png = termchart::image::encode_png(bmp);
        if (png.empty()) {
            std::fprintf(stderr, "png encode failed\n");
            return 1;
        }
        const std::string out = png_path.empty() ? std::string("world_map.png") : png_path;
        std::ofstream f(out, std::ios::binary);
        f.write(png.data(), static_cast<std::streamsize>(png.size()));
        if (!f) {
            std::fprintf(stderr, "write failed: %s\n", out.c_str());
            return 1;
        }
        std::printf("wrote %s (%ux%u, %zu bytes)\n", out.c_str(), png_w, png_h, png.size());
        return 0;
    }

    // ── 终端出口：子像素栅格化 + 真彩色 ─────────────────────────────────────
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    if (cols == 0 || rows == 0) {
        const termchart::term::TerminalSize sz = terminal.size();
        // 非 tty（ctest / 重定向）下 size 可能不可靠，退到一个确定的默认值，
        // 保证测试输出稳定且不会超出常见终端。
        const std::uint32_t tcols = sz.cols > 20 ? sz.cols : 100u;
        const std::uint32_t trows = sz.rows > 10 ? sz.rows : 32u;
        if (cols == 0) cols = tcols;
        // 留出标题行与提示行，否则图像会被终端折行/滚动挤歪（致命提醒 11）。
        if (rows == 0) rows = trows > 10 ? trows - 2 : 30u;
    }

    GeoRasterOptions ropts;
    ropts.style = style;

    termchart::CellBuffer buf = map.rasterize(cols, rows, ropts);

    // 标题单独放一行：标题走 SGR，不占地图格。
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
