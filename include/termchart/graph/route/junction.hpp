// junction.hpp —— 轴对齐折线的交汇口字符
//
// 为什么把它抽成公共件：这套"按这一格有上/下/左/右哪几个方向来选方块字符"的规则
// 原先在 `src/graph/diagrams/orgchart.cpp` 与 `examples/17_uml_class.cpp` 里**各有一份**
// （各写一张 switch 表）。地铁线路图会是第三个消费者 —— 三份复制必然漂移，所以在
// 第三个出现之前把它收到这里，规则只留一份。
//
// 与 `segment_char` 的分工：`segment_char`（diagrams 骨架内部）判的是**单段线**该用
// `─`/`│`/`╱`/`╲`；本函数判的是**交汇口**（横竖相接处）该用哪个 T 形/十字字符，
// 否则折角会渲染成两个独立字符拼出的"断角"。
#ifndef TERMCHART_GRAPH_ROUTE_JUNCTION_HPP
#define TERMCHART_GRAPH_ROUTE_JUNCTION_HPP

#include "termchart/export.hpp"

namespace termchart::graph {

// 四种方向连通性 → 方块字符。规则（方块字符按"缺哪边"选）：
//
//   上+下          │      只有竖向延续
//   左+右          ─      只有横向延续
//   下+右          ┌      上、左缺
//   下+左          ┐      上、右缺
//   上+右          └      下、左缺
//   上+左          ┘      下、右缺
//   上+下+右       ├      左缺
//   上+下+左       ┤      右缺
//   左+右+下       ┬      上缺
//   左+右+上       ┴      下缺
//   四向           ┼
//
// 退化口径（刻意定义清楚，免得调用方各自猜）：
//   * **只有一个方向**：按该方向的轴返回线字符（左/右 → `─`，上/下 → `│`）；
//     单方向不是交汇口，但那里确实有一段线，返回线字符比返回空格更不容易出错。
//   * **零个方向**：返回空格（`U' '`）—— 调用方据此跳过该格。
//
// 本函数是**纯数据表**判据，故做成 header-only `constexpr`（在
// `cmake/header_only.txt` 登记）：调用方可以直接在常量表达式里用（示例里的
// `constexpr` 包装函数就依赖这一点），也没有任何运行期开销。
[[nodiscard]] constexpr char32_t junction_char(bool up, bool down, bool left,
                                               bool right) noexcept {
    switch (static_cast<unsigned>(up) | (static_cast<unsigned>(down) << 1) |
            (static_cast<unsigned>(left) << 2) | (static_cast<unsigned>(right) << 3)) {
        case 0x3:  // 上+下
            return U'\u2502';
        case 0xC:  // 左+右
            return U'\u2500';
        case 0xA:  // 下+右
            return U'\u250C';
        case 0x6:  // 下+左
            return U'\u2510';
        case 0x9:  // 上+右
            return U'\u2514';
        case 0x5:  // 上+左
            return U'\u2518';
        case 0xB:  // 上+下+右
            return U'\u251C';
        case 0x7:  // 上+下+左
            return U'\u2524';
        case 0xE:  // 左+右+下
            return U'\u252C';
        case 0xD:  // 左+右+上
            return U'\u2534';
        case 0xF:  // 四向
            return U'\u253C';
        // 单方向：不是交汇口，但那一格确实有一段线，返回对应轴的线字符。
        case 0x4:
        case 0x8:
            return U'\u2500';
        case 0x1:
        case 0x2:
            return U'\u2502';
        default:  // 零方向
            return U' ';
    }
}

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_JUNCTION_HPP
