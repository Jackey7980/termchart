// ttf_font.cpp —— 自主 TTF 解析实现（sfnt / cmap / loca / glyf / hmtx / hhea）
//
// sfnt 全大端。load 时整份数据拷进 Impl，各表指针直接指进去（调用方释放原
// 缓冲无影响）。所有读取做边界检查：畸形字体返回 ParseError，不越界读。
#include "termchart/image/ttf_font.hpp"
#include "termchart/math/interpolate.hpp"  // 二次贝塞尔求值（与 canvas/Path 共用唯一真源）

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace termchart::image {
namespace {

// ── 大端读取 ────────────────────────────────────────────────────────────────
std::uint16_t u16at(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}
std::int16_t i16at(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(u16at(p));
}
std::uint32_t u32at(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

// 像素空间二维点（y 向下为正）。解析阶段为字体单位（y 向上），光栅化前翻转。
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// 标准化轮廓：闭合二次贝塞尔序列。on_off = [on, ctrl, on, ctrl, on, ...]，
// 长度奇数；每 (ctrl[i], on[i+1]) 相对 on[i-1] 组成一段二次曲线。
struct Contour {
    std::vector<Vec2> on_off;
};

// ── cmap ────────────────────────────────────────────────────────────────────
using Cmap = std::unordered_map<char32_t, std::uint16_t>;

bool parse_cmap4(const std::uint8_t* p, std::size_t len, Cmap& out) noexcept {
    if (len < 16) {
        return false;
    }
    const std::uint16_t seg_x2 = u16at(p + 6);
    if (seg_x2 == 0 || seg_x2 % 2 != 0) {
        return false;
    }
    const std::size_t seg_count = seg_x2 / 2;
    if (16 + static_cast<std::size_t>(seg_x2) * 4 > len) {
        return false;
    }
    const std::uint8_t* end_codes = p + 14;
    const std::uint8_t* start_codes = end_codes + seg_x2 + 2;
    const std::uint8_t* id_deltas = start_codes + seg_x2;
    const std::uint8_t* id_range_offsets = id_deltas + seg_x2;
    for (std::size_t s = 0; s < seg_count; ++s) {
        const std::uint32_t start = u16at(start_codes + s * 2);
        const std::uint32_t end = u16at(end_codes + s * 2);
        const std::int32_t delta = i16at(id_deltas + s * 2);
        const std::uint16_t range_offset = u16at(id_range_offsets + s * 2);
        if (start > end) {
            return false;
        }
        for (std::uint32_t c = start; c <= end; ++c) {
            std::uint32_t glyph = 0;
            if (range_offset == 0) {
                glyph = (c + static_cast<std::uint32_t>(delta)) & 0xFFFFu;
            } else {
                // 字槽地址 = &idRangeOffset[s] + range_offset + 2*(c - start)。
                const std::size_t slot = static_cast<std::size_t>(range_offset) / 2 +
                                         (c - start) + s;
                const std::size_t byte =
                    static_cast<std::size_t>(id_range_offsets - p) + slot * 2;
                if (byte + 2 > len) {
                    return false;
                }
                glyph = u16at(p + byte);
                if (glyph != 0) {
                    glyph = (glyph + static_cast<std::uint32_t>(delta)) & 0xFFFFu;
                }
            }
            if (glyph != 0) {
                out[static_cast<char32_t>(c)] = static_cast<std::uint16_t>(glyph);
            }
        }
    }
    return true;
}

bool parse_cmap12(const std::uint8_t* p, std::size_t len, Cmap& out) noexcept {
    if (len < 16) {
        return false;
    }
    const std::uint32_t n_groups = u32at(p + 12);
    if (16 + static_cast<std::uint64_t>(n_groups) * 12 > len) {
        return false;
    }
    for (std::uint32_t g = 0; g < n_groups; ++g) {
        const std::uint8_t* e = p + 16 + static_cast<std::uint64_t>(g) * 12;
        const std::uint32_t start = u32at(e);
        const std::uint32_t end = u32at(e + 4);
        const std::uint32_t start_glyph = u32at(e + 8);
        if (start > end) {
            return false;
        }
        for (std::uint64_t c = start; c <= end; ++c) {
            const std::uint64_t glyph = static_cast<std::uint64_t>(start_glyph) + (c - start);
            if (glyph > 0xFFFFu) {
                break;
            }
            if (glyph != 0) {
                out[static_cast<char32_t>(c)] = static_cast<std::uint16_t>(glyph);
            }
        }
    }
    return true;
}

// 子表选择：(3,10) > (3,1) > (0,x) > 任意 format 12 > 任意 format 4。
bool parse_cmap_table(const std::uint8_t* p, std::size_t len, Cmap& out) noexcept {
    if (len < 4) {
        return false;
    }
    const std::uint16_t count = u16at(p + 2);
    if (4 + static_cast<std::size_t>(count) * 8 > len) {
        return false;
    }
    std::int32_t best_score = -1;
    std::uint32_t best_off = 0;
    std::uint16_t best_fmt = 0;
    std::int32_t any12 = -1;
    std::uint32_t any12_off = 0;
    std::int32_t any4 = -1;
    std::uint32_t any4_off = 0;
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::uint8_t* rec = p + 4 + static_cast<std::size_t>(i) * 8;
        const std::uint16_t platform = u16at(rec);
        const std::uint16_t encoding = u16at(rec + 2);
        const std::uint32_t offset = u32at(rec + 4);
        if (static_cast<std::uint64_t>(offset) + 2 > len) {
            continue;
        }
        const std::uint16_t format = u16at(p + offset);
        std::int32_t score = -1;
        if (platform == 3 && encoding == 10) {
            score = 4;
        } else if (platform == 3 && encoding == 1) {
            score = 3;
        } else if (platform == 0) {
            score = 2;
        }
        if (format == 12) {
            if (any12 < 0) {
                any12 = 1;
                any12_off = offset;
            }
            if (score > best_score) {
                best_score = score;
                best_off = offset;
                best_fmt = 12;
            }
        } else if (format == 4) {
            if (any4 < 0) {
                any4 = 1;
                any4_off = offset;
            }
            if (score > best_score) {
                best_score = score;
                best_off = offset;
                best_fmt = 4;
            }
        }
    }
    if (best_score >= 0) {
        return best_fmt == 12 ? parse_cmap12(p + best_off, len - best_off, out)
                              : parse_cmap4(p + best_off, len - best_off, out);
    }
    if (any12 >= 0) {
        return parse_cmap12(p + any12_off, len - any12_off, out);
    }
    if (any4 >= 0) {
        return parse_cmap4(p + any4_off, len - any4_off, out);
    }
    return false;
}

// ── glyf：简单轮廓 ───────────────────────────────────────────────────────────
// 解析到"标准二次贝塞尔序列"（隐中点补齐）。cursor 前进到轮廓数据末尾。
bool load_simple_glyph(const std::uint8_t* p, std::size_t len, std::size_t& cursor,
                       std::vector<Contour>& out) noexcept {
    if (cursor + 10 > len) {
        return false;
    }
    const std::int16_t num_contours = i16at(p + cursor);
    if (num_contours < 0) {
        return false;  // 复合轮廓不走这里
    }
    const auto nc = static_cast<std::size_t>(num_contours);
    if (nc == 0) {
        cursor += 10;
        return true;  // 空轮廓（SPACE / .notdef 常见）
    }
    const std::size_t end_pts_begin = cursor + 10;
    if (end_pts_begin + nc * 2 + 2 > len) {
        return false;
    }
    const std::uint32_t total =
        static_cast<std::uint32_t>(u16at(p + end_pts_begin + (nc - 1) * 2)) + 1;
    if (total == 0 || total > 65535u) {
        return false;
    }
    const std::uint16_t instr_len = u16at(p + end_pts_begin + nc * 2);
    std::size_t i = end_pts_begin + nc * 2 + 2 + instr_len;
    if (i > len) {
        return false;
    }
    // flags（含 REPEAT 展开）；i 随后前移到坐标区起点。
    std::vector<std::uint8_t> flags;
    flags.reserve(total);
    while (flags.size() < total) {
        if (i >= len) {
            return false;
        }
        const std::uint8_t f = p[i++];
        flags.push_back(f);
        if ((f & 0x08u) != 0) {  // REPEAT
            if (i >= len) {
                return false;
            }
            const std::uint8_t rep = p[i++];
            for (std::uint16_t r = 0; r < rep && flags.size() < total; ++r) {
                flags.push_back(f);
            }
        }
    }
    if (flags.size() != total) {
        return false;
    }
    std::vector<std::int32_t> xs(total, 0);
    std::vector<std::int32_t> ys(total, 0);
    std::int32_t x = 0;
    for (std::uint32_t k = 0; k < total; ++k) {
        const std::uint8_t f = flags[k];
        if ((f & 0x02u) != 0) {  // x 短向量
            if (i >= len) {
                return false;
            }
            const int dx = p[i++];
            x += (f & 0x10u) != 0 ? dx : -dx;
        } else if ((f & 0x10u) == 0) {  // x 长向量（2 字节）
            if (i + 2 > len) {
                return false;
            }
            x += i16at(p + i);
            i += 2;
        }
        xs[k] = x;
    }
    std::int32_t y = 0;
    for (std::uint32_t k = 0; k < total; ++k) {
        const std::uint8_t f = flags[k];
        if ((f & 0x04u) != 0) {  // y 短向量
            if (i >= len) {
                return false;
            }
            const int dy = p[i++];
            y += (f & 0x20u) != 0 ? dy : -dy;
        } else if ((f & 0x20u) == 0) {  // y 长向量（2 字节）
            if (i + 2 > len) {
                return false;
            }
            y += i16at(p + i);
            i += 2;
        }
        ys[k] = y;
    }
    cursor = i;

    std::size_t begin = 0;
    for (std::size_t c = 0; c < nc; ++c) {
        const std::size_t end = static_cast<std::size_t>(u16at(p + end_pts_begin + c * 2));
        if (end + 1 < begin || end + 1 > total) {
            return false;
        }
        const std::size_t n = end + 1 - begin;
        const std::size_t base = begin;
        const auto on = [&](std::size_t k) { return (flags[base + k % n] & 0x01u) != 0; };
        const auto px = [&](std::size_t k) { return static_cast<double>(xs[base + k % n]); };
        const auto py = [&](std::size_t k) { return static_cast<double>(ys[base + k % n]); };

        // 起点取 on 点（首/末都 off 时取二者隐中点）。
        // 之后从起点顺时针走"候选点"游标：候选是 on → 直线段；是 off → 以它为
        // 控制点、终点取下一 on 点或两个 off 间的隐中点；游标绕回起点即收。
        // 注：连续 on 点也必须成段——纯直线轮廓（方形、.notdef 框）全靠它。
        Vec2 start{};
        std::size_t start_c = 0;
        if (on(0)) {
            start = {px(0), py(0)};
            start_c = 1;
        } else if (on(n - 1)) {
            start = {px(n - 1), py(n - 1)};
            start_c = 0;
        } else {
            start = {(px(n - 1) + px(0)) / 2.0, (py(n - 1) + py(0)) / 2.0};
            start_c = 0;
        }
        Contour contour;
        contour.on_off.push_back(start);
        Vec2 prev_on = start;
        std::size_t walk = start_c;
        for (std::size_t guard = 0; guard <= n; ++guard) {
            const std::size_t j = walk % n;
            if (j == start_c && guard > 0) {
                break;  // 绕回起点候选：轮廓闭合
            }
            if (on(j)) {
                contour.on_off.push_back({px(j), py(j)});  // 退化二次曲线 = 直线
                contour.on_off.push_back({px(j), py(j)});
                prev_on = {px(j), py(j)};
                ++walk;
                continue;
            }
            const std::size_t k = (j + 1) % n;
            Vec2 segment_end{};
            if (on(k)) {
                segment_end = {px(k), py(k)};
            } else {
                segment_end = {(px(j) + px(k)) / 2.0, (py(j) + py(k)) / 2.0};
            }
            contour.on_off.push_back({px(j), py(j)});  // 控制点
            contour.on_off.push_back(segment_end);     // 终点
            prev_on = segment_end;
            walk = on(k) ? k + 1 : k;  // off(k) 留作下一段控制点
        }
        (void)prev_on;
        // 单点/两点轮廓画不出东西（至少需要一段）。
        if (contour.on_off.size() >= 3) {
            out.push_back(std::move(contour));
        }
        begin = end + 1;
    }
    return true;
}

// 分量变换叠加：子轮廓经 (a b; c d) 线性变换 + (dx, dy) 平移后并入 out。
void append_transformed(const std::vector<Contour>& child, double dx, double dy, double a,
                        double b, double c, double d, std::vector<Contour>& out) noexcept {
    for (const Contour& contour : child) {
        Contour moved;
        moved.on_off.reserve(contour.on_off.size());
        for (const Vec2& v : contour.on_off) {
            moved.on_off.push_back({a * v.x + c * v.y + dx, b * v.x + d * v.y + dy});
        }
        out.push_back(std::move(moved));
    }
}

}  // namespace

}  // namespace termchart::image

