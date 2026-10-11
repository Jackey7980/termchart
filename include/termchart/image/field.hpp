// field.hpp —— 光栅器消费的标量强度场（FloatField）
//
// raster/ 的所有光栅器都把"连续标量场"当成输入：场里每格是一个 0..1 的强度
// （密度 / 高度 / 命中次数归一化后的值）。它不属于"位图"（那是 image/bitmap.hpp
// 的彩色像素缓冲），因此单独成文件，避免一个 bitmap.hpp 既当像素又当标量。
// 纯值类型（无虚函数、无堆外状态），列入 header_only。
#ifndef TERMCHART_IMAGE_FIELD_HPP
#define TERMCHART_IMAGE_FIELD_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::image {

// 行主序的 2D 强度场，值约定在 [0,1]。越界访问返回 0（方便光栅器按 subpixel
// 网格采样时忽略边缘外）。
class FloatField {
public:
    FloatField() = default;
    FloatField(std::uint32_t width, std::uint32_t height, float value = 0.0f) {
        resize(width, height, value);
    }

    void resize(std::uint32_t width, std::uint32_t height, float value = 0.0f) {
        w_ = width;
        h_ = height;
        data_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value);
    }

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return w_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return h_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return data_.empty(); }

    // 越界返回 0.0f（const 只读路径，光栅器依赖此行为）。
    [[nodiscard]] float at(std::uint32_t x, std::uint32_t y) const noexcept {
        if (x >= w_ || y >= h_) {
            return 0.0f;
        }
        return data_[static_cast<std::size_t>(y) * w_ + x];
    }

    // 写路径：调用方须保证在界内。
    [[nodiscard]] float& at(std::uint32_t x, std::uint32_t y) noexcept {
        return data_[static_cast<std::size_t>(y) * w_ + x];
    }

    void set(std::uint32_t x, std::uint32_t y, float value) noexcept {
        if (x < w_ && y < h_) {
            data_[static_cast<std::size_t>(y) * w_ + x] = value;
        }
    }

    void fill(float value) noexcept { data_.assign(data_.size(), value); }

    [[nodiscard]] const float* data() const noexcept { return data_.data(); }

private:
    std::uint32_t w_ = 0;
    std::uint32_t h_ = 0;
    std::vector<float> data_;
};

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_FIELD_HPP
