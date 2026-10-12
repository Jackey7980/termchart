// test_diagrams_relation.cpp —— 五种关系图渲染（和弦 / 弧线 / 邻接矩阵 / 语法树 / 电路）
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/adjacency_matrix.hpp"
#include "termchart/graph/diagrams/arc_diagram.hpp"
#include "termchart/graph/diagrams/chord.hpp"
#include "termchart/graph/diagrams/circuit.hpp"
#include "termchart/graph/diagrams/syntax_tree.hpp"

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

// 某一行上是否连续出现给定 ASCII 文本（用于确认标签落到画布上）。
bool row_contains(const canvas::Canvas& canvas, const std::string& text) {
    if (text.empty() || canvas.height() == 0 || canvas.width() < text.size()) {
        return false;
    }
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

bool is_arc_char(char32_t cp) {
    return cp == U'\u2500' || cp == U'\u2502' || cp == U'\u2571' || cp == U'\u2572';
}

}  // namespace

TEST_CASE("diagrams/chord: 环 + 弦带 + 标签") {
    std::vector<ChordEntity> entities{{"A"}, {"B"}, {"C"}, {"D"}};
    std::vector<ChordLink> links{{0, 1, 2.0}, {1, 2, 1.0}, {2, 3, 0.5}, {3, 0, 1.0}};

    canvas::Canvas canvas(48, 22);
    ChordOptions opts;
    opts.radius = 7.0;
    const Rect bounds = render_chord(entities, links, canvas, opts);
    CHECK(bounds.w > 0.0);
    CHECK(bounds.h > 0.0);
    // 弦与环的走向字符都该出现。
    CHECK(has_char(canvas, U'\u2500'));
    CHECK(has_char(canvas, U'\u2502'));
    CHECK(row_contains(canvas, "A"));
    CHECK(row_contains(canvas, "D"));
}

TEST_CASE("diagrams/chord: 值定带宽、零值/越界/自弦跳过、空表安全") {
    std::vector<ChordEntity> entities{{"A"}, {"B"}};
    std::vector<ChordLink> links{{0, 1, 3.0}, {0, 0, 2.0}, {0, 9, 1.0}, {1, 0, 0.0}};

    canvas::Canvas canvas(40, 20);
    ChordOptions opts;
    opts.radius = 6.0;
    opts.max_band = 3;
    (void)render_chord(entities, links, canvas, opts);
    CHECK(row_contains(canvas, "A"));
    CHECK(row_contains(canvas, "B"));
    CHECK(has_char(canvas, U'\u2500'));

    canvas::Canvas empty(20, 10);
    const Rect zero = render_chord({}, {}, empty, ChordOptions{});
    CHECK(zero.w == 0.0);
}

TEST_CASE("diagrams/arc_diagram: 跨度大弧更高、空表安全") {
    std::vector<ArcNode> nodes{{"a"}, {"b"}, {"c"}, {"d"}};
    std::vector<ArcLink> links{{0, 3}, {1, 2}};

    canvas::Canvas canvas(40, 14);
    const Rect bounds = render_arc_diagram(nodes, links, canvas, ArcDiagramOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "a"));
    CHECK(has_char(canvas, U'\u2500'));
    // 斜段或竖段至少其一：弧不只是横线。
    CHECK((has_char(canvas, U'\u2571') || has_char(canvas, U'\u2572') ||
           has_char(canvas, U'\u2502')));

    // 跨度大的弧（0→3）顶点更高：最高弧字符行距节点行至少 3 行。
    canvas::Canvas big(40, 20);
    (void)render_arc_diagram(nodes, links, big, ArcDiagramOptions{});
    std::uint32_t highest = big.height();
    for (std::uint32_t y = 0; y < big.height(); ++y) {
        for (std::uint32_t x = 0; x < big.width(); ++x) {
            if (is_arc_char(big.at(x, y).cp) && big.at(x, y).cp != U'\u2500') {
                highest = std::min(highest, y);
            }
        }
    }
    CHECK(highest + 3 < big.height() - 1);

    canvas::Canvas empty(20, 10);
    const Rect zero = render_arc_diagram({}, {}, empty, ArcDiagramOptions{});
    CHECK(zero.w == 0.0);
}

TEST_CASE("diagrams/arc_diagram: 自连接画小拱、越界弧跳过") {
    std::vector<ArcNode> nodes{{"a"}, {"b"}};
    std::vector<ArcLink> links{{0, 0}, {0, 5}};

    canvas::Canvas canvas(30, 12);
    const Rect bounds = render_arc_diagram(nodes, links, canvas, ArcDiagramOptions{});
    CHECK(row_contains(canvas, "a"));
    CHECK(bounds.w > 0.0);
}

