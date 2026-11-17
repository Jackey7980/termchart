// test_roundtrip_scene.cpp —— Scene IR 往返与消费者时序集成测试
//
// Scene 没有文本 serialize API；往返契约建立在 Command 聚合体的 operator== 与
// Scene::operator== 上（scene.hpp 注释即写明 golden 与 roundtrip 依赖它）。本测试覆盖：
// ① 逐指令拷贝重建后 Scene 整体相等；② 逐指令 accept 遍历不丢指令；
// ③ 消费者顺序与 Scene 顺序一致；④ 空/改动场景的不相等判定。
#include <doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/scene/visitor.hpp"

namespace {

using namespace termchart;
using namespace termchart::scene;

// 造一份覆盖路径/状态/内容/分组各类指令的 Scene。
Scene make_scene() {
    Scene scene;
    SceneBuilder b(scene);
    b.move_to(Point{1.0, 2.0})
        .line_to(Point{10.0, 2.0})
        .line_to(Point{10.0, 8.0})
        .rect(Rect{0.0, 0.0, 20.0, 10.0})
        .stroke(Color::ansi16(2), 1.5)
        .fill(Color::ansi16(4))
        .text(Point{2.0, 1.0}, "roundtrip", Style{})
        .begin_group(1u)
        .move_to(Point{3.0, 3.0})
        .line_to(Point{5.0, 5.0})
        .end_group(1u);
    return scene;
}

// 记录访问顺序的 visitor：每个 visit 记一条 Command（原样回压），据此重建 Scene。
// 这样既验证了"遍历不丢指令"，又复验了"重建后整体相等"。
class RecordingVisitor final : public CommandVisitor {
public:
    using CommandVisitor::visit;  // 避免隐藏基类的扩编 visit 重载（-Woverloaded-virtual）

    void visit(const MoveTo& c) override { rebuilt.push(c); }
    void visit(const LineTo& c) override { rebuilt.push(c); }
    void visit(const RectOp& c) override { rebuilt.push(c); }
    void visit(const ArcOp& c) override { rebuilt.push(c); }
    void visit(const FillOp& c) override { rebuilt.push(c); }
    void visit(const StrokeOp& c) override { rebuilt.push(c); }
    void visit(const TextOp& c) override { rebuilt.push(c); }
    void visit(const ImageOp& c) override { rebuilt.push(c); }
    void visit(const GroupOp& c) override { rebuilt.push(c); }

    Scene rebuilt;
};

}  // namespace

TEST_CASE("integration/roundtrip_scene: 逐指令拷贝重建后整体相等") {
    const Scene original = make_scene();
    REQUIRE(original.size() > 0);

    Scene rebuilt;
    for (const Command& command : original.commands()) {
        rebuilt.push(command);
    }

    CHECK(rebuilt.size() == original.size());
    CHECK(rebuilt == original);
    for (std::size_t i = 0; i < original.size(); ++i) {
        CHECK(rebuilt.commands()[i] == original.commands()[i]);
    }
}

TEST_CASE("integration/roundtrip_scene: accept_all 遍历不丢指令且重建相等") {
    const Scene scene = make_scene();

    // kind_of 是 variant index 直映射（Scene 比较与 golden 依赖此不变量）。
    for (const Command& command : scene.commands()) {
        CHECK(static_cast<int>(kind_of(command)) == command.index());
    }

    RecordingVisitor visitor;
    accept_all(scene, visitor);
    REQUIRE(visitor.rebuilt.size() == scene.size());
    CHECK(visitor.rebuilt == scene);
}

TEST_CASE("integration/roundtrip_scene: 改动一指令即不相等，空 Scene 往返相等") {
    Scene a = make_scene();
    const Scene b = a;

    // 追加一条 → 不相等。
    Scene changed = b;
    changed.push(MoveTo{Point{99.0, 99.0}});
    CHECK(changed != b);

    // 清空后往返仍相等（两个空 Scene）。
    Scene empty_a;
    Scene empty_b;
    for (const Command& command : empty_a.commands()) {
        empty_b.push(command);
    }
    CHECK(empty_a == empty_b);
    CHECK(empty_a.size() == 0);
}
