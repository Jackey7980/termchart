// bounds.cpp —— Scene 包围盒实现
#include "termchart/scene/bounds.hpp"

#include <algorithm>
#include <cstddef>

#include "termchart/scene/visitor.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::scene {
namespace {

class BoundsVisitor final : public CommandVisitor {
public:
    [[nodiscard]] Rect result() const noexcept {
        if (!any_) {
            return Rect{};
        }
        return Rect{min_x_, min_y_, max_x_ - min_x_, max_y_ - min_y_};
    }

    void visit(const MoveTo& command) override {
        extend(command.to);
        current_ = command.to;
    }

    void visit(const LineTo& command) override {
        extend(current_);  // 折线的起点也要算进去（MoveTo 之后可能没被 extend 过）
        extend(command.to);
        current_ = command.to;
    }

    void visit(const RectOp& command) override { extend_rect(command.rect); }

    void visit(const ArcOp& command) override {
        extend_rect(Rect{command.center.x - command.radius, command.center.y - command.radius,
                         command.radius * 2.0, command.radius * 2.0});
    }

    void visit(const TextOp& command) override {
        const auto width = static_cast<double>(util::display_width(command.text));
        extend_rect(Rect{command.at.x, command.at.y, width, 1.0});
    }

    void visit(const ImageOp& command) override { extend_rect(command.rect); }

    // 状态指令不产生几何。
    void visit(const FillOp&) override {}
    void visit(const StrokeOp&) override {}

private:
    void extend(Point p) noexcept {
        if (!any_) {
            min_x_ = max_x_ = p.x;
            min_y_ = max_y_ = p.y;
            any_ = true;
            return;
        }
        min_x_ = std::min(min_x_, p.x);
        max_x_ = std::max(max_x_, p.x);
        min_y_ = std::min(min_y_, p.y);
        max_y_ = std::max(max_y_, p.y);
    }

    void extend_rect(Rect rect) noexcept {
        if (rect.w <= 0.0 && rect.h <= 0.0) {
            extend(Point{rect.x, rect.y});
            return;
        }
        extend(Point{rect.x, rect.y});
        extend(Point{rect.right(), rect.bottom()});
    }

    bool any_ = false;
    double min_x_ = 0.0;
    double min_y_ = 0.0;
    double max_x_ = 0.0;
    double max_y_ = 0.0;
    Point current_{};
};

}  // namespace

Rect scene_bounds(const Scene& scene) noexcept {
    BoundsVisitor visitor;
    accept_all(scene, visitor);
    return visitor.result();
}

}  // namespace termchart::scene
