// util/geo/projection.hpp —— 经纬度 → 平面投影
//
// 极简集：4 种最常用投影，纯函数、零依赖、终端坐标友好（aspect 由 caller 自定）。
// 输入：纬度（-90..90）+ 经度（-180..180）→ 输出：平面 (x, y)（单位无定义，
//       调用方决定 1 单位 = 多少字符格 / 多少像素）。
//
// 约定：y 向上为正（北向上）；调用方需要"y 向下"时自行取负。
//
// 边界：极点附近 Mercator / Equirectangular 输出趋于无穷，由 caller 钳制。
#ifndef TERMCHART_UTIL_GEO_PROJECTION_HPP
#define TERMCHART_UTIL_GEO_PROJECTION_HPP

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::util::geo {

// 等距矩形（plate carrée）：最简单，lat → y, lon → x 直接线性。
// 适合：低分辨率地图、全局数据预览。
[[nodiscard]] TERMCHART_API Point equirectangular(double lat_deg, double lon_deg) noexcept;

// 墨卡托（Web Mercator）：google/高德/必应地图的标准投影。
// 公式：x = lon, y = ln(tan(π/4 + lat/2))
// 在 ±85.05113° 之外返回极大值（Mercator 极点奇点）。
[[nodiscard]] TERMCHART_API Point web_mercator(double lat_deg, double lon_deg) noexcept;

// 正射投影（orthographic）：从无穷远看地球的视点，圆形地图。
// 半球内有效；lat/lon 超出可见半球则返回 (NaN, NaN) 哨兵（由 caller 判定）。
[[nodiscard]] TERMCHART_API Point orthographic(double lat_deg, double lon_deg,
                                               double center_lat_deg,
                                               double center_lon_deg) noexcept;

// 兰勃特等面积圆柱投影：保持面积相等，世界地图常用。
// y = sin(lat), x = lon * cos(lat_mean)。
[[nodiscard]] TERMCHART_API Point lambert_cylindrical(double lat_deg, double lon_deg,
                                                       double reference_lat_deg = 0.0) noexcept;

}  // namespace termchart::util::geo

#endif  // TERMCHART_UTIL_GEO_PROJECTION_HPP
