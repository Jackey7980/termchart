// command.hpp —— Scene 的指令集（header-only）
//
// Scene 是**非完备图形 IR**：这里只放 vector 后端（SVG / LaTeX-TikZ）与
// canvas 都能消费的那部分图元。**不要往里塞 canvas 才有的能力**
// （例如渐变填充、裁剪路径、图层合成）——那些属于 canvas/（L10）。
//
// 指令一律用聚合体 + std::variant：
//   * variant 的 index() 就是指令类别，不需要额外的 kind 字段；
//   * 聚合体天然可 constexpr 构造、可 operator==，于是 Scene 可整体比较；
//   * 新增指令 = 往 variant 里加一个聚合体，访问者会**编译期**被逼着补全。
//
// 本文件是 header-only（见 cmake/header_only.txt），不得有同名 src 实现。
#ifndef TERMCHART_SCENE_COMMAND_HPP
#define TERMCHART_SCENE_COMMAND_HPP

#include <cstdint>
#include <string>
#include <variant>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"

namespace termchart::scene {

// 注意：聚合体**不会**隐式获得 operator==（C++20 只在你写 = default 时生成），
// 而 variant 的 == 要求每个候选都可比较——Scene 的整体比较依赖它，故逐个 default。

// ── 路径 ────────────────────────────────────────────────────────────────
struct MoveTo {
    Point to;

    constexpr bool operator==(const MoveTo&) const noexcept = default;
};

struct LineTo {
    Point to;

    constexpr bool operator==(const LineTo&) const noexcept = default;
};

// 轴对齐矩形。命名避开 core::Rect，否则成员声明会自引用。
struct RectOp {
    Rect rect;

    constexpr bool operator==(const RectOp&) const noexcept = default;
};

// 圆弧：角度为**弧度**，从 start 逆时针到 end。整圆 = {0, 2π}。
struct ArcOp {
    Point center;
    double radius = 0.0;
    double start = 0.0;
    double end = 0.0;

    constexpr bool operator==(const ArcOp&) const noexcept = default;
};

// ── 绘制状态 ────────────────────────────────────────────────────────────
// "当前路径"由消费者（canvas / vector 后端）维护，Scene 不保存路径栈——
// 保存它就等于把 canvas 的状态机搬进 IR，那正是本文件要避免的事。
struct FillOp {
    Color color;

    // Color::operator== 在库里实现（TERMCHART_API），因此这里不能是 constexpr。
    bool operator==(const FillOp& other) const noexcept { return color == other.color; }
};

struct StrokeOp {
    Color color;
    double width = 1.0;

    bool operator==(const StrokeOp& other) const noexcept {
        return color == other.color && width == other.width;
    }
};

// ── 内容 ────────────────────────────────────────────────────────────────
struct TextOp {
    Point at;
    std::string text;
    Style style;

    bool operator==(const TextOp& other) const noexcept {
        return at == other.at && text == other.text && style == other.style;
    }
};

// 图像只带**引用**：像素数据在 image/（L5），而 scene 是 L4，分层不允许
// scene 依赖 image。image_id 由调用方在导出时解析成实际位图。
struct ImageOp {
    Rect rect;
    std::uint32_t image_id = 0;

    constexpr bool operator==(const ImageOp&) const noexcept = default;
};

using Command = std::variant<MoveTo, LineTo, RectOp, ArcOp, FillOp, StrokeOp, TextOp, ImageOp>;

// 顺序与 Command 的候选列表一一对应；值即 variant 的 index()。
enum class CommandKind : std::uint8_t {
    MoveTo = 0,
    LineTo,
    Rect,
    Arc,
    Fill,
    Stroke,
    Text,
    Image,
};

constexpr CommandKind kind_of(const Command& command) noexcept {
    return static_cast<CommandKind>(command.index());
}

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_COMMAND_HPP
