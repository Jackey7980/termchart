// layer.hpp —— 图层合成
//
// 终端没有 alpha 通道，但有三种**真实可用**的合成语义：
//   * Overlay：只写"非空白"格（空白处露出下层）—— 等价于透明，最常用；
//   * Replace：连空白格一起覆盖（"这块区域归我"）；
//   * Tint   ：保留字符、统一换成给定样式（高亮 / 遮罩 / 变暗）。
//
// 空白的口径：`is_blank()`（空格且无样式）。**带样式的空格会被当作内容写入**，
// 这正是"铺底色"的用法；不想铺就用默认样式留空格。
//
// 另外提供最小重绘支持：mark_clean() 记快照，dirty_bounds() 给出与快照的差异包围盒。
#ifndef TERMCHART_CANVAS_LAYER_HPP
#define TERMCHART_CANVAS_LAYER_HPP

#include <cstddef>
#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

class Layer {
public:
    Layer() = default;
    TERMCHART_API Layer(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();

    [[nodiscard]] std::uint32_t width() const noexcept { return canvas_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return canvas_.height(); }

    // 在本层作画。
    [[nodiscard]] Canvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] const Canvas& canvas() const noexcept { return canvas_; }
    [[nodiscard]] const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept {
        return canvas_.at(x, y);
    }
    // 快照（拷贝），交给渲染管线或做对比用。
    [[nodiscard]] TERMCHART_API CellBuffer to_buffer() const;

    enum class Mode {
        Overlay,  // 只写非空白格
        Replace,  // 整层覆盖
        Tint,     // 非空白格保留字符、套用 tint 样式
    };

    // 合成到目标画布：at 为落点（可为负，负处按格裁剪丢弃）。
    // tint 仅 Tint 模式使用；传 nullptr 表示保留原样式。
    TERMCHART_API std::size_t composite_to(Canvas& target, Point at = {},
                                           Mode mode = Mode::Overlay,
                                           const Style* tint = nullptr) const;

    // 记下"当前状态即干净"；dirty_bounds() 之后给出差异包围盒。
    TERMCHART_API void mark_clean();

    // 与上次 mark_clean() 之间的差异包围盒（层坐标）。
    // 若从未 mark_clean() 或没有任何差异，返回零矩形（w == h == 0）。
    [[nodiscard]] TERMCHART_API Rect dirty_bounds() const;

private:
    Canvas canvas_;
    CellBuffer clean_;
    bool has_clean_ = false;
};

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_LAYER_HPP
