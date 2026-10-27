// backend/vector/eps_backend.hpp —— Scene → EPS（PostScript 子集，EPSF-1.2）
//
// 设计目标：与 svg_backend / pdf_backend 同款接口形态（Options + render + bytes()/str()）。
// 范围：
//   * Scene 的子集：MoveTo / LineTo / RectOp (fill+stroke) / FillOp / StrokeOp /
//     TextOp（仅 ASCII / Helvetica 字族，不嵌入字体）/ ArcOp（以折线逼近）；
//   * 真彩 RGB（颜色类型 / PostScript rg/RG）；
//   * 单页（多页留给后续切片）。
//
// 故意不写 / 未做（与 pdf_backend 一致）：
//   * 圆弧（ArcOp 以折线逼近，非原生圆弧算子）；贝塞尔（未做）；
//   * 位图嵌入（ImageOp 落白占位矩形，未内嵌位图）；
//   * 透明度 / 混合模式；字体嵌入（用标准字体零成本，但 ASCII only）。
//
// 实现说明：EPS 是 PostScript 子集，而 PDF 的内容流算子（M/L/re/f/S/B/rg/RG/w/
// BT/Tf/Td/Tj/ET/q/Q）在 PostScript 里同样有效。因此本后端几乎逐行镜像 pdf_backend，
// 只是去掉 xref / 对象表、改为 %!PS-Adobe 头 + %%BoundingBox 包裹。坐标系同样
// 翻转（Scene y 向下 → PostScript y 向上 = height_pt_ - y）。
#ifndef TERMCHART_BACKEND_VECTOR_EPS_BACKEND_HPP
#define TERMCHART_BACKEND_VECTOR_EPS_BACKEND_HPP

#include <string>
#include <string_view>
#include <vector>

#include "termchart/backend/backend.hpp"
#include "termchart/backend/vector/backend.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::vector {

struct EpsOptions {
    double width_pt  = 612.0;   // US Letter 宽（pt）
    double height_pt = 792.0;   // US Letter 高（pt）
    bool   showpage  = true;    // 末尾 emit showpage（独立查看用；严格 EPSF 可关）
};

class EpsBackend final : public VectorBackend {
public:
    void set_options(const EpsOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const EpsOptions& options() const noexcept { return opts_; }

    void begin() override { out_.clear(); }
    void end() override;
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

    void render(const scene::Scene& scene) override;

private:
    EpsOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_eps(EpsBackend& backend,
                                                  const scene::Scene& scene);

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_EPS_BACKEND_HPP
