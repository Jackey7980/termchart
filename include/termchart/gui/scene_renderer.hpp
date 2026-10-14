// scene_renderer.hpp —— Scene → Bitmap（GUI 平行栈的第一块落地）
//
// 定位（docs/design/gui_stack.md）：GUI 与终端是平行栈，共享到 `scene`（L4）
// 为止。本文件是 GUI 侧的 `render/scene_renderer`——消费与终端/SVG **同一份**
// Scene 指令集、**同一份** StatefulVisitor 时序；区别只在"往哪画"：
// 终端往字符格画，这里往 image::Bitmap 画。
//
// 渲染质量决策（为什么这样画）：
//   * 几何（线/弧/矩形）走 **S×S 超采样 + 盒式降采样**得到抗锯齿——实现小、
//     质量对得起"生产级"，且不需要解析覆盖率这类容易写错的数学；
//   * 文本**不参与超采样**：gui/font 是位图点阵字体，超采样只会把它糊掉，
//     所以文本在下采样后按最终分辨率直绘，且**永远在几何之上**
//     （图表标签/图例压在网格线上是生产惯例）；
//   * 线宽语义与 SVG 同构：StrokeOp.width 是 Scene 单位，随 autofit 缩放。
//
// 不做的事（边界）：不解析 ImageOp 的像素（Scene 不持有位图，画占位棋盘格）；
// 不做渐变/裁剪/图层（Scene 刻意不是完备 IR，见 scene/command.hpp）。
#ifndef TERMCHART_GUI_SCENE_RENDERER_HPP
#define TERMCHART_GUI_SCENE_RENDERER_HPP

#include <string>
#include <vector>

#include "termchart/color/resolve.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/scene/stateful_visitor.hpp"

namespace termchart::gui {

struct RenderOptions {
    // 输出位图尺寸（像素）。
    std::uint32_t width = 960;
    std::uint32_t height = 540;
    // true：按 Scene bounds 自动算缩放，内容带 margin 铺满画布（与 SVG autofit 同构）；
    // false：用固定 scale，Scene 坐标即像素坐标。
    bool autofit = true;
    double margin = 24.0;  // autofit 边距（像素）
    double scale = 1.0;    // autofit=false 时生效：Scene 单位 → 像素

    // 底色与"未设色"兜底。Color::none() 的描边/文本用 foreground。
    bool transparent = false;  // true → 背景 alpha 0（供再合成），几何不透明
    Rgb background{20, 20, 20};
    Rgb foreground{229, 229, 229};

    // 等宽文本：TextOp 没有字号（Scene 不建模排版），字符格大小在这里给，
    // 单位是 Scene 坐标（随 autofit 一起缩放）。
    double text_cell_w = 9.0;
    double text_cell_h = 16.0;

    // 抗锯齿超采样倍数。1 = 关（逐像素锯齿）；2–4 = 盒式降采样；越界值收 3。
    int supersample = 3;
};

// 无状态可复用：同一 renderer 可连续 render 多份 Scene（内部状态每次重置）。
class SceneRenderer final : private scene::StatefulVisitor {
public:
    explicit SceneRenderer(RenderOptions options) noexcept;

    // 渲染整份 Scene。输出恒为 4 通道 RGBA（transparent 时背景 alpha=0）。
    [[nodiscard]] image::Bitmap render(const scene::Scene& scene);

private:
    // ── StatefulVisitor 的"往哪画" ──
    // 路径：线段在 extend_path 时立即按 current_stroke() 落到超采样缓冲——
    // 时序保证此时读到的是"这段几何出现时的描边色"，无需 end_path 再收口。
    void begin_path(const Point& from) override;
    void extend_path(const Point& from, const Point& to) override;
    void end_path() override;  // 几何已即时落盘，无需收尾动作（保持幂等）
    void emit_filled_rect(const Rect& rect, const Color& fill) override;
    void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) override;
    void emit_arc(const scene::ArcOp& arc, const Color& stroke, double width) override;
    void emit_text(const scene::TextOp& text) override;
    void emit_image(const scene::ImageOp& image) override;

    // ── 超采样缓冲上的几何原语（坐标均为 Scene → 像素换算后的值）──
    void stamp(double cx, double cy, double radius, const Rgb& rgb);
    void draw_segment(double x0, double y0, double x1, double y1, double radius, const Rgb& rgb);
    void fill_rect_ss(double x, double y, double w, double h, const Rgb& rgb);
    void stroke_rect_ss(const Rect& rect, double radius, const Rgb& rgb);

    // Scene 坐标 → 超采样像素。
    [[nodiscard]] double to_px_x(double x) const noexcept;
    [[nodiscard]] double to_px_y(double y) const noexcept;

    // 待下采样后直绘的文本（位图字体不参与超采样，理由见文件头）。
    struct PendingText {
        double x = 0.0;  // 最终像素坐标（左上角）
        double y = 0.0;
        double cell_w = 0.0;
        double cell_h = 0.0;
        Rgb color{};
        std::string text;
    };

    RenderOptions opts_{};
    // 几何变换：px = scene * view_scale + offset（像素域），超采样时再乘 ss_。
    double view_scale_ = 1.0;
    double view_tx_ = 0.0;
    double view_ty_ = 0.0;
    int ss_ = 3;
    std::uint32_t sw_ = 0;  // 超采样缓冲宽
    std::uint32_t sh_ = 0;  // 超采样缓冲高
    std::vector<std::uint8_t> buffer_;  // RGBA8，sw_ × sh_ × 4
    Rgb stroke_rgb_{};
    double stroke_radius_ = 1.0;  // 半线宽（超采样像素）
    std::vector<PendingText> texts_;
    color::ResolveOptions resolve_{};
};

}  // namespace termchart::gui

#endif  // TERMCHART_GUI_SCENE_RENDERER_HPP
