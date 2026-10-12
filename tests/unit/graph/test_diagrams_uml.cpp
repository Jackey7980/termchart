// test_diagrams_uml.cpp —— 五种 UML/ER 图渲染（类图 / 时序图 / 用例图 / 活动图 / ER 图）
#include <doctest.h>

#include <cstdint>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/er_diagram.hpp"
#include "termchart/graph/diagrams/uml_activity.hpp"
#include "termchart/graph/diagrams/uml_class.hpp"
#include "termchart/graph/diagrams/uml_sequence.hpp"
#include "termchart/graph/diagrams/uml_usecase.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

bool has_char(const canvas::Canvas& canvas, char32_t cp) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == cp) {
                return true;
            }
        }
    }
    return false;
}

bool has_arrow(const canvas::Canvas& canvas) {
    return has_char(canvas, U'\u25B6') || has_char(canvas, U'\u25C0') ||
           has_char(canvas, U'\u25B2') || has_char(canvas, U'\u25BC');
}

// 某一行上是否连续出现给定 ASCII 文本（确认标签没被涂掉）。
bool row_contains(const canvas::Canvas& canvas, const std::string& text) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x + text.size() <= canvas.width(); ++x) {
            bool match = true;
            for (std::size_t k = 0; k < text.size(); ++k) {
                if (canvas.at(x + static_cast<std::uint32_t>(k), y).cp !=
                    static_cast<char32_t>(text[k])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }
    return false;
}

// 泛化边：Shape 为基类（from），Circle 为派生类。
Graph make_class_graph(bool assoc) {
    Graph graph;
    const NodeId shape = graph.add_node("Shape").id();
    graph.find(shape)->attributes().set("attrs", "# name : string");
    graph.find(shape)->attributes().set("ops", "+ area() : double");
    const NodeId circle = graph.add_node("Circle").id();
    graph.find(circle)->attributes().set("attrs", "- r : double");
    graph.find(circle)->attributes().set("ops", "+ area() : double");
    Edge* edge = graph.add_edge(shape, circle);
    if (assoc) {
        edge->attributes().set("kind", "assoc");
    }
    return graph;
}

}  // namespace

TEST_CASE("diagrams/uml_class: 三栏类框 + 空心三角泛化") {
    Graph graph = make_class_graph(false);
    canvas::Canvas canvas(60, 12);
    const Rect bounds = render_uml_class(graph, canvas, UmlClassOptions{});

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // ┌ 类框
    CHECK(has_char(canvas, U'\u251C'));  // ├ 栏分隔线
    CHECK(has_char(canvas, U'\u2524'));  // ┤
    CHECK(has_char(canvas, U'\u25C1'));  // ◁ 指向基类的空心三角
    CHECK(row_contains(canvas, "Shape"));
    CHECK(row_contains(canvas, "area() : double"));  // 操作栏内容没被涂掉
}

TEST_CASE("diagrams/uml_class: assoc 边退化为普通箭头") {
    Graph graph = make_class_graph(true);
    canvas::Canvas canvas(60, 12);
    (void)render_uml_class(graph, canvas, UmlClassOptions{});
    CHECK(has_arrow(canvas));
    CHECK_FALSE(has_char(canvas, U'\u25C1'));
}

TEST_CASE("diagrams/uml_class: 空图返回零矩形") {
    Graph graph;
    canvas::Canvas canvas(40, 10);
    const Rect bounds = render_uml_class(graph, canvas, UmlClassOptions{});
    CHECK(bounds.w == 0.0);
    CHECK(bounds.h == 0.0);
}

TEST_CASE("diagrams/uml_sequence: 头框 + 生命线 + 消息箭头 + 激活条 + 便签") {
    SeqModel model;
    SeqParticipant alice;
    alice.key = "alice";
    alice.label = "Alice";
    SeqParticipant bob;
    bob.key = "bob";
    bob.label = "Bob";
    model.participants.push_back(alice);
    model.participants.push_back(bob);

    SeqMessage ask;
    ask.from = "alice";
    ask.to = "bob";
    ask.text = "hi";
    ask.kind = SeqArrowKind::Solid;
    ask.activate_target = true;
    SeqMessage reply;
    reply.from = "bob";
    reply.to = "alice";
    reply.text = "ok";
    reply.kind = SeqArrowKind::Dotted;
    model.messages.push_back(ask);
    model.messages.push_back(reply);

    SeqNote note;
    note.placement = SeqNote::Placement::Over;
    note.anchor = "alice";
    note.anchor_b = "bob";
    note.text = "pend";
    note.after = 0;
    model.notes.push_back(note);

    canvas::Canvas canvas(60, 16);
    const Rect bounds = render_uml_sequence(model, canvas, UmlSequenceOptions{});

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u2502'));  // │ 生命线
    CHECK(has_char(canvas, U'\u25B6'));  // ▶ 实心箭头（alice → bob）
    CHECK(has_char(canvas, U'\u25C0'));  // ◀ 回程箭头（bob → alice）
    CHECK(has_char(canvas, U'\u2504'));  // ┄ 虚线
    CHECK(has_char(canvas, U'\u2588'));  // █ 激活条
    CHECK(has_char(canvas, U'['));       // 便签的方括号
    CHECK(row_contains(canvas, "Alice"));
    CHECK(row_contains(canvas, "Bob"));
    CHECK(row_contains(canvas, "hi"));
    CHECK(row_contains(canvas, "pend"));
}

