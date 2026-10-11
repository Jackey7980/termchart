// renderer.hpp —— 终端实时渲染管线（输出点）
//
// 终端实时路径的最后一环：CellBuffer → 增量字节流。
//   * 持有 prev 状态与 damage 跟踪，因此**一帧只输出变化的部分**；
//   * 与 snapshot 导出（backend/snapshot/*）的区别就在"有 prev 状态、有 diff"；
//   * 本类只生成字节，不写 stdout —— 写由 api/print.hpp 或调用方负责，
//     于是渲染在 CI / headless / 单元测试里完全可用。
#ifndef TERMCHART_RENDER_RENDERER_HPP
#define TERMCHART_RENDER_RENDERER_HPP

#include <string>

#include "termchart/core/buffer.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/render/damage.hpp"

namespace termchart::render {

class Renderer {
public:
    explicit Renderer(ColorDepth depth = ColorDepth::TrueColor) noexcept;
    TERMCHART_API ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    TERMCHART_API void set_depth(ColorDepth depth) noexcept;
    [[nodiscard]] TERMCHART_API ColorDepth depth() const noexcept { return depth_; }

    // 增量渲染：与内部 prev 比较。首帧或尺寸变化时自动退化为全量。
    TERMCHART_API std::string render(const CellBuffer& current) noexcept;

    // 全量渲染：忽略 prev（用于首帧、能力变化、被强制刷新）。
    TERMCHART_API std::string render_full(const CellBuffer& current) noexcept;

    // 丢弃 prev 状态：下次 render() 必为全量（终端被清屏后必须调用）。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] TERMCHART_API const CellBuffer* previous() const noexcept;
    [[nodiscard]] TERMCHART_API const DamageTracker& damage() const noexcept;
    [[nodiscard]] TERMCHART_API DamageTracker& damage() noexcept;

private:
    class Impl;
    Impl* impl_ = nullptr;
    ColorDepth depth_ = ColorDepth::TrueColor;
};

}  // namespace termchart::render

#endif  // TERMCHART_RENDER_RENDERER_HPP
