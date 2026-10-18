// examples/69_city_plot.cpp —— 主要城市点分布（兰勃特等面积投影）
//
// 用法：69_city_plot [geojson_path]
//   默认：assets/maps/cities.json
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/core/buffer.hpp"

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "assets/maps/cities.json";

    termchart::chart::GeoMap map;
    std::string err;
    if (!map.load_geojson_file(path, err)) {
        std::fprintf(stderr, "load failed: %s\n", err.c_str());
        return 1;
    }
    termchart::chart::GeoMapOptions opts;
    opts.projection = termchart::chart::GeoProjection::LambertCylindrical;
    opts.lambert_ref_lat = 30.0;
    map.set_options(opts);
    map.set_title("Major Cities (Lambert Cylindrical)");

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
