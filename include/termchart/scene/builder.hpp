// builder.hpp —— 流式构建辅助
//
// 存在的唯一理由：让"往 Scene 里追加图元"这件事读起来像绘图代码。
// 它**不持有** Scene（只持有引用），也不缓存当前路径——路径状态属于消费者。
#ifndef TERMCHART_SCENE_BUILDER_HPP
#define TERMCHART_SCENE_BUILDER_HPP

#include <cstdint>
#include <string_view>

#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::scene {

class SceneBuilder {
public:
    explicit SceneBuilder(Scene& scene) noexcept : scene_(&scene) {}

    TERMCHART_API SceneBuilder& move_to(Point to);
    TERMCHART_API SceneBuilder& move_to(double x, double y);

    TERMCHART_API SceneBuilder& line_to(Point to);
    TERMCHART_API SceneBuilder& line_to(double x, double y);

    TERMCHART_API SceneBuilder& rect(Rect value);

    // 角度为弧度；整圆传 {0.0, 2π}。
    TERMCHART_API SceneBuilder& arc(Point center, double radius, double start, double end);

    TERMCHART_API SceneBuilder& fill(Color color);
    TERMCHART_API SceneBuilder& stroke(Color color, double width = 1.0);

    TERMCHART_API SceneBuilder& text(Point at, std::string_view text, Style style = Style{});

    TERMCHART_API SceneBuilder& image(Rect rect, std::uint32_t image_id);

    // 逃生口：直接压一条已构造好的指令（variant 新候选还没加语法糖时用）。
    TERMCHART_API SceneBuilder& push(Command command);

    [[nodiscard]] Scene& scene() const noexcept { return *scene_; }

private:
    Scene* scene_;
};

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_BUILDER_HPP
