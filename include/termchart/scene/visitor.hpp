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
};

TERMCHART_API void accept(const Command& command, CommandVisitor& visitor);
TERMCHART_API void accept_all(const Scene& scene, CommandVisitor& visitor);

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_VISITOR_HPP
