// rasterizer.hpp —— 子像素光栅化基类与公共契约
//
// 终端字符是低分辨率网格，但一个字符格能塞下多个"子像素"：盲文 2×4、半块 1×2、
// 象限 2×2。光栅器把一个标量场（image::FloatField）压进这种子像素网格，产出
// CellBuffer——同一份场，换光栅器就是换分辨率/风格，GUI 控件与终端后端复用同一套。
//
// 不变量：
//   * rasterize 无副作用、不抛异常、不写 stdout；
//   * 输出的 CellBuffer 尺寸 = field / (subpixel_w, subpixel_h)；
//   * 单色光栅器：点亮处用 opts.fg，熄灭处用 opts.bg（invert 交换二者并反相）。
#ifndef TERMCHART_RASTER_RASTERIZER_HPP
#define TERMCHART_RASTER_RASTERIZER_HPP

#include <string>
#include <string_view>

#include "termchart/core/buffer.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/media/image/field.hpp"

namespace termchart::raster {

enum class RasterKind {
    Braille,    // 2×4，单色，分辨率最高
    HalfBlock,  // 1×2，上下双色
    Quadrant,   // 2×2，单色
    Ascii,      // 1×1，密度字符，最朴素
    Block,      // 1×1，八级竖条，单色
    Sextant,    // 2×3，六分仪字符（Unicode 1FB00 区）
};

struct RasterOptions {
    Color fg = Color::ansi16(7);    // 点亮色
    Color bg = Color::ansi16(0);    // 熄灭色
    bool invert = false;            // 反相（照片负片效果）
    double threshold = 0.5;         // 二进制子像素的开/关阈值
    // 盲文水平桥接（目前只有盲文光栅器认这个开关，其余光栅器忽略）：一条横贯的细
    // 笔画在每格只覆盖左半格（覆盖率不足阈值）时只点亮一个点，长直线在屏幕上呈虚线；
    // 打开后相邻两格同行都亮左点时把右点也补上 ⇒ 横向连续（竖向不受影响）。
    bool bridge_horizontal = true;
};

class Rasterizer {
public:
    virtual ~Rasterizer();
    [[nodiscard]] virtual CellBuffer rasterize(const image::FloatField& field,
                                              const RasterOptions& opts) const = 0;
    [[nodiscard]] virtual std::uint32_t subpixel_width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t subpixel_height() const noexcept = 0;
    [[nodiscard]] virtual RasterKind kind() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

// 一次性光栅化：按 kind 选实现，缺省选项。无对应实现时返回空 buffer。
TERMCHART_API CellBuffer rasterize(const image::FloatField& field, RasterKind kind,
                                   const RasterOptions& opts = {});

TERMCHART_API const char* raster_name(RasterKind kind) noexcept;

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_RASTERIZER_HPP
