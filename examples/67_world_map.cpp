// examples/67_world_map.cpp —— 真地图示例：世界地图 + 等距矩形投影 + 分级着色
//
// 用法：67_world_map [geojson_path] [output]
//   默认：assets/maps/world.json → stdout（终端）
//   给第二个参数则写文件（终端外场景）。
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "assets/maps/world.json";

    termchart::chart::GeoMap map;
    std::string err;
    if (!map.load_geojson_file(path, err)) {
        std::fprintf(stderr, "load failed: %s\n", err.c_str());
        return 1;
    }
    map.set_title("World (Equirectangular)");

    // 演示数据：随机几个国家的"虚构值"
    map.set_value("China",  95.0);
    map.set_value("India",  88.0);
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

    termchart::canvas::Canvas canvas(96, 28);
    termchart::chart::DrawContext ctx{canvas};
    map.draw(ctx);

    termchart::CellBuffer buf = canvas.to_buffer();
    std::string out(static_cast<std::size_t>(buf.height() * (buf.width() + 1)), '\n');
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const auto& cell = buf.at(x, y);
            const std::size_t idx = static_cast<std::size_t>(y) * (buf.width() + 1) + x;
            if (cell.cp == 0) out[idx] = ' ';
            else if (cell.cp < 0x80) out[idx] = static_cast<char>(cell.cp);
            else out[idx] = '?';
        }
    }
    if (argc > 2) {
        std::ofstream(argv[2]) << out;
    } else {
        std::fputs(out.c_str(), stdout);
    }
    return 0;
}
