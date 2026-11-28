// bitmap.hpp —— 彩色像素缓冲（宽 / 高 / 通道 / 像素数组 / 行对齐）
//
// 这是"图片"在库里的通用表示：protocol/（sixel/kitty/iterm2）与 image/png 都
// 面向它。通道数为 3（RGB）或 4（RGBA）；行按通道紧致对齐，row_bytes() 即步长。
// 标量强度场是另一种东西，见 image/field.hpp（raster 的输入）。
// 纯值类型，列入 header_only。
#ifndef TERMCHART_IMAGE_BITMAP_HPP
#define TERMCHART_IMAGE_BITMAP_HPP

#include <algorithm>
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

    [[nodiscard]] bool contains(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < w_ && y < h_;
    }

    // ---- GUI / 合成侧常用的原语（终端侧也用得上：sixel 缩略、PNG 截图拼版）----

    // 局部填充，越界部分裁剪。
    void fill_rect(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                   std::uint8_t r, std::uint8_t g, std::uint8_t b,
                   std::uint8_t a = 255) noexcept {
        for (std::uint32_t row = y; row < y + h && row < h_; ++row) {
            for (std::uint32_t col = x; col < x + w && col < w_; ++col) {
                set(col, row, 0, r);
                set(col, row, 1, g);
                set(col, row, 2, b);
                if (channels_ == 4) {
                    set(col, row, 3, a);
                }
            }
        }
    }

    // 把 src 以左上角 (dx, dy) 叠加到本图，源 alpha 做 source-over 混合。
    // 偏移用**带符号**类型：GUI 里"有一部分在窗口外"是常态，越界裁剪即可。
    void blit_over(const Bitmap& src, int dx, int dy) noexcept {
        if (src.empty() || empty() || src.channels() < 4) {
            return;  // 无 alpha 的源无法混合，请先 with_alpha()
        }
        for (std::uint32_t sy = 0; sy < src.height(); ++sy) {
            const int ty = dy + static_cast<int>(sy);
            if (ty < 0 || ty >= static_cast<int>(h_)) {
                continue;
            }
            for (std::uint32_t sx = 0; sx < src.width(); ++sx) {
                const int tx = dx + static_cast<int>(sx);
                if (tx < 0 || tx >= static_cast<int>(w_)) {
                    continue;
                }
                const double sa = src.at(sx, sy, 3) / 255.0;
                if (sa <= 0.0) {
                    continue;
                }
                const double da = channels_ == 4 ? at(static_cast<std::uint32_t>(tx),
                                                     static_cast<std::uint32_t>(ty), 3) /
                                                      255.0
                                                 : 1.0;
                const double out_a = sa + da * (1.0 - sa);
                for (std::uint32_t ch = 0; ch < 3; ++ch) {
                    const double sc = src.at(sx, sy, static_cast<std::uint8_t>(ch)) / 255.0;
                    const double dc = at(static_cast<std::uint32_t>(tx),
                                         static_cast<std::uint32_t>(ty),
                                         static_cast<std::uint8_t>(ch)) /
                                      255.0;
                    const double blended =
                        out_a > 0.0 ? (sc * sa + dc * da * (1.0 - sa)) / out_a : 0.0;
                    set(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty),
                        static_cast<std::uint8_t>(ch),
                        static_cast<std::uint8_t>(blended * 255.0 + 0.5));
                }
                if (channels_ == 4) {
                    set(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty), 3,
                        static_cast<std::uint8_t>(out_a * 255.0 + 0.5));
                }
            }
        }
    }

    // 最近邻缩放：**不做插值**，避免引入原图没有的颜色（调色板/像素画场景需要）。
    [[nodiscard]] Bitmap scaled_to(std::uint32_t width, std::uint32_t height) const {
        Bitmap out(width, height, channels_);
        if (out.empty() || empty() || w_ == 0 || h_ == 0) {
            return out;
        }
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint32_t sy =
                std::min(h_ - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * h_ /
                                                             height));
            for (std::uint32_t x = 0; x < width; ++x) {
                const std::uint32_t sx =
                    std::min(w_ - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * w_ /
                                                                 width));
                for (std::uint8_t ch = 0; ch < channels_; ++ch) {
                    out.set(x, y, ch, at(sx, sy, ch));
                }
            }
        }
        return out;
    }

    // 通道转换：3 → 4（补 alpha），4 → 3（丢 alpha）。已是目标通道数时原样返回。
    [[nodiscard]] Bitmap with_alpha(std::uint8_t alpha = 255) const {
        if (channels_ == 4) {
            return *this;
        }
        Bitmap out(w_, h_, 4);
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                out.set(x, y, 0, at(x, y, 0));
                out.set(x, y, 1, at(x, y, 1));
                out.set(x, y, 2, at(x, y, 2));
                out.set(x, y, 3, alpha);
            }
        }
        return out;
    }

    [[nodiscard]] Bitmap without_alpha() const {
        if (channels_ == 3) {
            return *this;
        }
        Bitmap out(w_, h_, 3);
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                out.set(x, y, 0, at(x, y, 0));
                out.set(x, y, 1, at(x, y, 1));
                out.set(x, y, 2, at(x, y, 2));
            }
        }
        return out;
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