namespace termchart::image {

// ── Impl ────────────────────────────────────────────────────────────────────
struct TtfFont::Impl {
    std::vector<std::uint8_t> data;
    std::uint32_t sfnt_base = 0;
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> tables;
    Cmap cmap;
    std::vector<std::uint16_t> advances;  // hmtx（按 numGlyphs 补齐后）
    double units_per_em = 1000.0;
    double ascent = 0.0;
    double descent = 0.0;
    double line_gap = 0.0;
    std::uint16_t num_glyphs = 0;
    bool loca_long = false;
    std::unordered_map<std::uint64_t, TtfGlyph> cache;

    // 取表体指针（越界声明返回 nullptr）；length 出参。
    [[nodiscard]] const std::uint8_t* table_ptr(std::uint32_t tag, std::uint32_t& length)
        const noexcept {
        const auto it = tables.find(tag);
        if (it == tables.end()) {
            return nullptr;
        }
        const std::uint64_t begin = static_cast<std::uint64_t>(sfnt_base) + it->second.first;
        const std::uint64_t end = begin + it->second.second;
        if (begin > data.size() || end > data.size()) {
            return nullptr;
        }
        length = it->second.second;
        return data.data() + begin;
    }

    [[nodiscard]] double scale(double pixel_size) const noexcept {
        if (pixel_size <= 0.0 || units_per_em <= 0.0) {
            return 0.0;
        }
        return pixel_size / units_per_em;
    }

