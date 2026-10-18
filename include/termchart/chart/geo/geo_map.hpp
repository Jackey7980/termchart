// chart/geo/geo_map.hpp —— 真地图图种（GeoJSON + 投影 + 区域着色）
//
// 区别于 Choropleth：
//   * Choropleth：调用方给字符格集合（cells）+ 值，本库只画"值 → 颜色"。
//   * GeoMap：本库读 GeoJSON → 经纬度多边形 → 投影 → 字符格画多边形 + 按值着色。
//
// 设计选择：
//   * 输入：assets/maps/*.json（GeoJSON FeatureCollection）+ 一组 "feature_id → value"
//     映射（默认按 properties.name 匹配）；feature_id 与 name 都不匹配时该多边形灰显。
//   * 投影：等距矩形（默认）/ Mercator / 正射 / 兰勃特 —— enum 切换。
//   * 输出：终端走 canvas.draw_line / draw_text；SVG / LaTeX 由 caller 自行转换
//     （本图种不直接 emit Scene，因为地图投影是字符格 ↔ 经纬度 的强契约，
//     Scene 走的是 0..1 归一化坐标）。
#ifndef TERMCHART_CHART_GEO_GEO_MAP_HPP
#define TERMCHART_CHART_GEO_GEO_MAP_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/export.hpp"
#include "termchart/util/geo/geojson.hpp"

namespace termchart::chart {

enum class GeoProjection : std::uint8_t {
    Equirectangular, WebMercator, Orthographic, LambertCylindrical,
};

[[nodiscard]] TERMCHART_API const char* geo_projection_name(GeoProjection p) noexcept;

struct GeoMapOptions {
    GeoProjection projection = GeoProjection::Equirectangular;
    std::uint32_t width = 0;        // 0 = 自适应画布
    std::uint32_t height = 0;       // 0 = 自适应画布
    std::uint32_t bins = 5;         // 分级数（[1,8]）
    bool          show_graticule = false;   // 经纬网格
    std::int32_t  ortho_center_lat = 0;     // 正射投影中心纬度
    std::int32_t  ortho_center_lon = 0;     // 正射投影中心经度
    double        lambert_ref_lat = 0.0;    // 兰勃特参考纬度
};

class GeoMap final : public Chart {
public:
    GeoMap();
    ~GeoMap() override;

    GeoMap(const GeoMap&) = delete;
    GeoMap& operator=(const GeoMap&) = delete;

    // 加载 GeoJSON 文本（feature_id 默认按 properties.name 取）。
    [[nodiscard]] bool load_geojson(const std::string& text, std::string& err);
    [[nodiscard]] bool load_geojson_file(const std::string& path, std::string& err);

    // 值映射：feature name → value。未命中的多边形不参与分级（仍按灰底画）。
    void set_value(const std::string& name, double value);
    void clear_values();

    void set_options(const GeoMapOptions& options) { options_ = options; }
    [[nodiscard]] const GeoMapOptions& options() const noexcept { return options_; }

    [[nodiscard]] std::size_t polygon_count() const noexcept;

    void draw(DrawContext ctx) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    GeoMapOptions         options_;
    std::unordered_map<std::string, double> values_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_GEO_MAP_HPP
