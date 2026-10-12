// headless.hpp —— 无终端模式
//
// 提供"渲染到 CellBuffer 而**不碰 stdout、不触碰 term/**"的入口封装：
//   Canvas（格粒度绘制）→ CellBuffer → render::Renderer → 字节串
//
// 与实时路径（Terminal → Renderer → stdout）共享同一个 Renderer，因此在
// CI / headless / 单元测试里得到的字节流与真终端一致；差异只剩"谁来写
// stdout"——这里谁都不写，字节串直接交给调用方（golden、断言、存文件）。
#ifndef TERMCHART_APP_HEADLESS_HPP
#define TERMCHART_APP_HEADLESS_HPP

#include <functional>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/render/renderer.hpp"

namespace termchart::app {

// 在画布上作画的回调（ImmediateApp 与 render_once 共用）。
using DrawFn = std::function<void(canvas::Canvas& frame)>;

// 一次性渲染：width×height 画布交给 draw 作画，返回渲染字节串。
// width 或 height 为 0 → InvalidArgument（空画布没有可渲染的东西）。
[[nodiscard]] TERMCHART_API Result<std::string> render_once(std::uint32_t width,
                                                            std::uint32_t height,
                                                            const DrawFn& draw,
                                                            ColorDepth depth =
                                                                ColorDepth::TrueColor);

// 多帧会话：保留画布与 Renderer 的 prev 状态，frame() 出**增量**帧
// （首帧自动全量）；reset() 丢弃 prev，下一帧恢复全量。
// 画面未变时 frame() 返回空串（无差异就无输出）——这正是增量路径的契约。
class HeadlessSession {
public:
    TERMCHART_API HeadlessSession(std::uint32_t width, std::uint32_t height,
                                  ColorDepth depth = ColorDepth::TrueColor);

    [[nodiscard]] canvas::Canvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] const canvas::Canvas& canvas() const noexcept { return canvas_; }

    // 画完再调用。返回本帧字节串（增量）。
    [[nodiscard]] TERMCHART_API std::string frame();

    // 无视 prev 的全量渲染（导出 / 校准用）。
    [[nodiscard]] TERMCHART_API std::string frame_full();

    // 丢弃 prev 状态：下一帧 frame() 必为全量。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] std::uint32_t width() const noexcept { return canvas_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return canvas_.height(); }

private:
    canvas::Canvas canvas_;
    render::Renderer renderer_;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_HEADLESS_HPP
