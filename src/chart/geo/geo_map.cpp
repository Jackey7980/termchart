// geo_map.cpp —— 真地图图种：GeoJSON → 投影 → 字符格多边形
//
// 实现要点：
//   * GeoJSON → util::geo::GeoFeatureCollection
//   * 每多边形按"匹配 properties.name"找 value，按 bins 分级着色
//   * 多边形顶点经纬度 → util::geo::projection 投影 → 画布坐标
//   * 字符格上用 canvas.draw_line 描外环 + canvas.put 写中心点 + draw_text 标 name
//   * 不填字符（多边形填色字符会糊到一起，描边才有"地图感"）
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/color/named_colors.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/detail/braille_table.hpp"
#include "termchart/layout/legend.hpp"
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
    int  layer = 0;  // 0 = 底图，1.. = 叠加图层（越大越在上面）
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

// ── 高清出口共用：投影 + 等比取景 ──────────────────────────────────────────
// 旧 draw() 只对等距矩形做归一化，其他投影直接把投影坐标当格坐标（WebMercator
// 的 y 落在 ±π、兰勃特落在 ±1），整张图会被挤成几个格。这里统一"先投影、再按
// 投影后 bbox 等比缩放并居中"，四种投影都成立，且不会横向/纵向独立拉伸变形。
class GridFit {
public:
    GridFit(GeoProjection proj, const GeoMapOptions& o) : proj_(proj), opts_(o) {}

    void add(double lat, double lon) {
        const Point p = project(proj_, lat, lon, opts_);
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return;  // 正射：半球外
        if (!has_) {
            minx_ = maxx_ = p.x;
            miny_ = maxy_ = p.y;
            has_ = true;
        } else {
            minx_ = std::min(minx_, p.x);
            maxx_ = std::max(maxx_, p.x);
            miny_ = std::min(miny_, p.y);
            maxy_ = std::max(maxy_, p.y);
        }
    }

    [[nodiscard]] bool valid() const noexcept {
        return has_ && (maxx_ - minx_) > 1e-12 && (maxy_ - miny_) > 1e-12;
    }

    // 目标网格（子像素或像素）尺寸 → 等比缩放系数与居中偏移。
    void fit(double width, double height) noexcept {
        const double spanx = maxx_ - minx_;
        const double spany = maxy_ - miny_;
        scale_ = std::min(width / spanx, height / spany);
        ox_ = (width - spanx * scale_) * 0.5;
        oy_ = (height - spany * scale_) * 0.5;
    }

    // 经纬度 → 网格坐标（y 向下，北在上）。
    [[nodiscard]] Point to_grid(double lat, double lon) const {
        const Point p = project(proj_, lat, lon, opts_);
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return {kOff, kOff};
        return {ox_ + (p.x - minx_) * scale_, oy_ + (maxy_ - p.y) * scale_};
    }

    static constexpr double kOff = -1e9;

private:
    GeoProjection     proj_;
    const GeoMapOptions& opts_;
    bool   has_ = false;
    double minx_ = 0.0, maxx_ = 0.0, miny_ = 0.0, maxy_ = 0.0;
    double scale_ = 1.0, ox_ = 0.0, oy_ = 0.0;
};

// even-odd 多环交点：把该 feature 所有环的边一起求交、排序、两两配对。
// 多环一起做奇偶是关键——外环 + 内环（洞）共 4 个交点，配对后中间那段自然被排除，
// 于是 GeoJSON 的 holes 不需要单独处理。
void row_crossings(const std::vector<std::vector<Point>>& rings, double y,
                   std::vector<double>& out) {
    out.clear();
    for (const auto& ring : rings) {
        const std::size_t n = ring.size();
        if (n < 3) continue;
        for (std::size_t i = 0; i < n; ++i) {
            const Point& a = ring[i];
            const Point& b = ring[(i + 1) % n];  // GeoJSON 环本就闭合，尾→首边长度为 0
            if (!std::isfinite(a.x) || !std::isfinite(b.x)) continue;
            if ((a.y > y) != (b.y > y)) {
                const double t = (y - a.y) / (b.y - a.y);
                out.push_back(a.x + (b.x - a.x) * t);
            }
        }
    }
    std::sort(out.begin(), out.end());
}

// 值 → 分级索引（所有出口共用，保证 bins 在三条出口一致生效）。
// frac = (v - vmin)/(vmax - vmin) ∈ [0,1]；返回 [0, bins-1]。bins<=1 时恒为 0 级。
static int geo_bin_of(double frac, int bins) {
    if (bins <= 1) return 0;
    int b = static_cast<int>(std::floor(frac * static_cast<double>(bins)));
    if (b < 0) b = 0;
    if (b > bins - 1) b = bins - 1;
    return b;
}
// 分级级中心 → [0,1] 采样坐标（与旧 draw/to_scene 内联公式保持一致）。
static double geo_bin_center(double frac, int bins) {
    if (bins <= 1) return 0.0;
    const double t = static_cast<double>(geo_bin_of(frac, bins)) /
                     static_cast<double>(bins - 1);
    return std::min(1.0, std::max(0.0, t));
}

