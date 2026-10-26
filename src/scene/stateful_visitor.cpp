// stateful_visitor.cpp —— 有状态 Scene 消费者的时序骨架
//
// 实现见头文件里的时序约定；这里只放"什么时候画"，不放"往哪画"。
#include "termchart/scene/stateful_visitor.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::scene {

namespace {

// 合成变换栈：从底（先施加）到顶（后施加）依次左乘；空栈=恒等。
Point apply_transforms(const std::vector<Transform>& stack, const Point& p) noexcept {
    if (stack.empty()) {
        return p;
    }
    double x = p.x;
    double y = p.y;
    for (const Transform& t : stack) {
        const double nx = t.a * x + t.c * y + t.e;
        const double ny = t.b * x + t.d * y + t.f;
        x = nx;
        y = ny;
    }
    return Point{x, y};
}

Rect apply_transforms_rect(const std::vector<Transform>& stack, const Rect& r) noexcept {
    const Point a = apply_transforms(stack, Point{r.x, r.y});
    const Point b = apply_transforms(stack, Point{r.right(), r.bottom()});
    return Rect{std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(b.x - a.x),
                std::fabs(b.y - a.y)};
}

}  // namespace

void StatefulVisitor::visit(const MoveTo& command) {
    // 不 finish_path：同一颜色下多子路径合并成一条 <path>（SVG 依赖这点）。
    current_ = apply_transforms(transforms_, command.to);
    path_open_ = false;
}

void StatefulVisitor::visit(const LineTo& command) {
    const Point to = apply_transforms(transforms_, command.to);
    if (!path_open_) {
        begin_path(current_);
        path_open_ = true;
        subpath_start_ = current_;
    }
    extend_path(current_, to);
    current_ = to;
}

void StatefulVisitor::visit(const FillOp& command) {
    finish_path();
    fill_ = command.color;
    fill_is_gradient_ = false;  // 实色填充清除渐变画刷
}

void StatefulVisitor::visit(const StrokeOp& command) {
    finish_path();
    stroke_ = command.color;
    stroke_width_ = command.width;
}

void StatefulVisitor::visit(const RectOp& command) {
    finish_path();
    const Rect r = apply_transforms_rect(transforms_, command.rect);
    if (fill_is_gradient_ || !fill_.is_none()) {
        if (fill_is_gradient_) {
            emit_filled_rect_gradient(r, fill_grad_radial_, fill_grad_g0_, fill_grad_g1_,
                                      fill_grad_radius_, fill_stops_);
        } else {
            emit_filled_rect(r, fill_);
        }
    } else {
        emit_stroked_rect(r, stroke_, stroke_width_);
    }
}

void StatefulVisitor::visit(const ArcOp& command) {
    finish_path();
    ArcOp a = command;
    a.center = apply_transforms(transforms_, a.center);  // 半径受非均匀缩放影响暂忽略（基线）
    emit_arc(a, stroke_, stroke_width_);
}

void StatefulVisitor::visit(const TextOp& command) {
    finish_path();
    TextOp t = command;
    t.at = apply_transforms(transforms_, t.at);
    emit_text(t);
}

void StatefulVisitor::visit(const ImageOp& command) {
    finish_path();
    ImageOp im = command;
    im.rect = apply_transforms_rect(transforms_, im.rect);
    emit_image(im);
}

void StatefulVisitor::visit(const GroupOp& /*command*/) {
    // 有意为空：分组是区间的标注，不是图元。
    // 这里绝不能调 finish_path()——那会把一条连续路径从组边界处劈成两段，
    // 描边多刷一次、SVG 多一个 <path>，分组就从"零影响"变成了"有影响"。
}

// ── 2026-10-26 扩编指令：状态机接入 ──

void StatefulVisitor::visit(const CurveTo& command) {
    const Point c1 = apply_transforms(transforms_, command.ctrl1);
    const Point c2 = apply_transforms(transforms_, command.ctrl2);
    const Point end = apply_transforms(transforms_, command.end);
    if (!path_open_) {
        begin_path(current_);
        path_open_ = true;
        subpath_start_ = current_;
    }
    extend_path_curve(current_, c1, c2, end);
    current_ = end;
}

void StatefulVisitor::visit(const PushTransformOp& command) {
    transforms_.push_back(command.matrix);
}

void StatefulVisitor::visit(const PopTransformOp&) {
    if (!transforms_.empty()) {
        transforms_.pop_back();
    }
}

void StatefulVisitor::visit(const ClipOp& command) {
    const Rect r = apply_transforms_rect(transforms_, command.region);
    if (!command.intersect || clips_.empty()) {
        clips_.clear();
        clip_composed_ = r;
    } else {
        // 与当前裁剪区取交
        const Rect& cur = clips_.back();
        const double x0 = std::max(cur.x, r.x);
        const double y0 = std::max(cur.y, r.y);
        const double x1 = std::min(cur.right(), r.right());
        const double y1 = std::min(cur.bottom(), r.bottom());
        clip_composed_ =
            Rect{x0, y0, std::max(0.0, x1 - x0), std::max(0.0, y1 - y0)};
    }
    clips_.push_back(clip_composed_);
}

void StatefulVisitor::visit(const BeginPathOp&) {
    // 重置当前子路径：下个 LineTo/CurveTo 重新 begin_path（SVG 重新发 'M'）。
    path_open_ = false;
}

void StatefulVisitor::visit(const ClosePathOp&) {
    if (path_open_) {
        extend_path(current_, subpath_start_);  // 直线连回起点（Canvas 描边闭合）
        current_ = subpath_start_;
        close_path();  // SVG 发 'Z'
    }
}

void StatefulVisitor::visit(const LinearGradientOp& command) {
    fill_is_gradient_ = true;
    fill_grad_radial_ = false;
    fill_grad_g0_ = apply_transforms(transforms_, command.start);
    fill_grad_g1_ = apply_transforms(transforms_, command.end);
    fill_grad_radius_ = 0.0;
    fill_stops_ = command.stops;
}

void StatefulVisitor::visit(const RadialGradientOp& command) {
    fill_is_gradient_ = true;
    fill_grad_radial_ = true;
    fill_grad_g0_ = apply_transforms(transforms_, command.center);
    fill_grad_g1_ = fill_grad_g0_;
    fill_grad_radius_ = command.radius;
    fill_stops_ = command.stops;
}

void StatefulVisitor::visit(const GlobalAlphaOp& command) {
    alpha_ = command.alpha;
}

void StatefulVisitor::visit(const DashOp& command) {
    dash_ = command;
    dash_active_ = !command.dashes.empty();
}

void StatefulVisitor::visit(const SetLineWidthOp& command) {
    stroke_width_ = command.width;
}

void StatefulVisitor::visit(const SetStrokeCapOp& command) {
    cap_ = command.cap;
}

void StatefulVisitor::visit(const BlendModeOp& command) {
    blend_ = command.mode;
}

Point StatefulVisitor::transform_point(const Point& p) const noexcept {
    return apply_transforms(transforms_, p);
}

Rect StatefulVisitor::transform_rect(const Rect& r) const noexcept {
    return apply_transforms_rect(transforms_, r);
}

void StatefulVisitor::run(const Scene& scene) {
    for (const Command& command : scene.commands()) {
        accept(command, *this);
    }
    finish_path();
}

void StatefulVisitor::finish_path() {
    // end_path() 无条件调用：判空交给实现自己（dirty 标志 / 空串守卫）。
    end_path();
    path_open_ = false;
}

}  // namespace termchart::scene