TEST_CASE("diagrams/adjacency_matrix: 标尺 + 值分档块字符") {
    std::vector<std::string> names{"x", "yy", "zzz"};
    std::vector<std::vector<double>> matrix{
        {0.0, 1.0, 0.0},
        {0.5, 0.0, 5.0},
        {0.0, 0.2, 0.0},
    };

    canvas::Canvas canvas(40, 8);
    const Rect bounds = render_adjacency_matrix(names, matrix, canvas, AdjacencyMatrixOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(bounds.h == 4.0);
    CHECK(has_char(canvas, U'\u2588'));  // 强值 5.0
    CHECK(has_char(canvas, U'\u00B7'));  // 零值
    CHECK(row_contains(canvas, "yy"));
    CHECK(row_contains(canvas, "zzz"));
}

TEST_CASE("diagrams/adjacency_matrix: 缺行缺列按零处理、空表安全") {
    std::vector<std::string> names{"a", "b", "c"};
    std::vector<std::vector<double>> matrix{
        {1.0, 2.0},
        {3.0},
    };

    canvas::Canvas canvas(40, 8);
    (void)render_adjacency_matrix(names, matrix, canvas, AdjacencyMatrixOptions{});
    CHECK(row_contains(canvas, "a"));
    CHECK(has_char(canvas, U'\u2588'));

    canvas::Canvas empty(20, 6);
    const Rect zero = render_adjacency_matrix({}, {}, empty, AdjacencyMatrixOptions{});
    CHECK(zero.w == 0.0);
}

TEST_CASE("diagrams/syntax_tree: ├ └ ─ 连接 + │ 延续线") {
    SyntaxNode tree{"S",
                    {
                        {"NP", {{"Det", {}}, {"N", {}}}},
                        {"VP", {{"V", {}}}},
                    }};

    canvas::Canvas canvas(40, 12);
    const Rect bounds = render_syntax_tree(tree, canvas, SyntaxTreeOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "S"));
    CHECK(row_contains(canvas, "Det"));
    CHECK(has_char(canvas, U'\u251C'));  // ├
    CHECK(has_char(canvas, U'\u2514'));  // └
    CHECK(has_char(canvas, U'\u2500'));  // ─
    CHECK(has_char(canvas, U'\u2502'));  // │（NP 不是末子，其下方延续）
    CHECK(has_char(canvas, U'V'));
}

TEST_CASE("diagrams/syntax_tree: 叶子树与空标签安全") {
    SyntaxNode leaf{"root", {}};
    canvas::Canvas canvas(20, 4);
    const Rect bounds = render_syntax_tree(leaf, canvas, SyntaxTreeOptions{});
    CHECK(row_contains(canvas, "root"));
    CHECK(bounds.h == 1.0);

    SyntaxNode empty{"", {}};
    canvas::Canvas blank(20, 4);
    const Rect zero = render_syntax_tree(empty, blank, SyntaxTreeOptions{});
    CHECK(zero.w == 0.0);
}

TEST_CASE("diagrams/circuit: 元件符号 + 直线引脚连线") {
    std::vector<CircuitComponent> parts{
        {ComponentKind::Battery, "V1", Point{2.0, 2.0}},
        {ComponentKind::Resistor, "R1", Point{14.0, 2.0}},
        {ComponentKind::Ground, "", Point{16.0, 8.0}},
        {ComponentKind::Capacitor, "C1", Point{2.0, 9.0}},
    };
    std::vector<CircuitWire> wires{
        {{0, PortSide::East}, {1, PortSide::West}},    // 电池 → 电阻（同一行直线）
        {{1, PortSide::South}, {2, PortSide::North}},  // 电阻 → 地（同一列直线）
    };

    canvas::Canvas canvas(40, 16);
    const Rect bounds = render_circuit(parts, wires, canvas, CircuitOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // 电阻框
    CHECK(row_contains(canvas, "R1"));   // 框内标签
    CHECK(row_contains(canvas, "V1"));   // 框下标签
    CHECK(has_char(canvas, U'\u2500'));  // 连线 / 电容板
    CHECK(has_char(canvas, U'\u00B7'));  // 地符末级
}

TEST_CASE("diagrams/circuit: 越界引脚跳过、空表安全") {
    std::vector<CircuitComponent> parts{{ComponentKind::Resistor, "R", Point{2.0, 2.0}}};
    std::vector<CircuitWire> wires{
        {{0, PortSide::East}, {9, PortSide::West}},
    };

    canvas::Canvas canvas(24, 10);
    const Rect bounds = render_circuit(parts, wires, canvas, CircuitOptions{});
    CHECK(row_contains(canvas, "R"));
    CHECK(bounds.w > 0.0);

    canvas::Canvas empty(20, 8);
    const Rect zero = render_circuit({}, {}, empty, CircuitOptions{});
    CHECK(zero.w == 0.0);
}