// 值 → 颜色（先按 bins 分级，再采样色带）。无值或值域退化（全相等）时用 no_data。
// draw/to_scene 走灰度 ramp_color，rasterize/render_bitmap 走色带 sample——
// 二者都先经过 geo_bin_center，于是同一份值在任一出口取到同一级色。
Rgb value_color(const PolygonEntry& e, bool have, double vmin, double vmax,
                int bins, color::ColormapKind kind, Rgb no_data) {
    if (!e.has_value || !have || !(vmax > vmin)) return no_data;
    const double frac = (e.value - vmin) / (vmax - vmin);
    return color::sample(kind, geo_bin_center(frac, bins));
}

// 象限块 16 种组合：位 0=左上 1=右上 2=左下 3=右下。
constexpr std::array<char32_t, 16> kQuadrantChars{
    U' ',       U'\u2598', U'\u259D', U'\u2580',  // 0 ▘ ▝ ▀
    U'\u2596', U'\u258C', U'\u259E', U'\u259B',   // ▖ ▌ ▞ ▛
    U'\u2597', U'\u259A', U'\u2590', U'\u259C',   // ▗ ▚ ▐ ▜
    U'\u2584', U'\u2599', U'\u259F', U'\u2588',   // ▄ ▙ ▟ █
};

// 半块 4 种组合：位 0=上 1=下。
constexpr std::array<char32_t, 4> kHalfBlockChars{
    U' ', U'\u2580', U'\u2584', U'\u2588',  // 空格 ▀ ▄ █
};

// 子像素 (sx, sy) 在给定样式下的位值。位含义按样式各自约定（见上两表/盲文表）。
inline std::uint8_t subpixel_bit(GeoRasterStyle style, std::uint32_t sx,
                                 std::uint32_t sy) noexcept {
    switch (style) {
    case GeoRasterStyle::Braille:
        return detail::braille_dots_for_cell(static_cast<int>(sx), static_cast<int>(sy));
    case GeoRasterStyle::Quadrant:
        return static_cast<std::uint8_t>(1u << (sy * 2 + sx));
    case GeoRasterStyle::HalfBlock:
        return static_cast<std::uint8_t>(1u << sy);
    }
    return 0;
}

char32_t raster_char(GeoRasterStyle style, std::uint8_t bits) noexcept {
    switch (style) {
    case GeoRasterStyle::Braille:   return detail::kBrailleChars[bits];
    case GeoRasterStyle::Quadrant:  return kQuadrantChars[bits & 0x0F];
    case GeoRasterStyle::HalfBlock: return kHalfBlockChars[bits & 0x03];
    }
    return U' ';
}

}  // namespace

struct GeoMap::Impl {
    util::geo::GeoFeatureCollection fc;
    std::vector<PolygonEntry> entries;
    // 与 fc.polygons 平行的图层号：0 = 底图，1.. = 依次叠加的图层。
    // 不放进 GeoPolygon（那是对外数据结构），只在这里维护。
    std::vector<int> layers;
    int next_layer = 1;
    bool loaded = false;

