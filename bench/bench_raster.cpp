// bench_raster.cpp —— 标量场 → 字符格光栅化基线
//
// 度量 `raster::rasterize`（FloatField → CellBuffer）在典型终端尺寸 × 六种光栅器下的
// 耗时。关注点：像素数翻倍时耗时是否接近线性（逐格光栅化 O(像素)，不该出现平方级）。
// 输入全部现场生成（正弦标量场），不读文件，保证任意 cwd 可跑。
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bench.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/raster/rasterizer.hpp"

namespace {

using namespace termchart;

// 现场造一张 size×size 的标量场：横向正弦 + 纵向梯度，取值 [-1,1]。
image::FloatField make_field(std::uint32_t w, std::uint32_t h) {
    image::FloatField field(w, h, 0.0f);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const double v = std::sin(static_cast<double>(x) * 0.08) * 0.6 +
                             (static_cast<double>(y) / static_cast<double>(h) - 0.5) * 0.8;
            field.set(x, y, static_cast<float>(v));
        }
    }
    return field;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes =
        quick ? std::vector<std::pair<std::uint32_t, std::uint32_t>>{{80u, 24u}}
              : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
                    {80u, 24u}, {160u, 48u}, {320u, 96u}};
    const std::vector<raster::RasterKind> kinds =
        quick ? std::vector<raster::RasterKind>{raster::RasterKind::Braille,
                                                raster::RasterKind::HalfBlock}
              : std::vector<raster::RasterKind>{
                    raster::RasterKind::Braille, raster::RasterKind::HalfBlock,
                    raster::RasterKind::Quadrant, raster::RasterKind::Ascii,
                    raster::RasterKind::Block};
    const int rounds = quick ? 3 : 12;

    bench::header("bench_raster", "raster::rasterize（FloatField → CellBuffer）");
    for (const auto& s : sizes) {
        const image::FloatField field = make_field(s.first, s.second);
        const std::string size = std::to_string(s.first) + "x" + std::to_string(s.second);
        for (const raster::RasterKind kind : kinds) {
            const double ns = bench::median_ns(
                [&] { (void)raster::rasterize(field, kind); }, rounds);
            bench::report(raster::raster_name(kind), size, ns);
        }
    }
    return 0;
}