    [[nodiscard]] std::uint16_t glyph_for(char32_t cp) const noexcept {
        const auto it = cmap.find(cp);
        return it != cmap.end() ? it->second : static_cast<std::uint16_t>(0);
    }

    // 0.25px 量化：更细的尺寸差对渲染无意义，纯省 cache。
    [[nodiscard]] static std::uint32_t quantize(double pixel_size) noexcept {
        const double q = pixel_size * 4.0;
        if (!(q > 0.0)) {
            return 0;
        }
        return static_cast<std::uint32_t>(q + 0.5);
    }

    // glyf 轮廓（含复合展开）。失败：Unsupported（无 glyf）/ ParseError /
    // OutOfRange（下标越界或递归过深）。
    [[nodiscard]] ErrorCode outline(std::uint16_t glyph_index, std::vector<Contour>& out,
                                    int depth = 0) const noexcept {
        if (depth > 8 || glyph_index >= num_glyphs) {
            return ErrorCode::OutOfRange;
        }
        std::uint32_t loca_len = 0;
        const std::uint8_t* loca = table_ptr(0x6C6F6361u /* loca */, loca_len);
        std::uint32_t glyf_len = 0;
        const std::uint8_t* glyf = table_ptr(0x676C7966u /* glyf */, glyf_len);
        if (loca == nullptr || glyf == nullptr) {
            return ErrorCode::Unsupported;  // CFF 轮廓或表缺失
        }
        const std::size_t entry = loca_long ? 4u : 2u;
        if (static_cast<std::size_t>(glyph_index + 1) * entry > loca_len) {
            return ErrorCode::ParseError;
        }
        const std::uint8_t* loca_at = loca + static_cast<std::size_t>(glyph_index) * entry;
        std::uint64_t g0 = loca_long ? u32at(loca_at)
                                     : static_cast<std::uint64_t>(u16at(loca_at)) * 2;
        std::uint64_t g1 = loca_long ? u32at(loca_at + 4)
                                     : static_cast<std::uint64_t>(u16at(loca_at + 2)) * 2;
        if (g1 < g0 || g1 > glyf_len) {
            return ErrorCode::ParseError;
        }
        if (g0 == g1) {
            return ErrorCode::Ok;  // 空字形
        }
        std::size_t cursor = static_cast<std::size_t>(g0);
        if (cursor + 10 > glyf_len) {
            return ErrorCode::ParseError;
        }
        const std::int16_t num_contours = i16at(glyf + cursor);
        if (num_contours == 0) {
            return ErrorCode::Ok;  // 空字形（SPACE 等）——不是复合轮廓！
        }
        if (num_contours > 0) {
            return load_simple_glyph(glyf, glyf_len, cursor, out) ? ErrorCode::Ok
                                                                 : ErrorCode::ParseError;
        }

        // 复合轮廓：逐分量解析 + 变换叠加。MORE_COMPONENTS 循环，guard 兜底。
        cursor += 10;
        for (int guard = 0; guard < 512; ++guard) {
            if (cursor + 4 > glyf_len) {
                return ErrorCode::ParseError;
            }
            const std::uint16_t flags = u16at(glyf + cursor);
            const std::uint16_t child_index = u16at(glyf + cursor + 2);
            cursor += 4;
            const bool xy = (flags & 0x0002u) != 0;         // ARGS_ARE_XY_VALUES
            const bool long_args = (flags & 0x0001u) != 0;  // ARGS_LONG
            double dx = 0.0;
            double dy = 0.0;
            if (long_args) {
                if (cursor + 8 > glyf_len) {
                    return ErrorCode::ParseError;
                }
                dx = xy ? static_cast<double>(static_cast<std::int32_t>(u32at(glyf + cursor)))
                        : 0.0;
                dy = xy ? static_cast<double>(static_cast<std::int32_t>(u32at(glyf + cursor + 4)))
                        : 0.0;
                cursor += 8;
            } else {
                if (cursor + 4 > glyf_len) {
                    return ErrorCode::ParseError;
                }
                dx = xy ? static_cast<double>(i16at(glyf + cursor)) : 0.0;
                dy = xy ? static_cast<double>(i16at(glyf + cursor + 2)) : 0.0;
                cursor += 4;
            }
            // 非 XY 参数（点号匹配）极少用且实现代价大：按零偏移处理，
            // 其余分量照常叠加（宁可形变也不崩）。
            double m00 = 1.0;
            double m01 = 0.0;
            double m10 = 0.0;
            double m11 = 1.0;
            if ((flags & 0x0008u) != 0) {  // WE_HAVE_A_SCALE
                if (cursor + 2 > glyf_len) {
                    return ErrorCode::ParseError;
                }
                const double s = static_cast<double>(i16at(glyf + cursor)) / 16384.0;
                cursor += 2;
                m00 = m11 = s;
            } else if ((flags & 0x0040u) != 0) {  // X_AND_Y_SCALE
                if (cursor + 4 > glyf_len) {
                    return ErrorCode::ParseError;
                }
                m00 = static_cast<double>(i16at(glyf + cursor)) / 16384.0;
                m11 = static_cast<double>(i16at(glyf + cursor + 2)) / 16384.0;
                cursor += 4;
            } else if ((flags & 0x0080u) != 0) {  // TWO_BY_TWO
                if (cursor + 8 > glyf_len) {
                    return ErrorCode::ParseError;
                }
                m00 = static_cast<double>(i16at(glyf + cursor)) / 16384.0;
                m01 = static_cast<double>(i16at(glyf + cursor + 2)) / 16384.0;
                m10 = static_cast<double>(i16at(glyf + cursor + 4)) / 16384.0;
                m11 = static_cast<double>(i16at(glyf + cursor + 6)) / 16384.0;
                cursor += 8;
            }
            std::vector<Contour> child_contours;
            const ErrorCode ec = outline(child_index, child_contours, depth + 1);
            if (ec != ErrorCode::Ok) {
                return ec;
            }
            append_transformed(child_contours, dx, dy, m00, m01, m10, m11, out);
            if ((flags & 0x0020u) == 0) {  // MORE_COMPONENTS 清零即收尾
                break;
            }
        }
        return ErrorCode::Ok;
    }
};

TtfFont::TtfFont(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Result<TtfFont> TtfFont::load(const std::uint8_t* data, std::size_t size) noexcept {
    auto impl = std::make_shared<Impl>();
    impl->data.assign(data, data + size);

    // ── sfnt 目录（含 ttcf 解包）───────────────────────────────────────────
    {
        auto& d = impl->data;
        if (d.size() < 12) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        constexpr std::uint32_t kTrue = 0x00010000u;
        constexpr std::uint32_t kTrue2 = 0x74727565u;  // 'true'
        constexpr std::uint32_t kTtcf = 0x74746366u;   // 'ttcf'
        std::uint32_t base = 0;
        std::uint32_t num_tables = 0;
        const std::uint32_t tag = u32at(d.data());
        if (tag == kTrue || tag == kTrue2) {
            num_tables = u16at(d.data() + 4);
        } else if (tag == kTtcf) {
            if (d.size() < 16) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            base = u32at(d.data() + 12);
            if (static_cast<std::uint64_t>(base) + 12 > d.size()) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            num_tables = u16at(d.data() + base + 4);
        } else {
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);  // 'OTTO'/CFF 等
        }
        const std::uint64_t dir_begin = static_cast<std::uint64_t>(base) + 12;
        if (dir_begin + static_cast<std::uint64_t>(num_tables) * 16 > d.size()) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        for (std::uint32_t i = 0; i < num_tables; ++i) {
            const std::uint8_t* e = d.data() + dir_begin + static_cast<std::uint64_t>(i) * 16;
            impl->tables[u32at(e)] = {u32at(e + 8), u32at(e + 12)};
        }
        impl->sfnt_base = base;
    }

    // ── head ───────────────────────────────────────────────────────────────
    {
        std::uint32_t len = 0;
        const std::uint8_t* head = impl->table_ptr(0x68656164u /* head */, len);
        if (head == nullptr || len < 54) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        impl->units_per_em = u16at(head + 18);
        if (impl->units_per_em == 0) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        impl->loca_long = i16at(head + 50) != 0;
    }

    // ── maxp ───────────────────────────────────────────────────────────────
    {
        std::uint32_t len = 0;
        const std::uint8_t* maxp = impl->table_ptr(0x6D617870u /* maxp */, len);
        if (maxp == nullptr || len < 6) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        impl->num_glyphs = u16at(maxp + 4);
    }

    // ── hhea / hmtx ────────────────────────────────────────────────────────
    {
        std::uint32_t len = 0;
        const std::uint8_t* hhea = impl->table_ptr(0x68686561u /* hhea */, len);
        if (hhea == nullptr || len < 36) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        impl->ascent = i16at(hhea + 4);
        impl->descent = i16at(hhea + 6);
        impl->line_gap = i16at(hhea + 8);
        const std::uint16_t n_metrics = u16at(hhea + 34);
        std::uint32_t hlen = 0;
        const std::uint8_t* hmtx = impl->table_ptr(0x686D7478u /* hmtx */, hlen);
        if (hmtx == nullptr || hlen < 4) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        const std::size_t rows = n_metrics == 0 ? 1 : n_metrics;
        if (hlen < rows * 4) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        impl->advances.clear();
        impl->advances.reserve(rows);
        for (std::size_t i = 0; i < rows; ++i) {
            impl->advances.push_back(u16at(hmtx + i * 4));
        }
        if (impl->num_glyphs > rows) {
            impl->advances.resize(impl->num_glyphs, impl->advances.back());
        }
    }

    // ── cmap ───────────────────────────────────────────────────────────────
    {
        std::uint32_t len = 0;
        const std::uint8_t* cmap = impl->table_ptr(0x636D6170u /* cmap */, len);
        if (cmap == nullptr || !parse_cmap_table(cmap, len, impl->cmap)) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
    }
    return TtfFont{std::move(impl)};
}

Result<TtfFont> TtfFont::load(std::string_view bytes) noexcept {
    return load(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

TtfMetrics TtfFont::metrics(double pixel_size) const noexcept {
    const double s = impl_->scale(pixel_size);
    return TtfMetrics{impl_->ascent * s, impl_->descent * s, impl_->line_gap * s};
}

double TtfFont::advance(char32_t codepoint, double pixel_size) const noexcept {
    const double s = impl_->scale(pixel_size);
    const std::uint16_t g = impl_->glyph_for(codepoint);
    if (g < impl_->advances.size()) {
        return static_cast<double>(impl_->advances[g]) * s;
    }
    return 0.0;
}

namespace {
// 把 glyf 轮廓（on_off = [on, ctrl, on, ctrl, ...] 二次贝塞尔序列）展平成多边形，
// 并转换到像素空间（y 向下）。与 glyph() 光栅化同款展平，保证轮廓与位图同形。
void flatten_glyph_contour(const Contour& c, double s, std::vector<Point>& out) {
    const std::size_t n = c.on_off.size();
    if (n < 3) {
        return;
    }
    const auto to_px = [s](const Vec2& v) { return Point{v.x * s, -v.y * s}; };
    out.push_back(to_px(c.on_off.front()));
    Vec2 cur = c.on_off.front();
    for (std::size_t k = 1; k + 1 < n; k += 2) {
        const Vec2 p0 = cur;
        const Vec2 p1 = c.on_off[k];
        const Vec2 p2 = c.on_off[k + 1];
        const double diag2 = (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y) +
                             (p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y);
        const int steps =
            std::clamp<int>(static_cast<int>(std::sqrt(diag2) * s / 8.0) + 2, 2, 32);
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            const Vec2 cp{math::quadratic_bezier(p0.x, p1.x, p2.x, t),
                          math::quadratic_bezier(p0.y, p1.y, p2.y, t)};
            out.push_back(to_px(cp));
        }
        cur = p2;
    }
}
}  // namespace

Result<GlyphOutline> TtfFont::glyph_outline(char32_t codepoint, double pixel_size) const noexcept {
    if (!(pixel_size > 0.0)) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const std::uint16_t g = impl_->glyph_for(codepoint);
    std::vector<Contour> contours;
    const ErrorCode ec = impl_->outline(g, contours);
    if (ec != ErrorCode::Ok) {
        return Unexpected<ErrorCode>(ec);
    }
    GlyphOutline out;
    const double s = impl_->scale(pixel_size);
    if (g < impl_->advances.size()) {
        out.advance_x = static_cast<double>(impl_->advances[g]) * s;
    }
    if (contours.empty()) {
        return out;  // 空字形（空格等）：无轮廓，advance 仍有效
    }
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    for (const Contour& c : contours) {
        std::vector<Point> poly;
        flatten_glyph_contour(c, s, poly);
        if (poly.size() < 3) {
            continue;
        }
        for (const Point& p : poly) {
            min_x = std::min(min_x, p.x);
            min_y = std::min(min_y, p.y);
        }
        out.contours.push_back(std::move(poly));
    }
    out.left = static_cast<std::int32_t>(std::floor(min_x)) - 1;
    out.top = static_cast<std::int32_t>(std::floor(min_y)) - 1;
    return out;
}

Result<TtfGlyph> TtfFont::glyph(char32_t codepoint, double pixel_size) noexcept {
    const std::uint64_t key = (static_cast<std::uint64_t>(codepoint) << 32) |
                              static_cast<std::uint64_t>(impl_->quantize(pixel_size));
    if (const auto it = impl_->cache.find(key); it != impl_->cache.end()) {
        return it->second;
    }

    const std::uint16_t g = impl_->glyph_for(codepoint);
    std::vector<Contour> contours;
    const ErrorCode ec = impl_->outline(g, contours);
    if (ec != ErrorCode::Ok) {
        return Unexpected<ErrorCode>(ec);
    }
    TtfGlyph entry;
    const double s = impl_->scale(pixel_size);
    if (g < impl_->advances.size()) {
        entry.advance_x = static_cast<double>(impl_->advances[g]) * s;
    }
    if (contours.empty()) {
        impl_->cache.emplace(key, entry);
        return impl_->cache.at(key);
    }

    // 轮廓 → 像素空间（y 翻转），外接框（含极值点）。
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    for (const Contour& c : contours) {
        for (const Vec2& v : c.on_off) {
            const double px = v.x * s;
            const double py = -v.y * s;
            min_x = std::min(min_x, px);
            min_y = std::min(min_y, py);
            max_x = std::max(max_x, px);
            max_y = std::max(max_y, py);
        }
    }
    // 控制点外的曲线极值最多外扩 1px；位图至少 1px。
    const std::int32_t bw = std::max(1, static_cast<std::int32_t>(std::floor(max_x - min_x)) + 2);
    const std::int32_t bh = std::max(1, static_cast<std::int32_t>(std::floor(max_y - min_y)) + 2);
    if (bw > 4096 || bh > 4096) {
        return Unexpected<ErrorCode>(ErrorCode::OutOfRange);  // 病态尺寸不渲染
    }
    entry.bitmap.resize(static_cast<std::uint32_t>(bw), static_cast<std::uint32_t>(bh), 4);
    entry.left = static_cast<std::int32_t>(std::floor(min_x)) - 1;
    entry.top = static_cast<std::int32_t>(std::floor(min_y)) - 1;

    // ── 光栅化：二次贝塞尔展平 → 扫描线非零环绕 → 4x4 超采样 → alpha ────────
    constexpr int kSuper = 4;
    struct Edge {
        double x0;
        double y0;
        double x1;
        double y1;
    };
    std::vector<Edge> edges;
    // 像素坐标 → 超采样坐标（左上角为 0,0；每像素 kSuper 个样本）。
    const auto to_ss = [&](Vec2 v) {
        return Vec2{(v.x * s - min_x + 1.0) * kSuper, (-v.y * s - min_y + 1.0) * kSuper};
    };
    auto add_edge = [&](Vec2 a, Vec2 b) {
        Vec2 sa = to_ss(a);
        Vec2 sb = to_ss(b);
        // 展平"直线段"时 (1-t)+t != 1 带来 1e-14 级抖动：近水平段会漏过水平
        // 剔除、近垂直段交点落在边界内侧一丝，本该整覆盖的像素出现一条
        // 63/255 的缝。近轴对齐的段两端共同吸附到中点旁的整数——展平前的真值
        // 必是网格对齐的整数（to_ss = 整数*4），抖动恢复它；真正的非整数位置
        // （亚像素 AA 边）不触发（|m - round(m)| 远大）。
        constexpr double kAxisEps = 1e-9;
        constexpr double kIntEps = 1e-6;
        if (std::abs(sa.x - sb.x) < kAxisEps) {
            const double m = (sa.x + sb.x) / 2.0;
            const double snapped = std::round(m);
            if (std::abs(m - snapped) < kIntEps) {
                sa.x = sb.x = snapped;
            }
        }
        if (std::abs(sa.y - sb.y) < kAxisEps) {
            const double m = (sa.y + sb.y) / 2.0;
            const double snapped = std::round(m);
            if (std::abs(m - snapped) < kIntEps) {
                sa.y = sb.y = snapped;
            }
        }
        if (sa.y == sb.y) {
            return;  // 水平边不贡献覆盖率
        }
        edges.push_back(Edge{sa.x, sa.y, sb.x, sb.y});
    };
    auto flatten_quad = [&](Vec2 p0, Vec2 p1, Vec2 p2) {
        const double len = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) +
                                     (p2.y - p1.y) * (p2.y - p1.y) +
                                     (p1.x - p0.x) * (p1.x - p0.x) +
                                     (p1.y - p0.y) * (p1.y - p0.y));
        const int steps = std::clamp<int>(static_cast<int>(len * s / 8.0) + 2, 2, 32);
        Vec2 prev = p0;
        for (int k = 1; k <= steps; ++k) {
            const double t = static_cast<double>(k) / steps;
            const Vec2 cur{math::quadratic_bezier(p0.x, p1.x, p2.x, t),
                           math::quadratic_bezier(p0.y, p1.y, p2.y, t)};
            add_edge(prev, cur);
            prev = cur;
        }
    };
    for (const Contour& c : contours) {
        const std::size_t n = c.on_off.size();
        if (n < 3) {
            continue;
        }
        const Vec2 start = c.on_off.front();
        Vec2 cur = start;
        for (std::size_t k = 1; k + 1 < n; k += 2) {
            flatten_quad(cur, c.on_off[k], c.on_off[k + 1]);
            cur = c.on_off[k + 1];
        }
        add_edge(cur, start);  // 闭合
    }