    void rebuild_entries() {
        entries.clear();
        entries.reserve(fc.polygons.size());
        const bool have_layers = layers.size() == fc.polygons.size();
        for (std::size_t i = 0; i < fc.polygons.size(); ++i) {
            const auto& p = fc.polygons[i];
            PolygonEntry e;
            e.rings = p.rings;
            e.layer = have_layers ? layers[i] : 0;
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
    impl_->layers.assign(impl_->fc.polygons.size(), 0);
    impl_->next_layer = 1;
    impl_->rebuild_entries();
    impl_->loaded = true;
    return true;
}

bool GeoMap::load_geojson_file(const std::string& path, std::string& err) {
    util::geo::GeoFeatureCollection fc;
    if (!util::geo::parse_geojson_file(path, fc, err)) return false;
    impl_->fc = std::move(fc);
    impl_->layers.assign(impl_->fc.polygons.size(), 0);
    impl_->next_layer = 1;
    impl_->rebuild_entries();
    impl_->loaded = true;
    return true;
}

bool GeoMap::add_overlay_geojson(const std::string& text, std::string& err) {
    util::geo::GeoFeatureCollection fc;
    if (!util::geo::parse_geojson(text, fc, err)) return false;
    append_collection(std::move(fc));
    return true;
}

bool GeoMap::add_overlay_geojson_file(const std::string& path, std::string& err) {
    util::geo::GeoFeatureCollection fc;
    if (!util::geo::parse_geojson_file(path, fc, err)) return false;
    append_collection(std::move(fc));
    return true;
}

void GeoMap::append_collection(util::geo::GeoFeatureCollection&& fc) {
    // 追加进同一份集合：投影/取景/值域全部自动覆盖新层，不需要第二套状态。
    auto& polys = impl_->fc.polygons;
    polys.reserve(polys.size() + fc.polygons.size());
    if (impl_->layers.size() != polys.size()) impl_->layers.resize(polys.size(), 0);
    const int layer = impl_->next_layer++;
    for (auto& p : fc.polygons) {
        polys.push_back(std::move(p));
        impl_->layers.push_back(layer);
    }

    // rebuild_entries 会重建 entries（has_value 归零），所以已有的值映射要重放一遍。
    impl_->rebuild_entries();
    for (const auto& [name, value] : values_) {
        for (auto& e : impl_->entries) {
            if (e.name == name) { e.value = value; e.has_value = true; }
        }
    }
    impl_->loaded = true;
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

std::vector<std::string> GeoMap::feature_names() const {
    // 一个 MultiPolygon feature 会拆成多个 entry（群岛、北极群岛、带飞地的国家），
    // 而 set_value 的键是 **feature 名**。按 entry 逐个列出只会把 --list 淹成
    // "Tianjin / Tianjin / Hebei ×4"，所以按首次出现顺序去重。
    // 注意：去重是**有损**的——同名的不同 feature（多份图层叠加）会被合并成一个名字，
    // set_value 无法分别给它们赋值。当前三份数据无同名冲突，够用；但 feature_names()
    // 仅用于展示/罗列，**不要当唯一键**使用。需要时请改用 polygon_count() 逐 entry 处理。
    std::vector<std::string> names;
    std::unordered_map<std::string, bool> seen;
    names.reserve(impl_->entries.size());
    for (const auto& e : impl_->entries) {
        if (seen.emplace(e.name, true).second) names.push_back(e.name);
    }
    return names;
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
        // 选色：先按 bins 量化成"级"，再取色带颜色（否则 options_.bins 形同虚设）
        Color256 rgb;
        if (e.has_value && have && vmax > vmin) {
            const double frac = (e.value - vmin) / (vmax - vmin);
            rgb = ramp_color(geo_bin_center(frac, bins));
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

bool GeoMap::to_scene(scene::Scene& out, std::uint32_t width,
                      std::uint32_t height) const {
    if (!impl_->loaded || impl_->entries.empty()) return true;

    // 1) bbox
    const auto bbox = util::geo::compute_bounds(impl_->fc);

    // 2) 值域
    double vmin = 0, vmax = 0; bool have = false;
    for (const auto& e : impl_->entries) {
        if (!e.has_value) continue;
        if (!have) { vmin = vmax = e.value; have = true; }
        else { vmin = std::min(vmin, e.value); vmax = std::max(vmax, e.value); }
    }

    if (width == 0 || height == 0) return true;

    // 等距矩形用 (lat, lon) 直接作 (y, x)；其他投影需 bbox → 单位方块映射。
    // Y 轴：地理 lat 北正南负，画布 y 顶 0 底 H-1 → 翻转。
    auto project_to_canvas = [&](double lat, double lon) -> Point {
        const Point p = project(options_.projection, lat, lon, options_);
        if (options_.projection == GeoProjection::Equirectangular) {
            // 1:1 线性映射。v 用 (1 - frac) 让 lat=max（北）落到 y=0（顶），
            // lat=min（南）落到 y=H-1（底）—— 否则南显示在画面顶部。
            const double u = (lon - bbox.min_lon) / std::max(1e-9, bbox.max_lon - bbox.min_lon);
            const double v = 1.0 - (lat - bbox.min_lat) / std::max(1e-9, bbox.max_lat - bbox.min_lat);
            return {u * (width - 1), v * (height - 1)};
        }
        // 其他投影：直接返回投影坐标（caller 自行理解；后续可加投影后 bbox 自适应）
        return p;
    };

    // 3) 描外环 + 中心标记 + name
    const int bins = std::min(8, std::max(1, static_cast<int>(options_.bins)));

    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    for (const auto& e : impl_->entries) {
        // 选色：先按 bins 量化成"级"，再取色带颜色（否则 options_.bins 形同虚设）
        Color256 rgb;
        if (e.has_value && have && vmax > vmin) {
            const double frac = (e.value - vmin) / (vmax - vmin);
            rgb = ramp_color(geo_bin_center(frac, bins));
        } else {
            rgb = kNoData;
        }
        Style st = Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b));

        // 每条 ring → 描边：用矢量 StrokeOp 复刻 Canvas::draw_line 的几何。
        for (const auto& ring : e.rings) {
            for (std::size_t i = 1; i < ring.size(); ++i) {
                const Point a = project_to_canvas(ring[i - 1].y, ring[i - 1].x);
                const Point bb = project_to_canvas(ring[i].y, ring[i].x);
                b.stroke(st.fg());
                b.move_to(a);
                b.line_to(bb);
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
            if (round_cell(cc.x, width, cx) && round_cell(cc.y, height, cy)) {
                sink.put(Point{static_cast<double>(cx), static_cast<double>(cy)},
                         U'\u25A0', st);  // ■
            }
        }
    }

    // 5) 标题
    if (!title().empty()) {
        layout::draw_title(sink, title(),
                           Rect{0, 0, static_cast<double>(width), static_cast<double>(height)});
    }
    b.end_group(0);
    return true;
}

// ── 高清出口 2：子像素栅格化 ────────────────────────────────────────────────

const char* geo_raster_style_name(GeoRasterStyle s) noexcept {
    switch (s) {
    case GeoRasterStyle::Braille:   return "braille";
    case GeoRasterStyle::Quadrant:  return "quadrant";
    case GeoRasterStyle::HalfBlock: return "halfblock";
    }
    return "?";
}

std::uint32_t geo_raster_subpixel_width(GeoRasterStyle s) noexcept {
    switch (s) {
    case GeoRasterStyle::Braille:   return 2;
    case GeoRasterStyle::Quadrant:  return 2;
    case GeoRasterStyle::HalfBlock: return 1;
    }
    return 2;
}

std::uint32_t geo_raster_subpixel_height(GeoRasterStyle s) noexcept {
    switch (s) {
    case GeoRasterStyle::Braille:   return 4;
    case GeoRasterStyle::Quadrant:  return 2;
    case GeoRasterStyle::HalfBlock: return 2;
    }
    return 4;
}

CellBuffer GeoMap::rasterize(std::uint32_t cols, std::uint32_t rows,
                             const GeoRasterOptions& opts) const {
    CellBuffer out(cols, rows);
    if (cols == 0 || rows == 0 || !impl_->loaded || impl_->entries.empty()) {
        return out;
    }

    const std::uint32_t sw = geo_raster_subpixel_width(opts.style);
    const std::uint32_t sh = geo_raster_subpixel_height(opts.style);
    const std::uint32_t SPW = cols * sw;
    const std::uint32_t SPH = rows * sh;

    // 1) 投影 + 等比取景（在子像素空间里做，于是 2×4 的子像素近似正方形）。
    GridFit fit(options_.projection, options_);
    for (const auto& e : impl_->entries) {
        for (const auto& ring : e.rings) {
            for (const auto& p : ring) fit.add(p.y, p.x);  // p = {lon, lat}
        }
    }
    if (!fit.valid()) return out;
    fit.fit(static_cast<double>(SPW), static_cast<double>(SPH));

    // 2) 值域 → 每 feature 一色；同时把顶点预投影（每个顶点只投影一次）。
    double vmin = 0.0, vmax = 0.0;
    bool have = false;
    for (const auto& e : impl_->entries) {
        if (!e.has_value) continue;
        if (!have) { vmin = vmax = e.value; have = true; }
        else { vmin = std::min(vmin, e.value); vmax = std::max(vmax, e.value); }
    }

    struct ProjEntry {
        std::vector<std::vector<Point>> rings;
        double ymin = 0.0, ymax = 0.0;
        bool   has_extent = false;
    };
    std::vector<ProjEntry>    proj;
    std::vector<Color>        colors;
    std::vector<int>          lay;
    proj.reserve(impl_->entries.size());
    colors.reserve(impl_->entries.size());
    lay.reserve(impl_->entries.size());
    for (const auto& e : impl_->entries) {
        const Rgb c = value_color(e, have, vmin, vmax,
                                  static_cast<int>(options_.bins), opts.colormap, opts.no_data);
        colors.push_back(Color::rgb(c.r, c.g, c.b));
        lay.push_back(e.layer);

        ProjEntry pe;
        pe.rings.reserve(e.rings.size());
        for (const auto& ring : e.rings) {
            std::vector<Point> pr;
            pr.reserve(ring.size());
            for (const auto& p : ring) {
                const Point g = fit.to_grid(p.y, p.x);
                if (g.x <= GridFit::kOff) continue;  // 正射半球外：跳过该顶点
                if (!pe.has_extent) { pe.ymin = pe.ymax = g.y; pe.has_extent = true; }
                else { pe.ymin = std::min(pe.ymin, g.y); pe.ymax = std::max(pe.ymax, g.y); }
                pr.push_back(g);
            }
            pe.rings.push_back(std::move(pr));
        }
        proj.push_back(std::move(pe));
    }

    // 3) 累计每个子像素的归属与水平覆盖率。
    std::vector<std::int32_t> owner(static_cast<std::size_t>(SPW) * SPH, -1);
    std::vector<float>        cover(static_cast<std::size_t>(SPW) * SPH, 0.0f);
    std::vector<double>       xs;

    for (std::size_t fi = 0; fi < proj.size(); ++fi) {
        const ProjEntry& pe = proj[fi];
        if (!pe.has_extent) continue;
        // 只扫该 feature 覆盖的行——否则 N 个 feature × 全部行会白跑。
        const auto y0 = static_cast<std::int64_t>(std::floor(pe.ymin));
        const auto y1 = static_cast<std::int64_t>(std::ceil(pe.ymax)) + 1;
        for (std::int64_t y = std::max<std::int64_t>(0, y0);
             y < y1 && y < static_cast<std::int64_t>(SPH); ++y) {
            const double sample_y = static_cast<double>(y) + 0.5;
            row_crossings(pe.rings, sample_y, xs);
            const std::size_t base = static_cast<std::size_t>(y) * SPW;
            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                const double x0 = std::max(0.0, xs[k]);
                const double x1 = std::min(static_cast<double>(SPW), xs[k + 1]);
                if (!(x1 > x0)) continue;
                const auto c0 = static_cast<std::uint32_t>(x0);
                const auto c1 = std::min(SPW, static_cast<std::uint32_t>(std::ceil(x1)));
                for (std::uint32_t c = c0; c < c1; ++c) {
                    const double l = std::max(x0, static_cast<double>(c));
                    const double r = std::min(x1, static_cast<double>(c) + 1.0);
                    if (!(r > l)) continue;
                    const std::size_t idx = base + c;
                    owner[idx] = static_cast<std::int32_t>(fi);
                    cover[idx] = std::max(cover[idx], static_cast<float>(r - l));
                }
            }
        }
    }

    // 4) 收口：每格取"覆盖面积最大的 feature"取色，亮位由覆盖率过阈值的子像素决定。
    //    子像素数最多 8（盲文），所以归属统计用固定小数组，不分配。
    for (std::uint32_t cy = 0; cy < rows; ++cy) {
        for (std::uint32_t cx = 0; cx < cols; ++cx) {
            std::int32_t acc_owner[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
            double       acc_area[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
            int          n_acc = 0;
            std::uint8_t bits = 0;
            std::int32_t distinct = -1;  // 用于判断该格是否跨区域（画边界）
            bool         multi = false;

            for (std::uint32_t sy = 0; sy < sh; ++sy) {
                for (std::uint32_t sx = 0; sx < sw; ++sx) {
                    const std::uint32_t gx = cx * sw + sx;
                    const std::uint32_t gy = cy * sh + sy;
                    const std::size_t   idx = static_cast<std::size_t>(gy) * SPW + gx;
                    const std::int32_t  o = owner[idx];
                    const double        c = static_cast<double>(cover[idx]);
                    if (o < 0 || c < opts.coverage) continue;

                    bits = static_cast<std::uint8_t>(bits | subpixel_bit(opts.style, sx, sy));
                    if (distinct < 0) distinct = o;
                    else if (distinct != o) multi = true;

                    int slot = -1;
                    for (int i = 0; i < n_acc; ++i) {
                        if (acc_owner[i] == o) { slot = i; break; }
                    }
                    if (slot < 0 && n_acc < 8) {
                        slot = n_acc++;
                        acc_owner[slot] = o;
                    }
                    if (slot >= 0) acc_area[slot] += c;
                }
            }

            if (bits == 0) continue;  // 空格：保持 CellBuffer 默认

            // 先比图层（后叠加的层优先），同层再比覆盖面积。这样"叠加层"（比如
            // 城市气泡）只要压到这一格就能上屏，不会被底图的整格覆盖面积压掉。
            int best = 0;
            for (int i = 1; i < n_acc; ++i) {
                const int li = lay[static_cast<std::size_t>(acc_owner[i])];
                const int lb = lay[static_cast<std::size_t>(acc_owner[best])];
                if (li != lb ? li > lb : acc_area[i] > acc_area[best]) best = i;
            }
            Color fg = colors[static_cast<std::size_t>(acc_owner[best])];
            if (multi && opts.draw_borders) {
                fg = opts.border;  // 跨区域的格子压暗，形成国界/省界
            }
            out.at(cx, cy) = Cell{raster_char(opts.style, bits), Style{}.set_fg(fg)};
        }
    }

    return out;
}

// ── 高清出口 3：真彩色位图 ─────────────────────────────────────────────────

// 在 (x0..x1, y0..y1) 矩形内画一条纵向分级色标：顶部 = 高值 bin，底部 = 低值 bin。
// 与 legend_bins() 共用同一套 bins/色带，保证字符格图例与位图色标一致。
static void draw_geo_colorbar(image::Bitmap& bmp, double x0, double x1, double y0, double y1,
                             int bins, color::ColormapKind kind) {
    const int W = static_cast<int>(bmp.width());
    const int H = static_cast<int>(bmp.height());
    const int ix0 = std::max(0, static_cast<int>(std::ceil(x0)));
    const int ix1 = std::min(W - 1, static_cast<int>(std::floor(x1)));
    const int iy0 = std::max(0, static_cast<int>(std::ceil(y0)));
    const int iy1 = std::min(H - 1, static_cast<int>(std::floor(y1)));
    if (ix0 > ix1 || iy0 > iy1) return;
    const double span = y1 - y0;
    for (int py = iy0; py <= iy1; ++py) {
        const double f = (y1 - static_cast<double>(py)) / span;  // 顶=1 底=0
        int bin = static_cast<int>(std::floor(f * static_cast<double>(bins)));
        if (bin < 0) bin = 0;
        if (bin > bins - 1) bin = bins - 1;
        const double t = (bins <= 1) ? 0.5
                                    : (static_cast<double>(bin) + 0.5) / static_cast<double>(bins);
        const Rgb c = color::sample(kind, t);
        for (int px = ix0; px <= ix1; ++px) {
            bmp.set(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), 0, c.r);
            bmp.set(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), 1, c.g);
            bmp.set(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), 2, c.b);
            bmp.set(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), 3, 255);
        }
    }
}

image::Bitmap GeoMap::render_bitmap(std::uint32_t width, std::uint32_t height,
                                    const GeoBitmapOptions& opts) const {
    image::Bitmap bmp(width, height, 4);
    if (width == 0 || height == 0) return bmp;

    // 背景铺满（含 alpha=255）。
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            bmp.set(x, y, 0, opts.background.r);
            bmp.set(x, y, 1, opts.background.g);
            bmp.set(x, y, 2, opts.background.b);
            bmp.set(x, y, 3, 255);
        }
    }
    if (!impl_->loaded || impl_->entries.empty()) return bmp;

    const double margin = std::max(0.0, opts.margin);
    // 开启图例时右侧预留一条色标带，地图主体缩进，避免与色标重叠。
    const double legend_w = opts.draw_legend ? 28.0 : 0.0;
    const double inner_w = static_cast<double>(width) - 2.0 * margin - legend_w;
    const double inner_h = static_cast<double>(height) - 2.0 * margin;
    if (!(inner_w > 1.0) || !(inner_h > 1.0)) return bmp;

    GridFit fit(options_.projection, options_);
    for (const auto& e : impl_->entries) {
        for (const auto& ring : e.rings) {
            for (const auto& p : ring) fit.add(p.y, p.x);
        }
    }
    if (!fit.valid()) return bmp;
    fit.fit(inner_w, inner_h);

    double vmin = 0.0, vmax = 0.0;
    bool have = false;
    for (const auto& e : impl_->entries) {
        if (!e.has_value) continue;
        if (!have) { vmin = vmax = e.value; have = true; }
        else { vmin = std::min(vmin, e.value); vmax = std::max(vmax, e.value); }
    }

    // 逐层合成：同层内每像素累积"颜色 × 覆盖率"并归一化（相邻区域在边界处自然
    // 按覆盖率混合，与同层内的绘制顺序无关）；层与层之间是 source-over ——
    // 后叠加的层盖在底图上，这正是"叠加图层"的语义。
    const std::size_t npix = static_cast<std::size_t>(width) * height;
    std::vector<float> acc_r(npix, 0.0f), acc_g(npix, 0.0f), acc_b(npix, 0.0f);
    std::vector<float> acc_a(npix, 0.0f);
    std::vector<std::uint8_t> edge(npix, 0);

    std::vector<double> xs;
    const double ox = margin;
    const double oy = margin;
    const std::uint32_t ss = std::max<std::uint32_t>(1, opts.supersample);
    const float ss_inv = 1.0f / static_cast<float>(ss);

    const Rgb& bd = opts.border;
    const bool want_edge = !(bd.r == opts.background.r && bd.g == opts.background.g &&
                             bd.b == opts.background.b);

    int max_layer = 0;
    for (const auto& e : impl_->entries) max_layer = std::max(max_layer, e.layer);

    for (int layer = 0; layer <= max_layer; ++layer) {
        std::fill(acc_r.begin(), acc_r.end(), 0.0f);
        std::fill(acc_g.begin(), acc_g.end(), 0.0f);
        std::fill(acc_b.begin(), acc_b.end(), 0.0f);
        std::fill(acc_a.begin(), acc_a.end(), 0.0f);
        std::fill(edge.begin(), edge.end(), std::uint8_t{0});

        for (const auto& e : impl_->entries) {
            if (e.layer != layer) continue;
            const Rgb c = value_color(e, have, vmin, vmax,
                                  static_cast<int>(options_.bins), opts.colormap, opts.no_data);
            const float fr = static_cast<float>(c.r) / 255.0f;
            const float fg = static_cast<float>(c.g) / 255.0f;
            const float fb = static_cast<float>(c.b) / 255.0f;

            // 投影到像素空间，并记住该 feature 的行范围（只扫这段行，避免 N×H 白跑）。
            std::vector<std::vector<Point>> rings;
            rings.reserve(e.rings.size());
            double ymin = 0.0, ymax = 0.0;
            bool   extent = false;
            for (const auto& ring : e.rings) {
                std::vector<Point> pr;
                pr.reserve(ring.size());
                for (const auto& p : ring) {
                    const Point g = fit.to_grid(p.y, p.x);
                    if (g.x <= GridFit::kOff) continue;  // 正射半球外
                    const Point q{g.x + ox, g.y + oy};
                    if (!extent) { ymin = ymax = q.y; extent = true; }
                    else { ymin = std::min(ymin, q.y); ymax = std::max(ymax, q.y); }
                    pr.push_back(q);
                }
                rings.push_back(std::move(pr));
            }
            if (!extent) continue;

            const auto y0 = static_cast<std::int64_t>(std::floor(ymin));
            const auto y1 = static_cast<std::int64_t>(std::ceil(ymax)) + 1;
            for (std::int64_t py = std::max<std::int64_t>(0, y0);
                 py < y1 && py < static_cast<std::int64_t>(height); ++py) {
                const std::size_t base = static_cast<std::size_t>(py) * width;
                // 纵向超采样：每像素行取 ss 条采样线，覆盖率按 ss 归一。
                for (std::uint32_t sub = 0; sub < ss; ++sub) {
                    const double sample_y =
                        static_cast<double>(py) +
                        (static_cast<double>(sub) + 0.5) / static_cast<double>(ss);
                    row_crossings(rings, sample_y, xs);
                    for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                        const double x0 = std::max(0.0, xs[k]);
                        const double x1 = std::min(static_cast<double>(width), xs[k + 1]);
                        if (!(x1 > x0)) continue;
                        const auto c0 = static_cast<std::uint32_t>(x0);
                        const auto c1 =
                            std::min(width, static_cast<std::uint32_t>(std::ceil(x1)));
                        for (std::uint32_t px = c0; px < c1; ++px) {
                            const double l = std::max(x0, static_cast<double>(px));
                            const double r = std::min(x1, static_cast<double>(px) + 1.0);
                            if (!(r > l)) continue;
                            const float a = static_cast<float>(r - l) * ss_inv;
                            const std::size_t idx = base + px;
                            acc_r[idx] += fr * a;
                            acc_g[idx] += fg * a;
                            acc_b[idx] += fb * a;
                            acc_a[idx] += a;
                        }
                    }
                }
            }
        }

        // 边缘：相邻像素覆盖率突变处压一条描边（区域界线），先标出来再统一上色。
        if (want_edge) {
            for (std::uint32_t py = 0; py < height; ++py) {
                for (std::uint32_t px = 0; px < width; ++px) {
                    const std::size_t idx = static_cast<std::size_t>(py) * width + px;
                    const float a = acc_a[idx];
                    bool boundary = false;
                    if (px + 1 < width && std::abs(a - acc_a[idx + 1]) > 0.5f) boundary = true;
                    if (py + 1 < height &&
                        std::abs(a - acc_a[idx + static_cast<std::size_t>(width)]) > 0.5f) {
                        boundary = true;
                    }
                    if (boundary) edge[idx] = 1;
                }
            }
        }

        // 把本层合成到已画好的结果上（source-over）。
        for (std::uint32_t py = 0; py < height; ++py) {
            for (std::uint32_t px = 0; px < width; ++px) {
                const std::size_t idx = static_cast<std::size_t>(py) * width + px;
                if (edge[idx] != 0) {
                    bmp.set(px, py, 0, bd.r);
                    bmp.set(px, py, 1, bd.g);
                    bmp.set(px, py, 2, bd.b);
                    continue;
                }
                const float raw = acc_a[idx];
                if (raw <= 0.0f) continue;  // 本层没覆盖到这里：保留下层/背景
                const float a = std::min(1.0f, raw);
                const float inv = 1.0f / raw;
                auto over = [&](std::uint8_t ch, float v) {
                    const float dst = static_cast<float>(bmp.at(px, py, ch)) / 255.0f;
                    const float col = v * inv;  // 覆盖率归一后的纯色
                    const float out = dst * (1.0f - a) + col * a;
                    return static_cast<std::uint8_t>(
                        std::clamp(out * 255.0f, 0.0f, 255.0f));
                };
                bmp.set(px, py, 0, over(0, acc_r[idx]));
                bmp.set(px, py, 1, over(1, acc_g[idx]));
                bmp.set(px, py, 2, over(2, acc_b[idx]));
            }
        }
    }

    // 右侧分级色标：纵向，高值在上；与 legend_bins() 共用同一套 bins/色带。
    if (opts.draw_legend && have && (vmax > vmin)) {
        const double bx0 = static_cast<double>(width) - margin - 28.0;
        const double bx1 = static_cast<double>(width) - margin;
        draw_geo_colorbar(bmp, bx0, bx1, margin,
                          static_cast<double>(height) - margin,
                          std::min(8, std::max(1, static_cast<int>(options_.bins))),
                          opts.colormap);
    }

    return bmp;
}

