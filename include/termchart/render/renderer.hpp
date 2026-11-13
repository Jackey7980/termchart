// renderer.hpp —— 终端实时渲染管线（输出点）
//
// 终端实时路径的最后一环：CellBuffer → 增量字节流。
//   * 持有 prev 状态与 damage 跟踪，因此**一帧只输出变化的部分**；
//   * 与 snapshot 导出（backend/snapshot/*）的区别就在"有 prev 状态、有 diff"；
//   * 本类只生成字节，不写 stdout —— 写由 api/print.hpp 或调用方负责，
//     于是渲染在 CI / headless / 单元测试里完全可用。
#ifndef TERMCHART_RENDER_RENDERER_HPP
#define TERMCHART_RENDER_RENDERER_HPP

#include <memory>
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

    // 可移动：否则"按值返回 Renderer"或放进容器都得绕道（历史上删了拷贝却漏了移动，
    // 配合裸 Impl* 更是误加拷贝就 double-free）。
    // 移出后的对象只剩"可析构 / 可被重新赋值"两个合法操作，与标准库的约定一致。
    TERMCHART_API Renderer(Renderer&&) noexcept;
    TERMCHART_API Renderer& operator=(Renderer&&) noexcept;

    TERMCHART_API void set_depth(ColorDepth depth) noexcept;
    [[nodiscard]] ColorDepth depth() const noexcept { return depth_; }

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
    // unique_ptr 而非裸指针：所有权由类型表达，误加拷贝会在编译期被拦住，
    // 而不是运行期 double-free。析构与移动因此在 .cpp 里定义（那里 Impl 才完整）。
    std::unique_ptr<Impl> impl_;
    ColorDepth depth_ = ColorDepth::TrueColor;
};

}  // namespace termchart::render

#endif  // TERMCHART_RENDER_RENDERER_HPP
