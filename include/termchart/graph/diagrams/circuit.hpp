// circuit.hpp —— 电路图（元件字符符号 + 引脚正交连线）
//
// 字符化策略：元件画成固定尺寸的字符符号——电阻是带内标的矩形框、电容是两块
// 平行板（─│ │─，板间断开）、电池是长板+短板、地是逐级收窄的横杠（│ + ─── +
// ─ + ·）；引脚连线走正交走线，转折收口成 ┌ ┐ └ ┘，中段逐格避让元件框
// （找不到空档才按 L 形直连，宁可直穿也不消失——与 route/ 的退化口径一致）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_CIRCUIT_HPP
#define TERMCHART_GRAPH_DIAGRAMS_CIRCUIT_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/port.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class ComponentKind {
    Resistor = 0,  // 电阻：5×3 矩形框，标签画在框内（放不下画到框下）
    Capacitor,     // 电容：─│ │─ 两块平行板，板间断开
    Battery,       // 电池：长板（3 行）+ 短板（1 行）
    Ground,        // 地：│ + ─── + ─ + ·（5×5，引脚在顶）
};

struct CircuitComponent {
    ComponentKind kind = ComponentKind::Resistor;
    std::string label;
    Point at{};  // 元件外框左上角（格坐标）
};

// 引脚 = 元件下标 + 引出侧；坐标由该元件外框推算（落在边框格上）。
struct CircuitPin {
    std::size_t component = 0;
    PortSide side = PortSide::East;
};

struct CircuitWire {
    CircuitPin from{};
    CircuitPin to{};
};

struct CircuitOptions {
    Style glyph_style{};
    Style label_style{};
    Style wire_style{};
};

// 就地绘制；返回元件与标签的包围盒（元件表为空返回零矩形）。
// 元件下标越界的连线跳过。
[[nodiscard]] TERMCHART_API Rect render_circuit(const std::vector<CircuitComponent>& parts,
                                                const std::vector<CircuitWire>& wires,
                                                canvas::Canvas& canvas,
                                                const CircuitOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_CIRCUIT_HPP
