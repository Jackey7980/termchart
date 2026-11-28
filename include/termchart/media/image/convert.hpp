// convert.hpp —— 位图与标量场的互转（GUI 与终端共用）
//
// 本文件是**规格清单之外的补充**，理由：GUI 侧的图片控件要把 RGBA 位图转成标量场
// 再喂给 raster/（盲文/密度预览），终端侧的图像预览走的也是同一条转换。把唯一实现
// 放在 image/ 里，比让每个上层各写一遍亮度公式可靠。
//
// 两点约定（刻意如此，别再"优化"）：
//   * 亮度用 Rec. 601 系数（0.299 / 0.587 / 0.114），**不做 gamma 校正** ——
//     GUI 与终端必须看到同一个亮度，加 gamma 只会让两边不一致且难以解释；
//   * alpha 不参与亮度，而是**乘进结果**：全透明像素在场里就是 0，
//     不会因为"透明黑"而变成一块黑斑。
//
// 纯函数、无状态；列入 header-only 清单。
#ifndef TERMCHART_IMAGE_CONVERT_HPP
#define TERMCHART_IMAGE_CONVERT_HPP

#include <algorithm>
#include <cstdint>

#include "termchart/core/rgb.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/field.hpp"

namespace termchart::image {

namespace detail {
inline constexpr float kLumaR = 0.299f;
inline constexpr float kLumaG = 0.587f;
inline constexpr float kLumaB = 0.114f;
}  // namespace detail

// 位图 → 标量场（尺寸相同）。值域 [0,1]。
[[nodiscard]] inline FloatField luminance_field(const Bitmap& bitmap) {
    FloatField field(bitmap.width(), bitmap.height(), 0.0f);
    for (std::uint32_t y = 0; y < bitmap.height(); ++y) {
        for (std::uint32_t x = 0; x < bitmap.width(); ++x) {
            const float luma = detail::kLumaR * static_cast<float>(bitmap.at(x, y, 0)) +
                               detail::kLumaG * static_cast<float>(bitmap.at(x, y, 1)) +
                               detail::kLumaB * static_cast<float>(bitmap.at(x, y, 2));
            float value = luma / 255.0f;
            if (bitmap.channels() == 4) {
                value *= static_cast<float>(bitmap.at(x, y, 3)) / 255.0f;
            }
            field.set(x, y, std::clamp(value, 0.0f, 1.0f));
        }
    }
    return field;
}

// 位图 → 标量场（先最近邻缩放到目标尺寸，避免上游为了预览而额外做一次缩放）。
[[nodiscard]] inline FloatField luminance_field(const Bitmap& bitmap, std::uint32_t width,
                                               std::uint32_t height) {
    if (bitmap.width() == width && bitmap.height() == height) {
        return luminance_field(bitmap);
    }
    return luminance_field(bitmap.scaled_to(width, height));
}

// 标量场 → 双色位图：按阈值决定取 on 还是 off。
// alpha_from_value 为真时 alpha 取场值（把场当遮罩用），否则 alpha 恒为 255。
[[nodiscard]] inline Bitmap field_to_bitmap(const FloatField& field, Rgb on, Rgb off,
                                            double threshold = 0.5,
                                            bool alpha_from_value = false) {
    Bitmap out(field.width(), field.height(), 4);
    for (std::uint32_t y = 0; y < field.height(); ++y) {
        for (std::uint32_t x = 0; x < field.width(); ++x) {
            const float value = field.at(x, y);
            const bool lit = static_cast<double>(value) >= threshold;
            const Rgb color = lit ? on : off;
            out.set(x, y, 0, color.r);
            out.set(x, y, 1, color.g);
            out.set(x, y, 2, color.b);
            const auto alpha = alpha_from_value
                                   ? static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) *
                                                               255.0f + 0.5f)
                                   : static_cast<std::uint8_t>(255);
            out.set(x, y, 3, alpha);
        }
    }
    return out;
}

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_CONVERT_HPP
