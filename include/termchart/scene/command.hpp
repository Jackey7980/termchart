// command.hpp —— Scene 的指令集（header-only）
//
// Scene 是**通用 2D 矢量 IR**：收纳 SVG / LaTeX-TikZ / canvas / PDF / GUI
// 都能消费的图元与状态指令（含路径、变换、裁剪、渐变、透明度、虚线、
// 线宽/线帽、混合模式）。把"状态机"放进 IR，可避免在各后端重复实现。
//
// 指令一律用聚合体 + std::variant：
//   * variant 的 index() 就是指令类别，不需要额外的 kind 字段；
//   * 聚合体天然可 constexpr 构造、可 operator==，于是 Scene 可整体比较。
//
// 扩编约定（2026-10-26）：
//   * 新增指令一律**追加在 variant 末尾**；CommandKind 同步追加。kind_of()
//     是 index 直映射，插在中间会把 0..8 的数值顶翻（那套值要进快照/序列化）。
//   * CommandVisitor / StatefulVisitor 对新指令提供**默认空 visit**，旧后端
//     无需改动即可编译；要真实渲染的后端自行 override 对应 hook。
//
// 本文件是 header-only（见 cmake/header_only.txt），不得有同名 src 实现。
#ifndef TERMCHART_SCENE_COMMAND_HPP
#define TERMCHART_SCENE_COMMAND_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
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

// 图像只带**引用**（image_id），不持有像素：矢量 IR 的职责是"描述画什么"，
// 把位图塞进来会让一份 Scene 的体积与内存随图片大小失控，也让它无法被廉价复制。
// image_id 由调用方在导出/渲染时解析成 image/ 里的实际位图。
struct ImageOp {
    Rect rect;
    std::uint32_t image_id = 0;

    constexpr bool operator==(const ImageOp&) const noexcept = default;
};

// ── 结构 ────────────────────────────────────────────────────────────────
// 语义分组：begin=true 开组，false 闭组；可嵌套，必须成对。
// 它只回答"从这条到那条之间的图元属于哪个组"，**不改变绘制顺序、不改变
// 任何像素**——这正是它与"给每个图元加 group 字段"的区别：分组是区间，
// 不是图元的属性，所以增删分组不会碰到任何一个几何指令。
// 不认识分组的消费者（光栅化、SVG/PDF 静态导出）直接忽略即可，语义与
// SVG 的 <g> 同构。
//
// 位置：**追加在 variant 末尾**。kind_of() 是 index 直映射，插在中间会把
// 0..7 的数值全部顶翻——而那套数值是要进快照和序列化的。
struct GroupOp {
    std::uint32_t id = 0;
    bool begin = true;

    constexpr bool operator==(const GroupOp&) const noexcept = default;
};

// ── 高级路径 / 状态（2026-10-26 扩编：升级为更完整的 2D 矢量 IR）──────────
// 这些指令追加在 variant 末尾：SVG / TikZ / canvas / PDF / GUI 均消费，
// 故收进 IR 避免各后端重复实现状态机。CommandVisitor / StatefulVisitor 对
// 其提供默认空 visit，旧后端无需改动。

// 三次贝塞尔曲线段：from = 当前点（同 LineTo 语义），ctrl1/ctrl2 控制点，end 终点。
struct CurveTo {
    Point ctrl1;
    Point ctrl2;
    Point end;

    constexpr bool operator==(const CurveTo&) const noexcept = default;
};

// 2×3 仿射矩阵 [[a,c,e],[b,d,f]] · (x,y,1)：a/d 缩放、b/c 剪切、e/f 平移。
struct Transform {
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, e = 0.0, f = 0.0;

    constexpr bool operator==(const Transform&) const noexcept = default;
};
struct PushTransformOp {
    Transform matrix;
    constexpr bool operator==(const PushTransformOp&) const noexcept = default;
};
struct PopTransformOp {
    constexpr bool operator==(const PopTransformOp&) const noexcept = default;
};

// 裁剪区：intersect=true 与现有裁剪区取交；false 覆盖为 region。
struct ClipOp {
    Rect region;
    bool intersect = true;
    constexpr bool operator==(const ClipOp&) const noexcept = default;
};
// 开启/重置当前子路径（其后首个 LineTo/CurveTo 重新发 M）。
struct BeginPathOp {
    constexpr bool operator==(const BeginPathOp&) const noexcept = default;
};
// 闭合当前子路径（连回起点）。
struct ClosePathOp {
    constexpr bool operator==(const ClosePathOp&) const noexcept = default;
};

struct GradientStop {
    Rgb color{};
    double offset = 0.0;  // ∈ [0,1]
    constexpr bool operator==(const GradientStop&) const noexcept = default;
};
// 线性渐变填充状态：作用于其后 Fill / 带填充色的 RectOp。
struct LinearGradientOp {
    Point start;
    Point end;
    std::vector<GradientStop> stops;
    bool operator==(const LinearGradientOp& o) const noexcept {
        return start == o.start && end == o.end && stops == o.stops;
    }
};
struct RadialGradientOp {
    Point center;
    double radius = 0.0;
    std::vector<GradientStop> stops;
    bool operator==(const RadialGradientOp& o) const noexcept {
        return center == o.center && radius == o.radius && stops == o.stops;
    }
};
// 全局透明度：其后图元的颜色按 alpha 缩放（alpha=1 恒等）。
struct GlobalAlphaOp {
    double alpha = 1.0;
    constexpr bool operator==(const GlobalAlphaOp&) const noexcept = default;
};
// 虚线：dashes 为"实、空、实、空…"像素长；长度为奇数时末项复用首项；offset 起步偏移。
struct DashOp {
    double offset = 0.0;
    std::vector<double> dashes;
    bool operator==(const DashOp& o) const noexcept {
        return offset == o.offset && dashes == o.dashes;
    }
};

enum class StrokeCap : std::uint8_t { Butt, Round, Square };
struct SetLineWidthOp {
    double width = 1.0;
    constexpr bool operator==(const SetLineWidthOp&) const noexcept = default;
};
struct SetStrokeCapOp {
    StrokeCap cap = StrokeCap::Butt;
    constexpr bool operator==(const SetStrokeCapOp&) const noexcept = default;
};

enum class BlendMode : std::uint8_t { Normal, Multiply, Screen, Overlay };
struct BlendModeOp {
    BlendMode mode = BlendMode::Normal;
    constexpr bool operator==(const BlendModeOp&) const noexcept = default;
};

using Command =
    std::variant<MoveTo, LineTo, RectOp, ArcOp, FillOp, StrokeOp, TextOp, ImageOp, GroupOp,
                CurveTo, PushTransformOp, PopTransformOp, ClipOp, BeginPathOp, ClosePathOp,
                LinearGradientOp, RadialGradientOp, GlobalAlphaOp, DashOp, SetLineWidthOp,
                SetStrokeCapOp, BlendModeOp>;

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
    Group,
    Curve,
    PushTransform,
    PopTransform,
    Clip,
    BeginPath,
    ClosePath,
    LinearGradient,
    RadialGradient,
    GlobalAlpha,
    Dash,
    SetLineWidth,
    SetStrokeCap,
    BlendMode,
};

constexpr CommandKind kind_of(const Command& command) noexcept {
    return static_cast<CommandKind>(command.index());
}

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_COMMAND_HPP
