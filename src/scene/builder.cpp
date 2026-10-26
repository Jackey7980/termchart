#include "termchart/scene/builder.hpp"

#include <string>
#include <utility>

namespace termchart::scene {

SceneBuilder& SceneBuilder::move_to(Point to) { return push(MoveTo{to}); }

SceneBuilder& SceneBuilder::move_to(double x, double y) { return push(MoveTo{Point{x, y}}); }

SceneBuilder& SceneBuilder::line_to(Point to) { return push(LineTo{to}); }

SceneBuilder& SceneBuilder::line_to(double x, double y) { return push(LineTo{Point{x, y}}); }

SceneBuilder& SceneBuilder::rect(Rect value) { return push(RectOp{value}); }

SceneBuilder& SceneBuilder::arc(Point center, double radius, double start, double end) {
    return push(ArcOp{center, radius, start, end});
}

SceneBuilder& SceneBuilder::fill(Color color) { return push(FillOp{color}); }

SceneBuilder& SceneBuilder::stroke(Color color, double width) {
    return push(StrokeOp{color, width});
}

SceneBuilder& SceneBuilder::text(Point at, std::string_view text, Style style) {
    return push(TextOp{at, std::string(text), style});
}

SceneBuilder& SceneBuilder::image(Rect rect, std::uint32_t image_id) {
    return push(ImageOp{rect, image_id});
}

SceneBuilder& SceneBuilder::begin_group(std::uint32_t id) { return push(GroupOp{id, true}); }

SceneBuilder& SceneBuilder::end_group(std::uint32_t id) { return push(GroupOp{id, false}); }

SceneBuilder& SceneBuilder::push(Command command) {
    scene_->push(std::move(command));
    return *this;
}

}  // namespace termchart::scene
