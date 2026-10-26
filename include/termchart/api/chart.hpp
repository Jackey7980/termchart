// chart.hpp —— 一行出图（chart 侧的 api 门面）
//
// 为什么要有它：examples/03 的"标准姿势"有五步（建 Canvas → 建 Chart → 喂 Series
// → chart.draw({canvas}) → Renderer::render_full → 写 stdout），其中四步是纯样板，
// 且**每一步都能写错**（忘了 render_full 会拿到空帧、忘了 flush 会丢半屏）。
// `render_chart` 把它们收成一次调用：你只负责数据与图表类型。
//
// 与 `quick::render`（CellBuffer → 字节）的分工：quick 面向"我已经有一块 buffer"，
// 这里面向"我只有一个图表"。
//
// 尺寸规则是**刻板**的（见 cpp 里的实现注释）：不自动探测终端。理由——
// 自动探测会让同一段代码在 CI/headless 与真终端里产出不同尺寸，测试和文档都没法
// 复现；要自适应请显式传 `opts.width(height_of_terminal)`（另见 CODEBUDDY.md 坑 11：
// 画布大于可视区会折行 + 滚动，图像错位）。
#ifndef TERMCHART_API_CHART_HPP
#define TERMCHART_API_CHART_HPP

#include <cstdint>
#include <string>

#include "termchart/api/opts.hpp"
#include "termchart/backend/vector/backend.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::api {

// 任意 Chart（折线 / 柱 / 散点 / 迷你线…）→ 一次成帧的字节流（全量，不含差分）。
// 尺寸：opts.width()/height() 有值就用（显式 0 是 InvalidArgument），否则 80×24。
[[nodiscard]] TERMCHART_API Result<std::string> render_chart(chart::Chart& chart,
                                                             const Opts& opts = {}) noexcept;

// = render_chart + 写 stdout + flush。返回 stdout 的写入状态（渲染失败时返回该错误）。
TERMCHART_API Status print_chart(chart::Chart& chart, const Opts& opts = {}) noexcept;

// ── 真实 Scene 通路 ──────────────────────────────────────────────────────
// 图表 → 矢量指令流（MoveTo/LineTo/StrokeOp/RectOp/TextOp/GroupOp），**不是**
// 把 Canvas 逐格转录成 TextOp。同一份 Scene 有三种消费者：render_scene 落字符格、
// SVG/PDF 静态导出、交互层按 GroupOp 成组命中。
//
// 未迁移的图种返回 ErrorCode::Unsupported —— 显式要 Scene 就显式报错，不悄悄
// 返回一份"长得像 Scene 的 Canvas 转录"。
[[nodiscard]] TERMCHART_API Result<scene::Scene> chart_to_scene(const chart::Chart& chart,
                                                                std::uint32_t width,
                                                                std::uint32_t height) noexcept;

// = chart_to_scene + SVG 后端。图表 Scene 的场景单位就是**字符格**，因此字号
// 默认给 1.0（scene 单位）；调用方仍可覆盖 options 里的 scale/autofit。
[[nodiscard]] TERMCHART_API Result<std::string> chart_to_svg(
    const chart::Chart& chart, std::uint32_t width, std::uint32_t height,
    const backend::vector::VectorOptions& options = {}) noexcept;

}  // namespace termchart::api

#endif  // TERMCHART_API_CHART_HPP