TEST_CASE("diagrams/uml_sequence: 未知参与者的消息被跳过、空模型返回零矩形") {
    SeqModel empty;
    canvas::Canvas canvas(40, 10);
    Rect bounds = render_uml_sequence(empty, canvas, UmlSequenceOptions{});
    CHECK(bounds.w == 0.0);

    SeqModel model;
    SeqParticipant p;
    p.key = "a";
    model.participants.push_back(p);
    SeqMessage ghost;
    ghost.from = "ghost";
    ghost.to = "a";
    model.messages.push_back(ghost);
    bounds = render_uml_sequence(model, canvas, UmlSequenceOptions{});
    CHECK(bounds.w > 0.0);  // 渲染不崩溃，消息被跳过
    CHECK_FALSE(has_arrow(canvas));
}

TEST_CASE("diagrams/uml_usecase: 小人 + 椭圆 + 无箭头关联线") {
    Graph graph;
    const NodeId user = graph.add_node("user").id();
    graph.find(user)->attributes().set("kind", "actor");
    const NodeId login = graph.add_node("login").id();
    (void)graph.add_edge(user, login);

    canvas::Canvas canvas(50, 14);
    const Rect bounds = render_uml_usecase(graph, canvas, UmlUsecaseOptions{});

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'O'));       // 小人头
    CHECK(has_char(canvas, U'|'));       // 身
    CHECK(has_char(canvas, U'\u2571'));  // ╱ 椭圆斜边
    CHECK(row_contains(canvas, "user"));
    CHECK(row_contains(canvas, "login"));
    CHECK_FALSE(has_arrow(canvas));  // 关联线不带箭头

    // 参与者默认站左侧（只作为 from），用例在中列。
    const Rect user_box = graph.find(user)->box();
    const Rect login_box = graph.find(login)->box();
    CHECK(user_box.x < login_box.x);
}

TEST_CASE("diagrams/uml_usecase: side=right 的参与者站右侧") {
    Graph graph;
    const NodeId user = graph.add_node("user").id();
    graph.find(user)->attributes().set("kind", "actor");
    graph.find(user)->attributes().set("side", "right");
    const NodeId login = graph.add_node("login").id();
    (void)graph.add_edge(user, login);

    canvas::Canvas canvas(50, 14);
    (void)render_uml_usecase(graph, canvas, UmlUsecaseOptions{});
    CHECK(graph.find(user)->box().x > graph.find(login)->box().x);
}

TEST_CASE("diagrams/uml_activity: 圆角动作框 + 菱形判定 + 自动起止标记 + 守卫标签") {
    Graph graph;
    const NodeId input = graph.add_node("input").id();
    const NodeId check = graph.add_node("check").id();
    graph.find(check)->attributes().set("kind", "decision");
    const NodeId work = graph.add_node("work").id();
    (void)graph.add_edge(input, check);
    (void)graph.add_edge(check, work, "yes");

    canvas::Canvas canvas(64, 14);
    UmlActivityOptions opts;
    opts.layout.h_gap = 10.0;  // 给守卫标签留位
    const Rect bounds = render_uml_activity(graph, canvas, opts);

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u256D'));  // ╭ 圆角动作框
    CHECK(has_char(canvas, U'\u25CF'));  // ● 自动起点（入度 0 的 input）
    CHECK(has_char(canvas, U'\u25CE'));  // ◎ 自动终点（出度 0 的 work）
    CHECK(has_char(canvas, U'y'));       // 守卫标签落到画布上
    CHECK(row_contains(canvas, "input"));
    CHECK(row_contains(canvas, "work"));

    // 自动标记写回了图：显式属性优先，再次渲染不再改标记。
    CHECK(graph.find(input)->attributes().get("initial") == "1");
    CHECK(graph.find(work)->attributes().get("final") == "1");
}

