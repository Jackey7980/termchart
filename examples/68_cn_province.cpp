// examples/68_cn_province.cpp —— 中国省级地图（墨卡托投影）+ 区域着色
//
// 用法：68_cn_province [geojson_path]
//   默认：assets/maps/cn.json → stdout
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/core/buffer.hpp"

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "assets/maps/cn.json";

    termchart::chart::GeoMap map;
    std::string err;
    if (!map.load_geojson_file(path, err)) {
        std::fprintf(stderr, "load failed: %s\n", err.c_str());
        return 1;
    }
    termchart::chart::GeoMapOptions opts;
    opts.projection = termchart::chart::GeoProjection::WebMercator;
    map.set_options(opts);
    map.set_title("China Provinces (Web Mercator)");

    // 演示数据：GDP 虚构值（仅示意）
    map.set_value("Guangdong",  120.0);
    map.set_value("Jiangsu",     95.0);
    map.set_value("Shandong",    80.0);
    map.set_value("Zhejiang",    70.0);
    map.set_value("Henan",       55.0);
    map.set_value("Sichuan",     50.0);
    map.set_value("Hubei",       45.0);
    map.set_value("Fujian",      42.0);
    map.set_value("Hunan",       40.0);
    map.set_value("Anhui",       38.0);
    map.set_value("Shanghai",    35.0);
    map.set_value("Beijing",     33.0);
    map.set_value("Heilongjiang",30.0);
    map.set_value("Liaoning",    28.0);
    map.set_value("Shaanxi",     25.0);
    map.set_value("Jiangxi",     24.0);
    map.set_value("Chongqing",   22.0);
    map.set_value("Yunnan",      20.0);
    map.set_value("Guangxi",     20.0);
    map.set_value("Inner Mongolia", 18.0);
    map.set_value("Xinjiang",    15.0);
    map.set_value("Tibet",       10.0);

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
    std::fputs(out.c_str(), stdout);
    return 0;
}
