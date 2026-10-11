// scene.hpp —— 绘制指令序列（矢量 IR）
//
// 三条渲染路径在它这里分岔：
//   * 终端实时：Scene → canvas/scene_renderer → Canvas → to_buffer() → CellBuffer → renderer；
//   * vector 导出：Scene → backend/vector/*（svg / latex-TikZ）；
//   * Document：scene 被 document/block.hpp 的 Chart 块持有，走 structured 导出。
//
// 因此 Scene 只有两个承诺：**可顺序回放**与**可整体比较**（golden 与
// roundtrip 测试都依赖后者）。它不理解颜色、不做裁剪、不算布局。
#ifndef TERMCHART_SCENE_SCENE_HPP
#define TERMCHART_SCENE_SCENE_HPP

#include <cstddef>
#include <span>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/command.hpp"

namespace termchart::scene {

class Scene {
public:
    Scene() = default;

    TERMCHART_API void clear() noexcept;
    TERMCHART_API void reserve(std::size_t count);
    TERMCHART_API void push(Command command);

    [[nodiscard]] std::size_t size() const noexcept { return commands_.size(); }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

    [[nodiscard]] std::span<const Command> commands() const noexcept { return {commands_}; }
    [[nodiscard]] std::span<Command> commands() noexcept { return {commands_}; }

    // 几何包围盒：只统计带几何的指令（MoveTo/LineTo 记点，Arc 用外接方框）。
    // Text 不参与——它的宽度要等字体度量，那不是 IR 该知道的事。
    // 空场景返回空矩形（w=h=0）。
    [[nodiscard]] TERMCHART_API Rect bounds() const noexcept;

    TERMCHART_API bool operator==(const Scene& other) const noexcept;
    TERMCHART_API bool operator!=(const Scene& other) const noexcept;

private:
    std::vector<Command> commands_;
};

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_SCENE_HPP
