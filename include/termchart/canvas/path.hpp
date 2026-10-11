// path.hpp —— 路径对象：构建 → 展平 → 描边/填充
//
// 路径只描述几何，不绑定样式与画布：先 add_* 建好，再交给 stroke()/fill()。
// 曲线（二次/三次贝塞尔、圆）统一用**自适应细分**展平成折线，容差以格为单位
// （默认 0.25 格）：容差越小越贴合，折线也越长。
//
// 圆用四段三次贝塞尔（kappa = 0.5523）表示，而不是固定分段数的多边形 ——
// 这样细分的密度由容差决定，放大缩小时都不会出现肉眼可见的多边形棱角。
#ifndef TERMCHART_CANVAS_PATH_HPP
#define TERMCHART_CANVAS_PATH_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/fill.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

enum class PathVerb : std::uint8_t {
    Move,   // a = 目标点
    Line,   // a = 目标点
    Quad,   // a = 控制点，b = 目标点
    Cubic,  // a = 控制点 1，b = 控制点 2，c = 目标点
    Close,  // 回到当前子路径起点
};

struct PathCommand {
    PathVerb verb = PathVerb::Move;
    Point a{};
    Point b{};
    Point c{};
};

class Path {
public:
    TERMCHART_API void move_to(Point p);
    TERMCHART_API void line_to(Point p);
    TERMCHART_API void quadratic_to(Point control, Point end);
    TERMCHART_API void cubic_to(Point c1, Point c2, Point end);
    TERMCHART_API void close();
    TERMCHART_API void add_rect(const Rect& rect);
    TERMCHART_API void add_circle(Point center, double radius);
    // 折线：自动 move_to + 逐点 line_to；closed 为真时闭合。
    TERMCHART_API void add_polyline(const std::vector<Point>& points, bool closed = false);

    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }
    [[nodiscard]] std::size_t command_count() const noexcept { return commands_.size(); }
    [[nodiscard]] const std::vector<PathCommand>& commands() const noexcept {
        return commands_;
    }

    // 展平：每个子路径一条折线（点列）。少于两个点的子路径被丢弃。
    [[nodiscard]] TERMCHART_API std::vector<std::vector<Point>> flatten(
        double tolerance = 0.25) const;

    // 保守包围盒：把曲线控制点也算进去，因此**一定**包住实际形状。
    [[nodiscard]] TERMCHART_API Rect bounds() const;

    // 描边：逐段画线。返回实际落在画布内的格数。
    [[nodiscard]] TERMCHART_API std::size_t stroke(Canvas& canvas, char32_t pen,
                                                   const Style& style = {},
                                                   double tolerance = 0.25) const;

    // 填充：对每个展平后的子路径做扫描线填充。返回写入的格数。
    [[nodiscard]] TERMCHART_API std::size_t fill(Canvas& canvas,
                                                 const FillOptions& opts = {},
                                                 double tolerance = 0.25) const;

private:
    std::vector<PathCommand> commands_;
};

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_PATH_HPP
