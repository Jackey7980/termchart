// geo_map.cpp —— 真地图图种：GeoJSON → 投影 → 字符格多边形
//
// 实现要点：
//   * GeoJSON → util::geo::GeoFeatureCollection
//   * 每多边形按"匹配 properties.name"找 value，按 bins 分级着色
//   * 多边形顶点经纬度 → util::geo::projection 投影 → 画布坐标
//   * 字符格上用 canvas.draw_line 描外环 + canvas.put 写中心点 + draw_text 标 name
//   * 不填字符（多边形填色字符会糊到一起，描边才有"地图感"）
#include "termchart/chart/geo/geo_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>

#include "termchart/canvas/canvas.hpp"
#include "termchart/color/named_colors.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/geo/projection.hpp"

namespace termchart::chart {

const char* geo_projection_name(GeoProjection p) noexcept {
    switch (p) {
    case GeoProjection::Equirectangular:    return "equirectangular";
    case GeoProjection::WebMercator:        return "web_mercator";
    case GeoProjection::Orthographic:       return "orthographic";
    case GeoProjection::LambertCylindrical: return "lambert_cylindrical";
    }
    return "?";
}

namespace {

struct PolygonEntry {
    std::vector<std::vector<Point>> rings;
    std::string name;
    double value = 0.0;
    bool has_value = false;
};

// 值 → 分级色（与 Choropleth 同款灰阶，便于串味时调色一致）
struct Color256 {
    std::uint8_t r, g, b;
};
constexpr Color256 kNoData   = {120, 120, 120};
constexpr Color256 kEmpty    = {240, 240, 240};

Color256 ramp_color(double fraction01) {
    const double f = std::min(1.0, std::max(0.0, std::isfinite(fraction01) ? fraction01 : 0.0));
    const auto level = static_cast<std::uint8_t>(24 + (1.0 - f) * 215);  // 24..239
    return {level, level, level};
}

bool round_cell(double v, std::uint32_t limit, std::uint32_t& out) noexcept {
    if (!(v >= 0.0) || !(v < static_cast<double>(limit))) return false;
    const long long r = std::llround(v);
    if (r < 0 || r >= static_cast<long long>(limit)) return false;
    out = static_cast<std::uint32_t>(r);
    return true;
}

Point project(GeoProjection p, double lat, double lon, const GeoMapOptions& o) {
    switch (p) {
    case GeoProjection::Equirectangular:    return util::geo::equirectangular(lat, lon);
    case GeoProjection::WebMercator:        return util::geo::web_mercator(lat, lon);
    case GeoProjection::Orthographic:
        return util::geo::orthographic(lat, lon, o.ortho_center_lat, o.ortho_center_lon);
    case GeoProjection::LambertCylindrical:
        return util::geo::lambert_cylindrical(lat, lon, o.lambert_ref_lat);
    }
    return {0, 0};
}

}  // namespace

struct GeoMap::Impl {
    util::geo::GeoFeatureCollection fc;
    std::vector<PolygonEntry> entries;
    bool loaded = false;

