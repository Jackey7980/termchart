// compositor.hpp —— 多图层合成（把若干 CellBuffer 按偏移叠加成最终帧）
//
// GUI / 重叠元素 / 多 Pass 渲染都需要"图层"概念：本组件把任意数量的图层
// （每个是一个 CellBuffer + 左上角偏移）合成到一张目标缓冲。后绘制的图层覆盖
// 先绘制的；transparent 时，空格单元不覆盖下层（透出底色）。
#ifndef TERMCHART_RENDER_COMPOSITOR_HPP
#define TERMCHART_RENDER_COMPOSITOR_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/buffer.hpp"
#include "termchart/export.hpp"

namespace termchart::render {

struct Layer {
    const CellBuffer* buffer = nullptr;  // 非空图层缓冲（不拷贝，零成本）
    std::int32_t x = 0;                  // 左上角列偏移
    std::int32_t y = 0;                  // 左上角行偏移
    bool transparent = true;             // 空格单元是否透出下层
};

class Compositor {
public:
    // 合成到给定的 out（out 尺寸需已足够容纳最远图层；否则用重载版本自动扩展）。
    TERMCHART_API static void composite(const std::vector<Layer>& layers, CellBuffer& out);
    // 自动按包围盒分配 out 并返回。
    [[nodiscard]] TERMCHART_API static CellBuffer composite(const std::vector<Layer>& layers);
};

}  // namespace termchart::render

#endif  // TERMCHART_RENDER_COMPOSITOR_HPP