std::vector<layout::LegendEntry> GeoMap::legend_bins(color::ColormapKind kind) const {
    std::vector<layout::LegendEntry> out;
    double vmin = 0.0, vmax = 0.0;
    bool have = false;
    for (const auto& kv : values_) {
        if (!have) { vmin = vmax = kv.second; have = true; }
        else { vmin = std::min(vmin, kv.second); vmax = std::max(vmax, kv.second); }
    }
    if (!have || !(vmax > vmin)) return out;  // 无值或值域退化 -> 空图例

    const int bins = std::min(8, std::max(1, static_cast<int>(options_.bins)));
    out.reserve(static_cast<std::size_t>(bins));
    const double span = vmax - vmin;
    for (int i = 0; i < bins; ++i) {
        const double lo = vmin + span * static_cast<double>(i) / bins;
        const double hi = vmin + span * static_cast<double>(i + 1) / bins;
        const double t = (bins <= 1) ? 0.5
                                    : (static_cast<double>(i) + 0.5) / static_cast<double>(bins);
        const Rgb c = color::sample(kind, t);
        layout::LegendEntry e;
        e.name = std::string(layout::format_tick(lo)) + "\u2013" +
                 std::string(layout::format_tick(hi));
        e.color = Color::rgb(c.r, c.g, c.b);
        out.push_back(std::move(e));
    }
    return out;
}

