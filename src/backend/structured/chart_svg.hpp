// chart_svg.hpp —— Chart 块的 Scene → 内联 SVG（backend/structured 私有头）
//
// Document 的 Chart 块只持有 Scene（backend 不认识 chart 类型，这正是
// backend > chart 禁止边成立的前提）。要让图表在导出的 HTML/Markdown 里真的
// 可见，就把这个 Scene 交给矢量物种现成的渲染器翻一遍——不新造第二条绘制路径、
// 也不把 chart 类型带进 backend。
//
// 空 Scene 仍输出 "[chart]" 占位符：examples/35_export_html 与
// tests/unit/backend/test_backend_structured.cpp 锁定的就是这条旧行为，而一个
// 没有任何几何的 Chart 块也确实无图可画。
#ifndef TERMCHART_SRC_BACKEND_STRUCTURED_CHART_SVG_HPP
#define TERMCHART_SRC_BACKEND_STRUCTURED_CHART_SVG_HPP

#include <string>

#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::backend::structured::detail {

// 图表 Scene 以字符格为场景单位，字号必须设成 1.0（同 api::chart_to_svg），
// 否则会被 VectorOptions::scale 放大 12 倍。
inline std::string chart_svg_or_placeholder(const scene::Scene& scene) {
    if (scene.empty()) {
        return "[chart]";
    }
    vector::SvgBackend svg;
    vector::VectorOptions opts;
    opts.font_size = 1.0;
    svg.set_options(opts);
    return vector::render_vector(svg, scene);
}

}  // namespace termchart::backend::structured::detail

#endif  // TERMCHART_SRC_BACKEND_STRUCTURED_CHART_SVG_HPP
