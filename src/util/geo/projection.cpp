#include "termchart/util/geo/projection.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::util::geo {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
}

Point equirectangular(double lat_deg, double lon_deg) noexcept {
    return {lon_deg, lat_deg};
}

Point web_mercator(double lat_deg, double lon_deg) noexcept {
    const double lat = std::clamp(lat_deg, -85.05113, 85.05113) * kDegToRad;
    const double lon = lon_deg * kDegToRad;
    const double y = std::log(std::tan(kPi / 4.0 + lat / 2.0));
    return {lon, y};
}

Point orthographic(double lat_deg, double lon_deg,
                   double center_lat_deg, double center_lon_deg) noexcept {
    const double lat     = lat_deg     * kDegToRad;
    const double lon     = lon_deg     * kDegToRad;
    const double cLat    = center_lat_deg * kDegToRad;
    const double cLon    = center_lon_deg * kDegToRad;
    const double cosC    = std::cos(cLat);
    const double x = std::cos(lat) * std::sin(lon - cLon);
    const double y = std::cos(cLat) * std::sin(lat) - std::sin(cLat) * std::cos(lat) * std::cos(lon - cLon);
    // 半球外（z < 0）返回哨兵：由 caller 判定 (x, y) 是否搭配 isfinite
    if (std::sin(cLat) * std::sin(lat) + cosC * std::cos(lat) * std::cos(lon - cLon) < 0) {
        return {std::nan(""), std::nan("")};
    }
    return {x, y};
}

Point lambert_cylindrical(double lat_deg, double lon_deg,
                          double reference_lat_deg) noexcept {
    const double lat = lat_deg * kDegToRad;
    const double ref = reference_lat_deg * kDegToRad;
    const double x = lon_deg * kDegToRad * std::cos(ref);
    const double y = std::sin(lat);
    return {x, y};
}

}  // namespace termchart::util::geo
