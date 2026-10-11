// crosshair.hpp —— 十字准线（悬停读数覆盖层）
//
// 职责：把一次"悬停/定位"落成画布上的水平 + 垂直准线（可选轴侧读数），并在
// 下一次绘制或 clear() 时**精确还原**被改写的格 —— 否则悬停一次就永久污染画布
// （覆盖层必须能在撤销时把原字符与样式逐格放回去）。这是本类唯一的核心不变量。
//
// 与既有模块的关系：
//   * 只依赖 L0/L10（core + canvas），不碰 render / term / 终端；
//   * 不拥有画布，只在调用方传入的 Canvas 上叠加，故可在无终端单测里完整驱动。
//
// 叠加口径（默认"保留数据字符、只改样式/背景"）：
//   * preserve_data == true（默认）：准线经过的格**保留 cp**，只把样式换成
//     "原样式叠加 line_style"（line_style 里未指定的前景/背景保持原值，属性位按
//     或并入）。于是折线/柱状的数据字符不会被准线吃掉，只被高亮。
//   * preserve_data == false：写入连接字符（交叉点 cross_cp，横线 horizontal_cp，
//     竖线 vertical_cp），整格换成 line_style。
//
// 裁剪与退化口径：
//   * 准线**严格**裁剪在 plot_rect 内（半开区间 [x, x+w) × [y, y+h)）：区外一格不写；
//   * at 落在 plot_rect 外，或 plot_rect 任一方向不足 2 格（退化成点/线段，失去
//     "十字"语义）→ 明确不画、返回 false（对应"绘图区只有 1 格"的退化输入）；
//   * readout 是**唯一**允许落在绘图区外的元素（画在轴侧，即绘图区下边框的下方），
//     仍由画布裁剪兜底；默认 show_readout == false，需要读数时显式打开——
//     这样"draw 只改绘图区内的格"在默认配置下是硬不变量。
#ifndef TERMCHART_INTERACTION_CROSSHAIR_HPP
#define TERMCHART_INTERACTION_CROSSHAIR_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

struct CrosshairOptions {
    Rect plot_rect{};             // 绘图区（半开区间）；任一方向 < 2 格视为退化
    bool preserve_data = true;    // 默认保留数据字符、只改样式
    bool show_readout = false;    // 是否在轴侧写读数（默认关，保证"只改区内"）
    char32_t horizontal_cp = U'─';
    char32_t vertical_cp = U'│';
    char32_t cross_cp = U'┼';
    // 准线样式：默认给一层暗底，肉眼可见又不动字符。未设的很/背景不覆盖原值。
    Style line_style = Style{}.set_bg(Color::ansi16(8));
};

class Crosshair {
public:
    Crosshair() = default;
    explicit Crosshair(CrosshairOptions options) noexcept;

    // 换配置。不主动碰画布：新的配置会在下一次 draw() 时生效，而 draw() 会先还原
    // 上一次的改写，因此"换配置后重绘"不会残留旧准线。
    TERMCHART_API void set_options(CrosshairOptions options) noexcept;
    [[nodiscard]] TERMCHART_API const CrosshairOptions& options() const noexcept;

    // 绘制到格坐标 at（只画落在 plot_rect 内的部分）。readout 非空且 show_readout
    // 时，把该文本写在下边框外侧（轴侧）。返回是否真的落笔。
    // 重复调用会先还原上一次的改写再按新点重绘 —— 幂等叠加，不累积污染。
    TERMCHART_API bool draw(canvas::Canvas& canvas, Point at, std::string_view readout = {});

    // 还原全部被改写的格（逐格逐 char/样式）。返回是否真的还原过；未绘制时为 no-op。
    // 注意：若画布尺寸在绘制后被改小，被裁掉的格无法还原 —— 调用方应先 clear 再 resize。
    TERMCHART_API bool clear(canvas::Canvas& canvas);

    [[nodiscard]] TERMCHART_API bool active() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t patch_count() const noexcept;

private:
    struct Patch {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        Cell before{};
    };

    // 还原 patches_ 并清空；返回是否还原过任何格。
    bool restore(canvas::Canvas& canvas);
    // 记录 (x,y) 的原格（若在画布内且尚未记录过）；去重靠 captured_ 标记表。
    void capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y);
    // 把去重标记表重置为 width × height（全 0）。
    void reset_captured(std::uint32_t width, std::uint32_t height);

    CrosshairOptions options_{};
    std::vector<Patch> patches_;
    std::vector<std::uint8_t> captured_;  // 尺寸 = captured_w_ * captured_h_
    std::uint32_t captured_w_ = 0;
    std::uint32_t captured_h_ = 0;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_CROSSHAIR_HPP
