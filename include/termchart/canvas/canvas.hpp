// canvas.hpp —— 浮点坐标画布 → CellBuffer
//
// 终端实时路径的中段：
//   Scene →（scene_renderer）→ Canvas → to_buffer() → CellBuffer → render/renderer
//
// 与 raster/ 的分工（重要）：
//   * 本文件工作在**格**粒度：一个浮点坐标落到哪一格，就写那一格；
//   * 子像素（一格内 2×4 盲文点、半块上下双色）是 raster/ 的事——阶段 7 接入后
//     draw_line / fill_rect 会转而调用光栅器，本文件的对外接口**不变**。
// 因此这里的图元字符是"占位级"的（线用 '*'、填充用 █），不是最终观感。
#ifndef TERMCHART_CANVAS_CANVAS_HPP
#define TERMCHART_CANVAS_CANVAS_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

class Canvas {
public:
    Canvas() = default;
    TERMCHART_API Canvas(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();

    [[nodiscard]] std::uint32_t width() const noexcept { return buffer_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return buffer_.height(); }

    // 浮点坐标 → 格：向下取整（与 core/rect 的半开区间约定一致）。
    // 越界一律**裁剪**（丢弃），不折回边缘、不报错；返回值告诉你到底落没落格——
    // 调用方要统计"实际画了多少格"时不必自己再判一次边界。
    TERMCHART_API bool put(Point p, char32_t cp, const Style& style = Style{}) noexcept;
    TERMCHART_API bool put(std::uint32_t x, std::uint32_t y, char32_t cp,
                           const Style& style = Style{}) noexcept;

    TERMCHART_API void fill_rect(Rect rect, char32_t cp, const Style& style = Style{}) noexcept;
    TERMCHART_API void draw_line(Point from, Point to, char32_t cp,
                                 const Style& style = Style{}) noexcept;
    TERMCHART_API void draw_text(Point at, std::string_view text,
                                 const Style& style = Style{}) noexcept;

    // 读取（测试与 layout 布局计算用）。
    [[nodiscard]] TERMCHART_API const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept;

    // 交给渲染管线。返回副本：Canvas 与 CellBuffer 生命周期解耦。
    [[nodiscard]] TERMCHART_API CellBuffer to_buffer() const;

private:
    CellBuffer buffer_;
};

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_CANVAS_HPP