    void rebuild_entries() {
        entries.clear();
        entries.reserve(fc.polygons.size());
        for (const auto& p : fc.polygons) {
            PolygonEntry e;
            e.rings = p.rings;
            for (const auto& [k, v] : p.properties) {
                if (k == "name" || k == "NAME" || k == "Name" || k == "iso_a3" || k == "adm0_a3") {
                    e.name = v;
                    break;
                }
            }
            entries.push_back(std::move(e));
        }
    }
};

GeoMap::GeoMap() : impl_(std::make_unique<Impl>()) {}
GeoMap::~GeoMap() = default;

bool GeoMap::load_geojson(const std::string& text, std::string& err) {
    util::geo::GeoFeatureCollection fc;
    if (!util::geo::parse_geojson(text, fc, err)) return false;
    impl_->fc = std::move(fc);
    impl_->rebuild_entries();
    impl_->loaded = true;
    return true;
}

bool GeoMap::load_geojson_file(const std::string& path, std::string& err) {
    util::geo::GeoFeatureCollection fc;
    if (!util::geo::parse_geojson_file(path, fc, err)) return false;
    impl_->fc = std::move(fc);
    impl_->rebuild_entries();
    impl_->loaded = true;
    return true;
}

void GeoMap::set_value(const std::string& name, double value) {
    values_[name] = value;
    for (auto& e : impl_->entries) {
        if (e.name == name) { e.value = value; e.has_value = true; }
    }
}

void GeoMap::clear_values() {
    values_.clear();
    for (auto& e : impl_->entries) e.has_value = false;
}

std::size_t GeoMap::polygon_count() const noexcept {
    return impl_->entries.size();
}

void GeoMap::draw(DrawContext ctx) {
    if (!impl_->loaded || impl_->entries.empty()) return;

    // 1) bbox
    const auto bbox = util::geo::compute_bounds(impl_->fc);

    // 2) 值域
    double vmin = 0, vmax = 0; bool have = false;
    for (const auto& e : impl_->entries) {
        if (!e.has_value) continue;
        if (!have) { vmin = vmax = e.value; have = true; }
        else { vmin = std::min(vmin, e.value); vmax = std::max(vmax, e.value); }
    }

    // 3) 投影 + 映射到画布
    const std::uint32_t W = ctx.canvas.width();
    const std::uint32_t H = ctx.canvas.height();
    if (W == 0 || H == 0) return;

    // 等距矩形用 (lat, lon) 直接作 (y, x)；其他投影需 bbox → 单位方块映射。
    // Y 轴：地理 lat 北正南负，画布 y 顶 0 底 H-1 → 翻转。
    auto project_to_canvas = [&](double lat, double lon) -> Point {
        const Point p = project(options_.projection, lat, lon, options_);
        if (options_.projection == GeoProjection::Equirectangular) {
            // 1:1 线性映射。v 用 (1 - frac) 让 lat=max（北）落到 y=0（顶），
            // lat=min（南）落到 y=H-1（底）—— 否则南显示在画面顶部（致命提醒 1）。
            const double u = (lon - bbox.min_lon) / std::max(1e-9, bbox.max_lon - bbox.min_lon);
            const double v = 1.0 - (lat - bbox.min_lat) / std::max(1e-9, bbox.max_lat - bbox.min_lat);
            return {u * (W - 1), v * (H - 1)};
        }
        // 其他投影：直接返回投影坐标（caller 自行理解；后续可加投影后 bbox 自适应）
        return p;
    };

    // 4) 描外环 + 中心标记 + name
    const int bins = std::min(8, std::max(1, static_cast<int>(options_.bins)));

    for (const auto& e : impl_->entries) {
        // 选色
        Color256 rgb;
        if (e.has_value && have && vmax > vmin) {
            const double frac = (e.value - vmin) / (vmax - vmin);
            rgb = ramp_color(frac);
        } else {
            rgb = kNoData;
        }
        Style st = Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b));

        // 每条 ring → 描边：用 Canvas::draw_line 走 Bresenham，字符按主方向选。
        // 主方向横（|dx|>|dy|）：─  ；竖：│ ；对角：▪  。
        for (const auto& ring : e.rings) {
            for (std::size_t i = 1; i < ring.size(); ++i) {
                const Point a = project_to_canvas(ring[i - 1].y, ring[i - 1].x);
                const Point b = project_to_canvas(ring[i].y,     ring[i].x);
                const double dx = std::abs(b.x - a.x);
                const double dy = std::abs(b.y - a.y);
                char32_t cp;
                if (dx > dy * 2.0)      cp = U'\u2500';  // ─ 横
                else if (dy > dx * 2.0) cp = U'\u2502';  // │ 竖
                else                    cp = U'\u2022';  // • 对角/短
                ctx.canvas.draw_line(a, b, cp, st);
            }
        }
        // 中心点（实心方块，让小国家也可视）
        if (!e.rings.empty() && !e.rings[0].empty()) {
            Point c{0, 0};
            for (const auto& p : e.rings[0]) { c.x += p.x; c.y += p.y; }
            c.x /= static_cast<double>(e.rings[0].size());
            c.y /= static_cast<double>(e.rings[0].size());
            const Point cc = project_to_canvas(c.y, c.x);
            std::uint32_t cx, cy;
            if (round_cell(cc.x, W, cx) && round_cell(cc.y, H, cy)) {
                ctx.canvas.put(cx, cy, U'\u25A0', st);  // ■
            }
        }
    }

    // 5) 标题
    if (!title().empty()) {
        layout::draw_title(ctx.canvas, title(),
                           Rect{0, 0, static_cast<double>(W), static_cast<double>(H)});
    }
}

}  // namespace termchart::chart
