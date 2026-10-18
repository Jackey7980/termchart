// backend/vector/pdf_backend.hpp —— Scene → PDF（极简 PDF 1.4）
//
// 设计目标：与 svg_backend 同款接口形态（Options + render + bytes()/str()）。
// 范围：
//   * Scene 的子集：MoveTo / LineTo / RectOp (fill+stroke) / FillOp / StrokeOp /
//     TextOp（仅 ASCII / Helvetica 字族，不嵌入字体）；
//   * 真彩 RGB（颜色类型 / PDF Color Operators rg/RG）；
//   * 内嵌 PNG（走 image::Bitmap → encode_png → /Filter /FlateDecode）；
//   * 单页（多页留给后续切片，初始只支持 1 页）；
//   * 文本流：仅 BaseFont / Helvetica（PDF 14 标准字体之一，零嵌入）。
//
// 故意不写：
//   * 圆弧 / 贝塞尔（PDF 支持但与 Scene 圆弧表示不一致，留 TODO）；
//   * 透明度 / 混合模式；
//   * 字体嵌入（用标准字体零成本，但 ASCII only）。
//
// 输出：完整 PDF 字节流（%PDF-1.4 + objects + xref + trailer），可在浏览器/Acrobat 打开。
#ifndef TERMCHART_BACKEND_VECTOR_PDF_BACKEND_HPP
#define TERMCHART_BACKEND_VECTOR_PDF_BACKEND_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "termchart/backend/backend.hpp"
#include "termchart/backend/vector/backend.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::vector {

struct PdfOptions {
    double width_pt  = 612.0;   // US Letter 宽（pt）
    double height_pt = 792.0;   // US Letter 高（pt）
    bool   compress  = true;    // 内容流走 zlib /FlateDecode（需 TERMCHART_HAVE_ZLIB）
};

class PdfBackend final : public VectorBackend {
public:
    void set_options(const PdfOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const PdfOptions& options() const noexcept { return opts_; }

    void begin() override { out_.clear(); }
    void end() override;
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

    void render(const scene::Scene& scene) override;

private:
    PdfOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_pdf(PdfBackend& backend,
                                                   const scene::Scene& scene);

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_PDF_BACKEND_HPP
