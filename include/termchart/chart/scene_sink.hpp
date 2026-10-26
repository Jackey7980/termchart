// scene_sink.hpp —— 把 layout 的落笔语义翻译成**真实的 Scene 指令**
//
// 与"把 Canvas 逐格录成 TextOp"（那种只是把位图换了个容器）的区别：
//   * Solid 线 → MoveTo/LineTo + StrokeOp，是矢量。终端侧由 render_scene
//     光栅成盲文/半块，信息量比逐格 • 更高；导出侧就是真正的 <line>。
//   * Dashed 线 → 逐格 TextOp。原因是盲文光栅器会把虚线画成实线，语义会丢；
//     虚线本来就是字符艺术，保留字符才是忠实的。
//   * 文本/角/刻度钩 → TextOp，它们本来就只能是字符。
//
// 分组：调用方用 begin_group()/end_group() 把一段指令包起来（见 SceneBuilder）。
// 分组不改变任何像素，只让交互层能把 HitRegion 成组收拢。
#ifndef TERMCHART_CHART_SCENE_SINK_HPP
#define TERMCHART_CHART_SCENE_SINK_HPP

#include <string_view>

#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/sink.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::chart {

class SceneSink final : public layout::Sink {
public:
    explicit SceneSink(scene::Scene& scene) noexcept;

    void put(Point at, char32_t cp, const Style& style) override;
    void text(Point at, std::string_view value, const Style& style) override;
    void line(Point a, Point b, layout::LineStyle kind, const Style& style) override;

    [[nodiscard]] scene::SceneBuilder& builder() noexcept { return builder_; }

private:
    scene::SceneBuilder builder_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SCENE_SINK_HPP