TEST_CASE("diagrams/uml_activity: 显式标记优先，另一端自动兜底") {
    Graph graph;
    const NodeId a = graph.add_node("aaa").id();
    const NodeId b = graph.add_node("bbb").id();
    graph.find(b)->attributes().set("initial", "1");  // 刻意把起点标在中间节点
    (void)graph.add_edge(a, b);

    canvas::Canvas canvas(50, 12);
    (void)render_uml_activity(graph, canvas, UmlActivityOptions{});
    CHECK(has_char(canvas, U'\u25CF'));        // ● 用的是显式标记（在 b 上）
    CHECK(has_char(canvas, U'\u25CE'));        // ◎ 自动兜底（出度 0 的 b）
    CHECK_FALSE(graph.find(a)->attributes().has("initial"));
    CHECK_FALSE(graph.find(a)->attributes().has("final"));  // a 有出边，final 兜底落在 b
}

TEST_CASE("diagrams/er_diagram: 实体表框 + 鸭掌记号 + 关系名") {
    Graph graph;
    const NodeId user = graph.add_node("User").id();
    graph.find(user)->attributes().set("fields", "id: int\nname: text");
    const NodeId order = graph.add_node("Order").id();
    graph.find(order)->attributes().set("fields", "id: int");
    Edge* rel = graph.add_edge(user, order, "places");
    rel->attributes().set("card_from", "1");
    rel->attributes().set("card_to", "N");

    canvas::Canvas canvas(70, 16);
    ErDiagramOptions opts;
    opts.layout.h_gap = 12.0;  // 给关系名标签留位
    const Rect bounds = render_er_diagram(graph, canvas, opts);

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // ┌ 表框
    CHECK(has_char(canvas, U'\u251C'));  // ├ 名称/字段分隔线
    CHECK(row_contains(canvas, "User"));
    CHECK(row_contains(canvas, "Order"));
    CHECK(row_contains(canvas, "name: text"));  // 字段行没被涂掉
    CHECK(row_contains(canvas, "places"));      // 关系名
    CHECK(has_char(canvas, U'<'));              // N 端的鸭掌
}

TEST_CASE("diagrams/er_diagram: 0..1 基数画圆圈记号") {
    Graph graph;
    const NodeId user = graph.add_node("User").id();
    const NodeId order = graph.add_node("Order").id();
    Edge* rel = graph.add_edge(user, order);
    rel->attributes().set("card_from", "1");
    rel->attributes().set("card_to", "0..1");

    canvas::Canvas canvas(70, 16);
    (void)render_er_diagram(graph, canvas, ErDiagramOptions{});
    CHECK(has_char(canvas, U'\u25CB'));  // ○ 零
}

TEST_CASE("diagrams/er_diagram: 基数拼写解析") {
    CHECK(parse_er_cardinality("1") == ErCardinality::One);
    CHECK(parse_er_cardinality("") == ErCardinality::One);
    CHECK(parse_er_cardinality("whatever") == ErCardinality::One);
    CHECK(parse_er_cardinality("N") == ErCardinality::Many);
    CHECK(parse_er_cardinality("many") == ErCardinality::Many);
    CHECK(parse_er_cardinality("*") == ErCardinality::Many);
    CHECK(parse_er_cardinality("0..1") == ErCardinality::ZeroOrOne);
    CHECK(parse_er_cardinality("o|") == ErCardinality::ZeroOrOne);
}

TEST_CASE("diagrams/er_diagram: 空图返回零矩形") {
    Graph graph;
    canvas::Canvas canvas(40, 10);
    const Rect bounds = render_er_diagram(graph, canvas, ErDiagramOptions{});
    CHECK(bounds.w == 0.0);
    CHECK(bounds.h == 0.0);
}

TEST_CASE("diagrams/uml_activity: 空图返回零矩形") {
    Graph graph;
    canvas::Canvas canvas(40, 10);
    const Rect bounds = render_uml_activity(graph, canvas, UmlActivityOptions{});
    CHECK(bounds.w == 0.0);
    CHECK(bounds.h == 0.0);
}

TEST_CASE("diagrams/uml_usecase: 空图返回零矩形") {
    Graph graph;
    canvas::Canvas canvas(40, 10);
    const Rect bounds = render_uml_usecase(graph, canvas, UmlUsecaseOptions{});
    CHECK(bounds.w == 0.0);
    CHECK(bounds.h == 0.0);
}
