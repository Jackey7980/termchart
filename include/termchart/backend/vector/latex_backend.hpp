// backend/vector/latex_backend.hpp —— CellBuffer → LaTeX（TikZ）
//
// 与 svg_backend 同款接口形态：Options 结构 + render 入口 + bytes()/str() 返回
// 字符串 + 便捷函数 render_latex（对齐 render_vector / render_snapshot）。
// 差异只在输入物种：按 backend/backend.hpp 的铁律"不做跨渲染模型转换"，
// 快照导出的输入恒为 CellBuffer，因此这里不继承 VectorBackend（它的 render
// 把输入钉死在 Scene），而是直接实现 Backend 生命周期。
//
// 映射口径：
//   * 坐标：格 (x, y) → TikZ (x, -y)；单位由 tikzpicture 的 x/y 选项定成
//     scale pt，y 翻转让第一行落在最上方（TikZ 坐标向上为正）。
//   * 非空格（空白格与宽字符续接格不输出）→ \node，文字经 LaTeX 转义。
//   * 横向连续的 U+2500（─）→ \draw 直线段（格中心连线）；
//     U+2588（█）→ \fill 实心矩形（左右各扩半格，视觉上无缝）。
//   * 颜色：Style 前景色经 color::resolve 解析成 RGB，用 xcolor 的
//     {rgb,255:red,R;green,G;blue,B} 模型内联；None 不发色键，沿用默认黑。
#ifndef TERMCHART_BACKEND_VECTOR_LATEX_BACKEND_HPP
#define TERMCHART_BACKEND_VECTOR_LATEX_BACKEND_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "termchart/backend/backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::vector {

struct LatexOptions {
    double scale = 12.0;             // 1 格 → 多少 pt（tikzpicture 的 x/y 单位）
    bool standalone = true;          // 包 documentclass/document 还是裸 tikzpicture
    bool trim_trailing_blank = true; // 行尾"空白且无样式"的格不输出
};

class LatexBackend final : public Backend {
public:
    void set_options(const LatexOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const LatexOptions& options() const noexcept { return opts_; }

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

    // 快照物种的输入恒为 CellBuffer。
    void render(const CellBuffer& buffer);

private:
    // 该行有效宽度：trim 时裁掉行尾"空白且无样式"的格（语义同 SnapshotBackend）。
    [[nodiscard]] std::uint32_t row_width(const CellBuffer& buffer,
                                          std::uint32_t y) const noexcept;

    LatexOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_latex(LatexBackend& backend,
                                                     const CellBuffer& buffer);

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_LATEX_BACKEND_HPP