CellBuffer GeoMap::render_legend(color::ColormapKind kind, std::uint32_t max_width_cells) const {
    const auto entries = legend_bins(kind);
    CellBuffer out;
    if (entries.empty()) return out;
    const std::uint32_t rows = static_cast<std::uint32_t>(entries.size()) + 2;
    canvas::Canvas c(max_width_cells, rows);
    const Rect rect{0.0, 0.0, static_cast<double>(max_width_cells),
                    static_cast<double>(rows)};
    layout::draw_legend(c, entries, rect, layout::HAlign::Left);
    return c.to_buffer();
}

// even-odd 射线法：点 (lon,lat) 是否在环集内（外环 + 洞一起算，holes 自动排除）。
// 环点按 (x=lon, y=lat) 存储，与 draw/rasterize 投影前的原始坐标一致。
static bool point_in_rings(double lon, double lat, const std::vector<std::vector<Point>>& rings) {
    int crossings = 0;
    for (const auto& ring : rings) {
        const std::size_t n = ring.size();
        if (n < 3) continue;
        for (std::size_t i = 0; i < n; ++i) {
            const Point a = ring[i];
            const Point b = ring[(i + 1) % n];
            if ((a.y > lat) != (b.y > lat)) {  // 边跨越水平线 lat
                const double x = a.x + (b.x - a.x) * (lat - a.y) / (b.y - a.y);
                if (x > lon) ++crossings;
            }
        }
    }
    return (crossings % 2) == 1;
}

std::string GeoMap::hit_test(double lon, double lat) const {
    if (!impl_ || !impl_->loaded) return std::string{};
    std::string best;
    int best_layer = std::numeric_limits<int>::min();
    for (const auto& e : impl_->entries) {
        if (point_in_rings(lon, lat, e.rings)) {
            if (e.layer >= best_layer) {  // 最上层优先；同层取最后命中者
                best_layer = e.layer;
                best = e.name;
            }
        }
    }
    return best;
}

}  // namespace termchart::chart
