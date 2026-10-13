// stateful_visitor.cpp —— 有状态 Scene 消费者的时序骨架
//
// 实现见头文件里的时序约定；这里只放"什么时候画"，不放"往哪画"。
#include "termchart/scene/stateful_visitor.hpp"

namespace termchart::scene {

void StatefulVisitor::visit(const MoveTo& command) {
    // 不 finish_path：同一颜色下多子路径合并成一条 <path>（SVG 依赖这点）。
    current_ = command.to;
    path_open_ = false;
}

void StatefulVisitor::visit(const LineTo& command) {
    if (!path_open_) {
        begin_path(current_);
        path_open_ = true;
    }
    extend_path(current_, command.to);
    current_ = command.to;
}

void StatefulVisitor::visit(const FillOp& command) {
    finish_path();
    fill_ = command.color;
}

void StatefulVisitor::visit(const StrokeOp& command) {
    finish_path();
    stroke_ = command.color;
    stroke_width_ = command.width;
}

void StatefulVisitor::visit(const RectOp& command) {
    finish_path();
    if (!fill_.is_none()) {
        emit_filled_rect(command.rect, fill_);
    } else {
        emit_stroked_rect(command.rect, stroke_, stroke_width_);
    }
}

void StatefulVisitor::visit(const ArcOp& command) {
    finish_path();
    emit_arc(command, stroke_, stroke_width_);
}

void StatefulVisitor::visit(const TextOp& command) {
    finish_path();
    emit_text(command);
}

void StatefulVisitor::visit(const ImageOp& command) {
    finish_path();
    emit_image(command);
}

void StatefulVisitor::run(const Scene& scene) {
    for (const Command& command : scene.commands()) {
        accept(command, *this);
    }
    finish_path();
}

void StatefulVisitor::finish_path() {
    // end_path() 无条件调用：判空交给实现自己（dirty 标志 / 空串守卫）。
    //
    // 收尾那一次尤其不能省：若最后一个图元是**描边矩形**，它只往 stroke_field_
    // 里累积、自己不 flush，run() 末尾这次 end_path() 就是它唯一的刷出点。
    // 那不是一次可以优化掉的空调用——删掉它，canvas 侧会直接少一条边框。
    end_path();
    // 顺带修掉"非路径图元后紧跟 LineTo 会丢 M"的隐患（SVG 侧会产出
    // `<path d="L ...">`，起点丢失）。
    path_open_ = false;
}

}  // namespace termchart::scene
