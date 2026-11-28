// hover_chart.hpp —— GUI 悬停层：命中 → 把浮层翻译成 Scene 图元
//
// 定位：GUI 平行栈的交互呈现。核心设计是"**悬停不改渲染器，只改 Scene**"——
// gui::SceneRenderer 根本不知道交互存在，它只是又消费了一份 Scene。
//
// 复用面（都是既有件，不是新写的状态机）：
//   * chart::HitRegion         —— 图表渲染时登记的命中形状 + 不透明 payload，
//                                 是 chart → interaction 的唯一契约；
//   * interaction::topmost_hit —— 纯几何判定（最后登记者在上），禁图表名；
//   * scene::SceneBuilder      —— 浮层就是普通的 RectOp / TextOp 图元。
//
// 与终端侧 interaction::Tooltip 的关系：**刻意不共用实现**。终端 tooltip 是
// cell 补丁语义（记住被改写的格、clear() 后逐格还原），因为它画在字符画布上、
// 覆盖即破坏；GUI 每帧重建整份 Scene，浮层只是"这一帧多塞几个图元"，既不需要
// patch/restore，也不该把"字符格"这个概念带进像素域。二者共享的只有上游的
// HitRegion / topmost_hit，以及"贴边要翻方向"这条直觉。
//
// 坐标系：HitRegion 与浮层坐标都在 **Scene 坐标系**里，与底图同一坐标系。
// 调用方负责把窗口/鼠标像素换算成 Scene 坐标；autofit=false 时二者恒等。
#ifndef TERMCHART_GUI_HOVER_CHART_HPP
#define TERMCHART_GUI_HOVER_CHART_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/export.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::gui {

class HoverLayer {
public:
    // base    底图 Scene；
    // regions 命中区域（约定 payload ≥ 1，0 视为"只有几何、未登记语义"）；
    // labels  labels[payload-1] 是该 payload 的浮层文本，可含 '\n' 多行；
    // options 与渲染底图时**同一份** RenderOptions（坐标系/文本格必须一致）。
    TERMCHART_API HoverLayer(scene::Scene base,
                             std::vector<chart::HitRegion> regions,
                             std::vector<std::string> labels,
                             RenderOptions options);

    // 不带浮层的底图。
    [[nodiscard]] TERMCHART_API image::Bitmap render_base() const;

    // (x,y) 命中则返回"底图 + 浮层"；未命中等价于 render_base()。
    // 浮层位置：优先锚点右上，越界翻左上、再越界翻下方，最后夹进画布。
    [[nodiscard]] TERMCHART_API image::Bitmap render_at(double x, double y) const;

    // 只查询不渲染：命中返回该 payload 的浮层文本，未命中/未登记返回空串。
    [[nodiscard]] TERMCHART_API std::string hit_label(double x, double y) const;

private:
    scene::Scene base_;
    std::vector<chart::HitRegion> regions_;
    std::vector<std::string> labels_;
    RenderOptions options_;
};

}  // namespace termchart::gui

#endif  // TERMCHART_GUI_HOVER_CHART_HPP
