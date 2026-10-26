// sink.hpp —— 布局落笔的抽象出口
//
// 为什么需要这层：轴、网格、标题、图例的**布局决策**（刻度落在第几列、
// 标签怎么对齐、拐角画在哪一格）只应该有一份；但"落笔"的目标有两种：
//   * canvas::Canvas —— 终端字符格；
//   * Scene —— 矢量 IR（svg / pdf / 未来的交互层）。
// 之前每个 draw_xxx 都直接吃 `canvas::Canvas&`，想导 Scene 就只能把布局
// 逻辑抄一遍，两份实现对同一个刻度迟早会算出不一样的列——这正是
// `canvas/scene_renderer.hpp` 开头警告的"两份解释会分叉"。
//
// 接口刻意按**语义**命名而不是按字符：
//   * put()  —— 字符艺术（角、刻度钩），只能以字符形式存在；
//   * text() —— 一段文本，起点是左上格；
//   * line() —— "这里有一条线"，用实线还是虚线由调用方按语义指定，
//               至于最终是 ─ 还是盲文点，交给 sink 的实现决定。
#ifndef TERMCHART_LAYOUT_SINK_HPP
#define TERMCHART_LAYOUT_SINK_HPP

#include <cstdint>
#include <string_view>

#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::layout {

// 线的语义类别。Solid 是"数据/边框"这类实线；Dashed 是网格、参考线。
// 分开的理由：终端侧它们是不同字符（─ vs ┄），Scene 侧实线可以走矢量
// 描边、虚线必须保留虚线外观（盲文光栅化会把虚线变成实线）。
enum class LineStyle : std::uint8_t { Solid, Dashed };

// 终端侧的默认线字符。放在公开头里，是因为 CanvasSink 与 SceneSink 都要用
// 同一套字形；两处各写一份 U+2500 迟早会出现一个 ─、一个 ━。
inline constexpr char32_t kSolidH = U'\u2500';   // ─
inline constexpr char32_t kSolidV = U'\u2502';   // │
inline constexpr char32_t kDashedH = U'\u2504';  // ┄
inline constexpr char32_t kDashedV = U'\u2506';  // ┆

class Sink {
public:
    virtual ~Sink();

    // 在格坐标处写一个字符。
    virtual void put(Point at, char32_t cp, const Style& style) = 0;
    // 一段文本，起点是左上格；越界由实现自行裁剪。
    virtual void text(Point at, std::string_view value, const Style& style) = 0;
    // 两端都是格坐标，含端点。
    virtual void line(Point a, Point b, LineStyle kind, const Style& style) = 0;
};

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_SINK_HPP
