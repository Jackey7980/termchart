// backend/vector/backend.hpp —— 矢量物种子接口（Scene → bytes）
//
// 矢量导出把 Scene（非完备图形 IR）翻成某种向量格式。与 snapshot 不同，这里
// 输入是**保留几何**的 Scene，因此能无损缩放（SVG viewBox / TikZ 坐标）。
// 后端不做跨渲染模型转换：Scene 进来，字节流出去。
#ifndef TERMCHART_BACKEND_VECTOR_BACKEND_HPP
#define TERMCHART_BACKEND_VECTOR_BACKEND_HPP

#include <string>
#include <string_view>

#include "termchart/backend/backend.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::backend::vector {

struct VectorOptions {
    double scale = 12.0;   // 1 场景单位 → 多少输出单位（px / pt）
    bool autofit = true;   // 用 Scene::bounds() 自动定 viewBox
    double width = 640.0;  // autofit=false 时使用的画布尺寸
    double height = 480.0;
    // 文字的字号（场景单位）。0 表示沿用旧行为 = scale：scene 单位当作像素时
    // 恰好是 1px 字（历史金标如此）。以"格"为场景单位的图形（如 graph）要把
    // 它设成 1.0——字号是场景单位而不是输出单位，否则会放大 scale 倍。
    double font_size = 0.0;
};

class VectorBackend : public Backend {
public:
    ~VectorBackend() override;

    void set_options(const VectorOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const VectorOptions& options() const noexcept { return opts_; }

    // 矢量物种的输入恒为 Scene。
    virtual void render(const scene::Scene& scene) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    VectorOptions opts_;
    std::string out_;
};

[[nodiscard]] TERMCHART_API std::string render_vector(VectorBackend& backend,
                                                      const scene::Scene& scene);

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_BACKEND_HPP
