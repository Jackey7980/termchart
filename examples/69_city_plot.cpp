// examples/69_city_plot.cpp —— 主要城市人口气泡图：底图 + 叠加层，盲文高清 + 分级着色
//
// 用法：69_city_plot [cities.geojson] [选项]
//   --base=world.geojson|none            底图（默认 assets/maps/world.json，只画灰轮廓）
//   --style=braille|quadrant|halfblock   子像素样式（默认 braille，2×4 = 8× 分辨率）
//   --cols=N --rows=M                    强制字符格尺寸（默认跟随终端）
//   --png[=PATH]                         导出位图（默认 city_plot.png，1600×1200）
//   --png-size=WxH                       位图尺寸
//   --color=auto|truecolor|none          颜色深度（重定向到文件时用 truecolor）
//   --list                               列出数据里的城市名（用于 set_value 对齐）
//
// 数据侧说明：assets/maps/cities.json 由 tools/fetch_map_data.py 从 Natural Earth
// populated places 生成——真实经纬度 + 真实人口，几何化成"气泡"多边形
// （半径 ∝ sqrt(pop_max)，因为 GeoMap 只认 Polygon，不支持 Point）。
// 所以这张图的信息是真的：气泡大小和颜色都由人口决定。
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

using termchart::Rgb;
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
    // 真实数据：城市人口（百万）。与 assets/maps/cities.json 里的 pop_max 一致，
    // 于是"气泡大小"（数据侧半径）与"颜色"（这里的分级值）表达同一个量。
    static constexpr std::pair<const char*, double> kPopulation[] = {
        {"Tokyo", 35.68},          {"New York", 19.04},     {"Mexico City", 19.03},
        {"Mumbai", 18.98},         {"Sao Paulo", 18.84},    {"Delhi", 15.93},
        {"Shanghai", 14.99},       {"Kolkata", 14.79},      {"Dhaka", 12.80},
        {"Buenos Aires", 12.79},   {"Los Angeles", 12.50},  {"Karachi", 12.13},
        {"Cairo", 11.89},          {"Rio de Janeiro", 11.75}, {"Osaka", 11.29},
        {"Beijing", 11.11},        {"Manila", 11.10},       {"Moscow", 10.45},
        {"Istanbul", 10.06},       {"Paris", 9.90},         {"Seoul", 9.80},
        {"Lagos", 9.47},           {"Jakarta", 9.12},       {"Chicago", 8.99},
    };
    for (const auto& [name, value] : kPopulation) map.set_value(name, value);
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "assets/maps/cities.json";
    std::string base_path = "assets/maps/world.json";
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
        } else if (starts_with(a, "--base=")) {
            base_path = std::string(a.substr(7));
            if (base_path == "none") base_path.clear();
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
    // 两张图的叠加顺序有讲究：栅格化时后加载的层盖住先加载的层，所以
    // 「城市层（有值 → 彩色气泡）」必须在「底图（不设值 → 灰轮廓）」之后。
    // 两者共用同一套投影与取景，不需要第二张地图。
    if (!base_path.empty()) {
        if (!map.add_overlay_geojson_file(base_path, err)) {
            std::fprintf(stderr, "base layer failed: %s\n", err.c_str());
            return 1;
        }
    }
    termchart::chart::GeoMapOptions mopts;
    mopts.projection = GeoProjection::LambertCylindrical;
    mopts.lambert_ref_lat = 30.0;
    map.set_options(mopts);
    attach_demo_values(map);
    map.set_title("Major Cities (Lambert Cylindrical, area ∝ population)");

    if (list_names) {
        for (const auto& name : map.feature_names()) std::printf("%s\n", name.c_str());
        return 0;
    }

    // ── 位图出口 ────────────────────────────────────────────────────────────
    if (want_png) {
        termchart::chart::GeoBitmapOptions bopts;
        bopts.no_data = Rgb{70, 74, 82};
        const termchart::image::Bitmap bmp = map.render_bitmap(png_w, png_h, bopts);
        const std::string png = termchart::image::encode_png(bmp);
        if (png.empty()) {
            std::fprintf(stderr, "png encode failed\n");
            return 1;
        }
        const std::string out = png_path.empty() ? std::string("city_plot.png") : png_path;
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
    // 底图（无值国家）压暗成"背景轮廓"，彩色气泡才跳出来。
    ropts.no_data = Rgb{70, 74, 82};
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