    const int sh = bh * kSuper;  // 超采样网格（行 = y，样本中心在 row + 0.5）
    const int sw = bw * kSuper;
    std::vector<double> acc(static_cast<std::size_t>(sh) * static_cast<std::size_t>(sw), 0.0);
    std::vector<double> xs;
    for (int row = 0; row < sh; ++row) {
        xs.clear();
        const double sy = static_cast<double>(row) + 0.5;
        for (const Edge& e : edges) {
            const double lo = std::min(e.y0, e.y1);
            const double hi = std::max(e.y0, e.y1);
            if (sy < lo || sy >= hi) {
                continue;
            }
            const double t = (sy - e.y0) / (e.y1 - e.y0);
            xs.push_back(e.x0 + (e.x1 - e.x0) * t);
        }
        if (xs.size() < 2) {
            continue;
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            int x0 = std::clamp(static_cast<int>(std::floor(xs[k])), 0, sw);
            int x1 = std::clamp(static_cast<int>(std::ceil(xs[k + 1])), 0, sw);
            for (int x = x0; x < x1; ++x) {
                acc[static_cast<std::size_t>(row) * static_cast<std::size_t>(sw) +
                    static_cast<std::size_t>(x)] += 1.0;
            }
        }
    }
    // 4x4 盒式降采样 → 8-bit alpha（覆盖整格才算 255）。
    for (std::int32_t y = 0; y < bh; ++y) {
        for (std::int32_t x = 0; x < bw; ++x) {
            double sum = 0.0;
            for (int sy2 = 0; sy2 < kSuper; ++sy2) {
                for (int sx = 0; sx < kSuper; ++sx) {
                    sum += acc[static_cast<std::size_t>(y * kSuper + sy2) *
                                   static_cast<std::size_t>(sw) +
                               static_cast<std::size_t>(x * kSuper + sx)];
                }
            }
            const double alpha = sum / static_cast<double>(kSuper * kSuper);
            const auto a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0, 1.0) * 255.0);
            entry.bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                             255);  // RGB 恒白，覆盖率走 alpha
            entry.bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                             255);
            entry.bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                             255);
            entry.bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 3,
                             a);
        }
    }
    impl_->cache.emplace(key, std::move(entry));
    return impl_->cache.at(key);
}

}  // namespace termchart::image
