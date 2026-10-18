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
//   * 输出有三条，分辨率从低到高（共用同一份投影几何，不重复实现投影）：
//     1. draw(DrawContext)     —— 字符格 Bresenham 描边，兼容旧调用方（最低清）
//     2. rasterize(cols, rows) —— 子像素栅格化：盲文 2×4 / 象限 2×2 / 半块 1×2，
//                                 逐格保留分级色（一次出图即有 8× 分辨率与颜色）
//     3. render_bitmap(w, h)   —— 真彩色位图任意像素尺寸（GUI / PNG / sixel 的数据源）
//     仍不直接 emit Scene：Scene 走 0..1 归一化坐标，而地图是"经纬度 ↔ 目标网格"的
//     强契约，塞进归一化坐标就丢掉了栅格化时的子像素归属。
#ifndef TERMCHART_CHART_GEO_GEO_MAP_HPP
#define TERMCHART_CHART_GEO_GEO_MAP_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
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

// 子像素样式。后面的倍数是对字符格的分辨率倍数：
//   Braille   2×4 = 8×（最高清；单格内多区域时取覆盖面积最大的区域取色）
//   Quadrant  2×2 = 4×（U+2596–U+259F 象限块，字体覆盖好）
//   HalfBlock 1×2 = 2×（上下半，字形最通用）
// 刻意不提供 sextant（2×3）：U+1FB00 区字体覆盖差，且位模式顺序有特殊规则，
// 收益低于维护成本。
enum class GeoRasterStyle : std::uint8_t {
    Braille, Quadrant, HalfBlock,
};

[[nodiscard]] TERMCHART_API const char* geo_raster_style_name(GeoRasterStyle s) noexcept;

// 子像素数（每字符格）。给调用方换算"要多少字符格才能达到目标分辨率"。
[[nodiscard]] TERMCHART_API std::uint32_t geo_raster_subpixel_width(GeoRasterStyle s) noexcept;
[[nodiscard]] TERMCHART_API std::uint32_t geo_raster_subpixel_height(GeoRasterStyle s) noexcept;

struct GeoRasterOptions {
    GeoRasterStyle style = GeoRasterStyle::Braille;
    Rgb   no_data{120, 120, 120};                   // 无值多边形
    // 子像素点亮阈值：该子像素被多边形覆盖的面积比例 >= 此值才置位。
    double coverage = 0.35;
    bool   draw_borders = true;                     // 区域边界压一条暗边
    Color  border       = Color::rgb(30, 30, 34);
    // 值色带：t∈[0,1] 采样。默认 Turbo（与热力图同源，颜色排序单调）。
    color::ColormapKind colormap = color::ColormapKind::Turbo;
};

struct GeoBitmapOptions {
    double margin = 16.0;               // 四周留白（像素）
    Rgb    background{18, 18, 22};
    Rgb    no_data{120, 120, 120};
    // 区域边界色：填充后按"相邻像素覆盖率突变"检测边缘并压暗一条 1 像素线。
    // 与 background 同色即等于关闭描边。
    Rgb    border{28, 28, 32};
    // 纵向超采样（1 = 关）。水平方向本来就按精确重叠算覆盖率，纵向靠这个补。
    std::uint32_t supersample = 2;
    color::ColormapKind colormap = color::ColormapKind::Turbo;
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

    // 叠加图层：把另一个 GeoJSON 追加到已加载数据之上，共用同一套投影、取景与
    // 值映射（先 load 底图，再 add_overlay 数据层）。
    // 栅格化时每个子像素记录"最后覆盖它的 feature"，所以后叠加的层天然画在上面；
    // 收口仍按覆盖面积最大者取色，不被蹭到的底图不会漏色。
    // 典型用法：国家轮廓当底图（不 set_value → 灰显）+ 城市气泡当数据层（有值 → 分级着色）。
    [[nodiscard]] bool add_overlay_geojson(const std::string& text, std::string& err);
    [[nodiscard]] bool add_overlay_geojson_file(const std::string& path, std::string& err);

    // 值映射：feature name → value。未命中的多边形不参与分级（仍按灰底画）。
    void set_value(const std::string& name, double value);
    void clear_values();

    void set_options(const GeoMapOptions& options) { options_ = options; }
    [[nodiscard]] const GeoMapOptions& options() const noexcept { return options_; }

    [[nodiscard]] std::size_t polygon_count() const noexcept;

    // 已加载区域的全部名字（按文件顺序）。set_value 的键必须与这里一致，
    // 否则那个区域会静默留在"无值灰底"——这是这个 getter 存在的唯一理由。
    [[nodiscard]] TERMCHART_API std::vector<std::string> feature_names() const;

    void draw(DrawContext ctx) override;

    // ── 高清出口 2：子像素栅格化（盲文/象限/半块）────────────────────────────
    // 输出 CellBuffer 的字符尺寸是 cols × rows；每格内部再按 style 切 2×4 等子像素。
    // 每个子像素记录"被哪个 feature 覆盖"，收口时每格取覆盖面积最大的 feature 取色——
    // 于是既拿到 8× 分辨率，又保住分级着色（这是单通道 FloatField 做不到的）。
    // 与 draw() 共用同一份投影，不重复实现投影逻辑。
    [[nodiscard]] TERMCHART_API CellBuffer rasterize(std::uint32_t cols, std::uint32_t rows,
                                                    const GeoRasterOptions& opts = {}) const;

    // ── 高清出口 3：真彩色位图 ────────────────────────────────────────────────
    // 任意像素尺寸，扫描线填充 + 区域描边 + 可选超采样抗锯齿。GUI / PNG / sixel 的数据源。
    [[nodiscard]] TERMCHART_API image::Bitmap render_bitmap(std::uint32_t width,
                                                           std::uint32_t height,
                                                           const GeoBitmapOptions& opts = {}) const;

private:
    struct Impl;
    void append_collection(util::geo::GeoFeatureCollection&& fc);
    std::unique_ptr<Impl> impl_;
    GeoMapOptions         options_;
    std::unordered_map<std::string, double> values_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_GEO_GEO_MAP_HPP
