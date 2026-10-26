// tile_grid.hpp —— 瓦片网格（示意地图级别）
//
// **精度立场**：每个区域一个瓦片（1 格），不做真实地理投影，**不做假
// 精度**——这是"一个区域一格"的示意图（类似 tile grid map），不适合
// 面积/形状读数。瓦片字符取区域名的首簇（空名用 █），颜色按值分级
// （灰阶 232..255，值越高越深）。
#ifndef TERMCHART_CHART_GEO_TILE_GRID_HPP
#define TERMCHART_CHART_GEO_TILE_GRID_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct GeoTile {
    std::string name;
    int col = 0;    // 瓦片在网格中的列（画布格）
    int row = 0;
    double value = 0.0;
};

struct TileGridOptions {
    int bins = 5;   // 分级数，收敛到 [1, 8]
};

class TileGrid final : public Chart {
public:
    void add_tile(GeoTile tile) { tiles_.push_back(std::move(tile)); }
    void set_options(const TileGridOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t tile_count() const noexcept { return tiles_.size(); }

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<GeoTile> tiles_;
    TileGridOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_TILE_GRID_HPP
