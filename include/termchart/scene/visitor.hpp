// visitor.hpp —— 指令访问者接口
//
// 为什么既给 std::visit 又给这套虚函数接口：
//   * std::visit 适合"一次性、无状态"的消费（统计、序列化），零分配；
//   * 虚函数访问者适合"有状态、可复用"的消费者（canvas 的光栅化器、
//     SVG 后端），它们需要把状态挂在对象上而不是塞进 lambda 捕获。
//
// accept() 的实现放在 cpp：variant 的访问模板只实例化一次，不进头文件。
#ifndef TERMCHART_SCENE_VISITOR_HPP
#define TERMCHART_SCENE_VISITOR_HPP

#include "termchart/export.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::scene {

class CommandVisitor {
public:
    virtual ~CommandVisitor();

    virtual void visit(const MoveTo& command) = 0;
    virtual void visit(const LineTo& command) = 0;
    virtual void visit(const RectOp& command) = 0;
    virtual void visit(const ArcOp& command) = 0;
    virtual void visit(const FillOp& command) = 0;
    virtual void visit(const StrokeOp& command) = 0;
    virtual void visit(const TextOp& command) = 0;
    virtual void visit(const ImageOp& command) = 0;

    // 分组是**可选语义**，默认忽略。
    // 忽略分组本来就是正确的：光栅化结果与静态导出都不该因为分组而改变。
    // 需要分组语义的消费者（交互层按组收拢 HitRegion、SVG 输出 <g>）自己
    // override；不 override 的消费者保持原有行为，无需改动。
    virtual void visit(const GroupOp& command) { (void)command; }

    // 2026-10-26 扩编指令：默认空实现，旧后端无需改动即可编译；
    // 要在状态机里生效的（曲线/变换/裁剪/渐变/…）由 StatefulVisitor override。
    virtual void visit(const CurveTo&) {}
    virtual void visit(const PushTransformOp&) {}
    virtual void visit(const PopTransformOp&) {}
    virtual void visit(const ClipOp&) {}
    virtual void visit(const BeginPathOp&) {}
    virtual void visit(const ClosePathOp&) {}
    virtual void visit(const LinearGradientOp&) {}
    virtual void visit(const RadialGradientOp&) {}
    virtual void visit(const GlobalAlphaOp&) {}
    virtual void visit(const DashOp&) {}
    virtual void visit(const SetLineWidthOp&) {}
    virtual void visit(const SetStrokeCapOp&) {}
    virtual void visit(const BlendModeOp&) {}
};

TERMCHART_API void accept(const Command& command, CommandVisitor& visitor);
TERMCHART_API void accept_all(const Scene& scene, CommandVisitor& visitor);

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_VISITOR_HPP
