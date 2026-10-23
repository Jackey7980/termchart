// showcase/catalog_util.hpp —— 全功能 catalog 的共享助手（header-only）
//
// 供 catalog_charts / catalog_graphs / catalog_systems / catalog_main 复用：
// UTF-8 文本绘制（正确处理宽字符续接格）、面板边框、经 canvas 渲染的统一入口，
// 以及一批确定性样本数据。
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/termchart_full.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"

namespace tc = termchart;

namespace cat {

using tc::CellBuffer;
using tc::Color;
using tc::Point;
using tc::Style;

// ── UTF-8 / 宽字符 ──────────────────────────────────────────────────────────
inline std::vector<char32_t> utf8_decode(std::string_view s) {
    std::vector<char32_t> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        int len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c >> 4) == 0xE) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c >> 3) == 0x1E) {
            cp = c & 0x07;
            len = 4;
        } else {
            cp = 0xFFFD;
        }
        for (int k = 1; k < len && i + static_cast<std::size_t>(k) < s.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3F);
        }
        out.push_back(cp);
        i += static_cast<std::size_t>(len);
    }
    return out;
}

inline bool is_wide(char32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x3000 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0x33FF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

inline void draw_text(CellBuffer& b, std::uint32_t x, std::uint32_t y, std::string_view s,
                      const Style& st) {
    std::uint32_t cx = x;
    for (const char32_t cp : utf8_decode(s)) {
        if (cp == U'\n') {
            ++y;
            cx = x;
            continue;
        }
        const std::uint32_t w = is_wide(cp) ? 2u : 1u;
        if (!b.contains(cx, y)) break;
        b.at(cx, y).cp = cp;
        b.at(cx, y).style = st;
        if (w == 2 && cx + 1 < b.width()) {
            b.at(cx + 1, y).cp = U'\0';
            b.at(cx + 1, y).style = st;
        }
        cx += w;
        if (cx >= b.width()) break;
    }
}

// 画边框 + 标题（标题可含中文）。
inline void frame(CellBuffer& p, std::string_view title, Color border, Color title_c) {
    const std::uint32_t w = p.width();
    const std::uint32_t h = p.height();
    if (w < 2 || h < 2) return;
    Style bs;
    bs.set_fg(border);
    auto put = [&](std::uint32_t x, std::uint32_t y, char32_t ch) {
        if (p.contains(x, y)) {
            p.at(x, y).cp = ch;
            p.at(x, y).style = bs;
        }
    };
    put(0, 0, U'┌');
    put(w - 1, 0, U'┐');
    put(0, h - 1, U'└');
    put(w - 1, h - 1, U'┘');
    for (std::uint32_t x = 1; x + 1 < w; ++x) {
        put(x, 0, U'─');
        put(x, h - 1, U'─');
    }
    for (std::uint32_t y = 1; y + 1 < h; ++y) {
        put(0, y, U'│');
        put(w - 1, y, U'│');
    }
    Style ts;
    ts.set_fg(title_c).set_attr(tc::Attr::Bold);
    draw_text(p, 2, 0, title, ts);
}

// 经 canvas 渲染一个面板：draw 画进 (w-2,h-2) 的内里画布，再套边框标题。
inline CellBuffer panel(std::uint32_t w, std::uint32_t h, std::string_view title,
                        const std::function<void(tc::canvas::Canvas&)>& draw, Color border,
                        Color title_c) {
    CellBuffer p(w, h);
    const std::uint32_t iw = (w > 2) ? w - 2 : w;
    const std::uint32_t ih = (h > 2) ? h - 2 : h;
    if (iw > 0 && ih > 0) {
        tc::canvas::Canvas cv(iw, ih);
        draw(cv);
        const CellBuffer inner = cv.to_buffer();
        for (std::uint32_t y = 0; y < ih && y < inner.height(); ++y) {
            for (std::uint32_t x = 0; x < iw && x < inner.width(); ++x) {
                p.at(1 + x, 1 + y) = inner.at(x, y);
            }
        }
    }
    frame(p, title, border, title_c);
    return p;
}

// 直接操作 CellBuffer 的面板（用于非 chart.draw 的自由函数渲染）。
inline CellBuffer panel_buf(std::uint32_t w, std::uint32_t h, std::string_view title,
                            const std::function<void(CellBuffer&)>& draw, Color border,
                            Color title_c) {
    CellBuffer p(w, h);
    CellBuffer inner((w > 2) ? w - 2 : w, (h > 2) ? h - 2 : h);
    draw(inner);
    for (std::uint32_t y = 0; y + 1 < h && y < inner.height(); ++y) {
        for (std::uint32_t x = 0; x + 1 < w && x < inner.width(); ++x) {
            p.at(1 + x, 1 + y) = inner.at(x, y);
        }
    }
    frame(p, title, border, title_c);
    return p;
}

// ── 确定性样本数据 ──────────────────────────────────────────────────────────
inline std::vector<double> ramp(double a, double b, std::size_t n) {
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = a + (b - a) * static_cast<double>(i) / static_cast<double>(n > 1 ? n - 1 : 1);
    }
    return v;
}

inline std::vector<Point> sine_pts(std::size_t n, double freq, double amp = 1.0) {
    std::vector<Point> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        v.push_back(Point{x, amp * std::sin(x * freq)});
    }
    return v;
}

}  // namespace cat
