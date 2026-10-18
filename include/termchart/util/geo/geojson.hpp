// util/geo/geojson.hpp —— GeoJSON 解析（最小集，仅 FeatureCollection）
//
// GeoJSON 规范参考 RFC 7946。本文件只覆盖：
//   * FeatureCollection → 一组 Polygon / MultiPolygon（边界数据）；
//   * 每个 Feature 的 `properties` 全部拷成 std::string → std::string；
//   * `geometry.type == "Polygon" | "MultiPolygon"`（其他类型抛错并返回 false）；
//   * 坐标数组 [lon, lat] → Point（高度忽略）。
//
// 用途：把世界地图/中国省份/主要城市的边界从 assets/maps/*.json 读进 termchart，
// 再走 util::geo::projection 投影到画布坐标系。
//
// 故意不写：
//   * GeometryCollection（用得少，Poly/MultiPoly 已覆盖 ≥99% 边界数据）；
//   * 弧线（RFC 7946 §3.1.6 弧线段，termchart 用直线段足够）；
//   * 跨子午线环（antimeridian cutting）—— caller 自己预处理。
//
// 错误：解析失败返回 false，错误信息通过 out_error 输出。
#ifndef TERMCHART_UTIL_GEO_GEOJSON_HPP
#define TERMCHART_UTIL_GEO_GEOJSON_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::util::geo {

struct GeoPolygon {
    std::vector<std::vector<Point>> rings;  // 首环外环，其余为内环（孔）
    std::vector<std::pair<std::string, std::string>> properties;
};

struct GeoFeatureCollection {
    std::vector<GeoPolygon> polygons;
};

// 从字符串解析。空字符串或缺 type/feature 字段返回 false。
[[nodiscard]] TERMCHART_API bool parse_geojson(const std::string& text,
                                                GeoFeatureCollection& out,
                                                std::string& out_error) noexcept;

// 从文件中读 + 解析（薄包装）。
[[nodiscard]] TERMCHART_API bool parse_geojson_file(const std::string& path,
                                                     GeoFeatureCollection& out,
                                                     std::string& out_error) noexcept;

// 经纬度范围（粗略 bbox，用于投影前估算画布 / 选投影）。
struct GeoBounds {
    double min_lat = 0.0, max_lat = 0.0;
    double min_lon = 0.0, max_lon = 0.0;
};
[[nodiscard]] TERMCHART_API GeoBounds
compute_bounds(const GeoFeatureCollection& fc) noexcept;

}  // namespace termchart::util::geo

#endif  // TERMCHART_UTIL_GEO_GEOJSON_HPP
