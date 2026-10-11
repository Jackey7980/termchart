// opts.hpp —— Opts 链式配置
//
// 面向"一行出图"的入口：所有可选参数都在这一处，且**每个都有默认值**，
// 于是 tc::quick::hello() 之类的调用可以完全零配置。
// 语义：未设置的项（std::optional 为空）表示"由库自行决定"。
#ifndef TERMCHART_API_OPTS_HPP
#define TERMCHART_API_OPTS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "termchart/core/color_depth.hpp"
#include "termchart/export.hpp"

namespace termchart::api {

class Opts {
public:
    // ── 尺寸 ──────────────────────────────────────────────────────────
    TERMCHART_API Opts& width(std::uint16_t value) noexcept;
    TERMCHART_API Opts& height(std::uint16_t value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> width() const noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> height() const noexcept;

    // ── 颜色 ──────────────────────────────────────────────────────────
    TERMCHART_API Opts& color_depth(ColorDepth value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<ColorDepth> color_depth() const noexcept;

    // ── 文本模式 ──────────────────────────────────────────────────────
    // unicode == false 时一切绘制退化到 ASCII（制表符 → '+'、块字符 → '#'）。
    TERMCHART_API Opts& unicode(bool value) noexcept;
    TERMCHART_API Opts& ascii_only() noexcept;
    [[nodiscard]] TERMCHART_API bool unicode() const noexcept;

    // ── 输出 ──────────────────────────────────────────────────────────
    // newline：是否在输出末尾补一个换行（默认补）。
    TERMCHART_API Opts& newline(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool newline() const noexcept;

    // reset：帧尾是否复位 SGR（默认复位，避免污染后续输出）。
    TERMCHART_API Opts& reset_at_end(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool reset_at_end() const noexcept;

private:
    std::optional<std::uint16_t> width_;
    std::optional<std::uint16_t> height_;
    std::optional<ColorDepth> depth_;
    bool unicode_ = true;
    bool newline_ = true;
    bool reset_ = true;
};

}  // namespace termchart::api

#endif  // TERMCHART_API_OPTS_HPP
