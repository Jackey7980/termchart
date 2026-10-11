// bitmap.hpp —— 彩色像素缓冲（宽 / 高 / 通道 / 像素数组 / 行对齐）
//
// 这是"图片"在库里的通用表示：protocol/（sixel/kitty/iterm2）与 image/png 都
// 面向它。通道数为 3（RGB）或 4（RGBA）；行按通道紧致对齐，row_bytes() 即步长。
// 标量强度场是另一种东西，见 image/field.hpp（raster 的输入）。
// 纯值类型，列入 header_only。
#ifndef TERMCHART_IMAGE_BITMAP_HPP
#define TERMCHART_IMAGE_BITMAP_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::image {

class Bitmap {
public:
    Bitmap() = default;
    Bitmap(std::uint32_t width, std::uint32_t height, std::uint8_t channels = 4) {
        resize(width, height, channels);
    }

    void resize(std::uint32_t width, std::uint32_t height, std::uint8_t channels = 4) {
        w_ = width;
        h_ = height;
        channels_ = (channels == 3 || channels == 4) ? channels : 4;
        pixels_.assign(static_cast<std::size_t>(row_bytes()) * static_cast<std::size_t>(h_), 0);
    }

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return w_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return h_; }
    [[nodiscard]] constexpr std::uint8_t channels() const noexcept { return channels_; }
    // 行步长（字节）：行对齐后的每行字节数。
    [[nodiscard]] constexpr std::uint32_t row_bytes() const noexcept {
        return w_ * static_cast<std::uint32_t>(channels_);
    }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return pixels_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return pixels_.empty(); }

    [[nodiscard]] std::uint8_t* data() noexcept { return pixels_.data(); }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return pixels_.data(); }

    // 越界返回 0；写越界静默忽略。
    [[nodiscard]] std::uint8_t at(std::uint32_t x, std::uint32_t y,
                                  std::uint8_t channel = 0) const noexcept {
        if (x >= w_ || y >= h_ || channel >= channels_) {
            return 0;
        }
        return pixels_[static_cast<std::size_t>(y) * row_bytes() +
                       static_cast<std::size_t>(x) * channels_ + channel];
    }

    void set(std::uint32_t x, std::uint32_t y, std::uint8_t channel, std::uint8_t value) noexcept {
        if (x >= w_ || y >= h_ || channel >= channels_) {
            return;
        }
        pixels_[static_cast<std::size_t>(y) * row_bytes() +
                static_cast<std::size_t>(x) * channels_ + channel] = value;
    }

    void fill(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept {
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                set(x, y, 0, r);
                set(x, y, 1, g);
                set(x, y, 2, b);
                if (channels_ == 4) {
                    set(x, y, 3, a);
                }
            }
        }
    }

    [[nodiscard]] bool operator==(const Bitmap& other) const noexcept {
        return w_ == other.w_ && h_ == other.h_ && channels_ == other.channels_ &&
               pixels_ == other.pixels_;
    }
    [[nodiscard]] bool operator!=(const Bitmap& other) const noexcept {
        return !(*this == other);
    }

private:
    std::uint32_t w_ = 0;
    std::uint32_t h_ = 0;
    std::uint8_t channels_ = 4;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_BITMAP_HPP
