#include "termchart/canvas/scene_renderer.hpp"

#include "termchart/canvas/primitives.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/scene/visitor.hpp"

namespace termchart::canvas {
namespace {

// 占位字符：光栅器（raster/，阶段 7 起）接入后会被盲文 / 半块字符替换。
// 接口不变，观感变——这正是把"离散"和"取字符"分开的目的。
constexpr char32_t kStrokeChar = U'*';

class ScenePainter final : public scene::CommandVisitor {
public:
    explicit ScenePainter(Canvas& canvas) noexcept : canvas_(&canvas) {}

    void visit(const scene::MoveTo& command) override { current_ = command.to; }

    void visit(const scene::LineTo& command) override {
        canvas_->draw_line(current_, command.to, kStrokeChar, stroke_);
        current_ = command.to;
    }

    void visit(const scene::RectOp& command) override {
        if (!fill_.fg().is_none()) {
            canvas_->fill_rect(command.rect, detail::kFullBlock, fill_);
            return;
        }
        for (const Point& p : rect_outline_points(command.rect)) {
            canvas_->put(p, kStrokeChar, stroke_);
        }
    }

    void visit(const scene::ArcOp& command) override {
        for (const Point& p : arc_points(command.center, command.radius, command.start,
                                        command.end)) {
            canvas_->put(p, kStrokeChar, stroke_);
        }
    }

    // Fill / Stroke 是**状态指令**：作用于其后出现的图元（与 SVG 同构）。
    void visit(const scene::FillOp& command) override { fill_ = Style{}.set_fg(command.color); }
    void visit(const scene::StrokeOp& command) override { stroke_ = Style{}.set_fg(command.color); }

    void visit(const scene::TextOp& command) override {
        canvas_->draw_text(command.at, command.text, command.style);
    }

    void visit(const scene::ImageOp& command) override {
        // 像素数据不在 Scene 里（分层不允许 scene 依赖 image），这里只占位。
        canvas_->fill_rect(command.rect, detail::kMediumShade, Style{});
    }

private:
    Canvas* canvas_;
    Point current_{};
    Style fill_{};
    Style stroke_{};
};

}  // namespace

void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept {
    ScenePainter painter(canvas);
    scene::accept_all(scene, painter);
}

}  // namespace termchart::canvas
