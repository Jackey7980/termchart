// style.hpp —— 语义样式（属性位 + 前景 + 背景）
//
// Style 与后端无关：它不认识 SGR，不知道 A'tuin 终端能否显示斜体。
// 从 Style 到字节流的唯一转换点在 render/sgr.hpp。
//
// Color 用"带判别标识的值"而非 variant：Cell 是逐帧遍历百万次的热结构，
// sizeof(Color) 必须小而固定（8 字节），不能承担 variant 的分支与额外存储。
#ifndef TERMCHART_CORE_STYLE_HPP
#define TERMCHART_CORE_STYLE_HPP

#include <cstdint>

#include "termchart/core/attr.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"

namespace termchart {

enum class ColorKind : std::uint8_t {
    None = 0,       // 未指定：沿用终端默认色
    Ansi16 = 1,     // 0..15
    Indexed = 2,    // 0..255（含 16..231 立方格，232..255 灰阶）
    Rgb = 3,        // 真彩
};

class Color {
public:
    constexpr Color() noexcept = default;

    static constexpr Color none() noexcept { return Color{}; }
    static constexpr Color ansi16(std::uint8_t index) noexcept {
        return Color{ColorKind::Ansi16, static_cast<std::uint8_t>(index & 0x0Fu), Rgb{}};
    }
    static constexpr Color indexed(std::uint8_t index) noexcept {
        return Color{ColorKind::Indexed, index, Rgb{}};
    }
    static constexpr Color rgb(Rgb value) noexcept { return Color{ColorKind::Rgb, 0, value}; }
    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        return Color{ColorKind::Rgb, 0, Rgb{r, g, b}};
    }

    [[nodiscard]] constexpr ColorKind kind() const noexcept { return kind_; }
    [[nodiscard]] constexpr bool is_none() const noexcept { return kind_ == ColorKind::None; }

    // Ansi16 / Indexed 的索引值；其它 kind 返回 0。
    [[nodiscard]] constexpr std::uint8_t index() const noexcept { return index_; }
    [[nodiscard]] constexpr Rgb rgb() const noexcept { return rgb_; }

    TERMCHART_API bool operator==(const Color& other) const noexcept;
    TERMCHART_API bool operator!=(const Color& other) const noexcept;

private:
    constexpr Color(ColorKind kind, std::uint8_t index, Rgb value) noexcept
        : rgb_(value), index_(index), kind_(kind) {}

    Rgb rgb_{};
    std::uint8_t index_ = 0;
    ColorKind kind_ = ColorKind::None;
};

class Style {
public:
    constexpr Style() noexcept = default;

    [[nodiscard]] static constexpr Style plain() noexcept { return Style{}; }

    constexpr Style& set_attr(Attr attr, bool on = true) noexcept {
        attrs_ = on ? (attrs_ | mask_of(attr)) : without(attrs_, attr);
        return *this;
    }
    constexpr Style& set_fg(Color color) noexcept {
        fg_ = color;
        return *this;
    }
    constexpr Style& set_bg(Color color) noexcept {
        bg_ = color;
        return *this;
    }

    [[nodiscard]] constexpr AttrMask attrs() const noexcept { return attrs_; }
    [[nodiscard]] constexpr Color fg() const noexcept { return fg_; }
    [[nodiscard]] constexpr Color bg() const noexcept { return bg_; }
    [[nodiscard]] constexpr bool has_attr(Attr attr) const noexcept {
        return has_flag(attrs_, attr);
    }

    TERMCHART_API bool operator==(const Style& other) const noexcept;
    TERMCHART_API bool operator!=(const Style& other) const noexcept;

private:
    Color fg_{};
    Color bg_{};
    AttrMask attrs_ = kNoAttrs;
};

// 常用语义色：给 ui 组件默认样式用，避免到处魔数。
inline constexpr Rgb kDefaultFg{204, 204, 204};
inline constexpr Rgb kDefaultBg{0, 0, 0};

}  // namespace termchart

#endif  // TERMCHART_CORE_STYLE_HPP
