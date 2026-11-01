// test_scene.cpp —— Scene（矢量 IR）契约
//
// 契约只有三条：指令可顺序回放、场景可整体比较、包围盒只算几何。
// canvas 与 vector 后端都依赖这三条，改动 Scene 前先看这里。
#include <doctest.h>

#include <cstddef>
#include <map>
#include <string>

#include "termchart/core/style.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/scene/visitor.hpp"

namespace {

using namespace termchart;
using namespace termchart::scene;

class CountingVisitor final : public CommandVisitor {
public:
    using CommandVisitor::visit;  // 仅 override 部分指令；引入基类其余 visit 重载，避免 -Woverloaded-virtual
    void visit(const MoveTo&) override { ++counts_[CommandKind::MoveTo]; }
    void visit(const LineTo&) override { ++counts_[CommandKind::LineTo]; }
    void visit(const RectOp&) override { ++counts_[CommandKind::Rect]; }
    void visit(const ArcOp&) override { ++counts_[CommandKind::Arc]; }
    void visit(const FillOp&) override { ++counts_[CommandKind::Fill]; }
    void visit(const StrokeOp&) override { ++counts_[CommandKind::Stroke]; }
    void visit(const TextOp&) override { ++counts_[CommandKind::Text]; }
    void visit(const ImageOp&) override { ++counts_[CommandKind::Image]; }

    [[nodiscard]] int count_of(CommandKind kind) const {
        const auto it = counts_.find(kind);
        return it == counts_.end() ? 0 : it->second;
    }

private:
    std::map<CommandKind, int> counts_;
};

Scene triangle_scene() {
    Scene scene;
    SceneBuilder(scene)
        .move_to(0.0, 0.0)
        .line_to(10.0, 0.0)
        .line_to(5.0, 8.0)
        .fill(Color::rgb(1, 2, 3));
    return scene;
}

TEST_CASE("scene: 构建器按调用顺序压入指令") {
    const Scene scene = triangle_scene();
    REQUIRE(scene.size() == 4);
    CHECK(kind_of(scene.commands()[0]) == CommandKind::MoveTo);
    CHECK(kind_of(scene.commands()[1]) == CommandKind::LineTo);
    CHECK(kind_of(scene.commands()[2]) == CommandKind::LineTo);
    CHECK(kind_of(scene.commands()[3]) == CommandKind::Fill);
}

TEST_CASE("scene: 访问者收到全部指令类别") {
    Scene scene;
    SceneBuilder(scene)
        .move_to(1.0, 1.0)
        .line_to(2.0, 2.0)
        .rect(Rect{0.0, 0.0, 4.0, 3.0})
        .arc(Point{5.0, 5.0}, 2.0, 0.0, 6.28)
        .fill(Color::ansi16(3))
        .stroke(Color::indexed(200), 1.5)
        .text(Point{0.0, 0.0}, "hi", Style{}.set_attr(Attr::Bold))
        .image(Rect{0.0, 0.0, 8.0, 4.0}, 7u);

    CountingVisitor visitor;
    accept_all(scene, visitor);

    CHECK(visitor.count_of(CommandKind::MoveTo) == 1);
    CHECK(visitor.count_of(CommandKind::LineTo) == 1);
    CHECK(visitor.count_of(CommandKind::Rect) == 1);
    CHECK(visitor.count_of(CommandKind::Arc) == 1);
    CHECK(visitor.count_of(CommandKind::Fill) == 1);
    CHECK(visitor.count_of(CommandKind::Stroke) == 1);
    CHECK(visitor.count_of(CommandKind::Text) == 1);
    CHECK(visitor.count_of(CommandKind::Image) == 1);
}

TEST_CASE("scene: 包围盒只统计几何指令") {
    const Scene scene = triangle_scene();
    const Rect box = scene.bounds();
    CHECK(box.x == 0.0);
    CHECK(box.y == 0.0);
    CHECK(box.right() == 10.0);
    CHECK(box.bottom() == 8.0);
}

TEST_CASE("scene: 空场景的包围盒是空矩形") {
    const Scene scene;
    CHECK(scene.empty());
    CHECK(scene.bounds().empty());
}

TEST_CASE("scene: 文本不参与包围盒（宽度要等字体度量）") {
    Scene scene;
    SceneBuilder(scene).text(Point{3.0, 4.0}, "很长很长的一段文字");
    CHECK(scene.size() == 1);
    CHECK(scene.bounds().empty());
}

TEST_CASE("scene: 场景可整体比较，clear 后回到空") {
    const Scene a = triangle_scene();
    Scene b = triangle_scene();
    CHECK(a == b);

    b.commands()[0] = MoveTo{Point{9.0, 9.0}};
    CHECK(a != b);

    b.clear();
    CHECK(b.empty());
    CHECK(a != b);
}

}  // namespace
