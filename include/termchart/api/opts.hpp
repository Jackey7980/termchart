// opts.hpp —— ApiOptions 链式配置（旧名 `Opts`，见文件末尾的兼容别名）
//
// 面向"一行出图"的入口：所有可选参数都在这一处，且**每个都有默认值**，
// 于是 tc::quick::hello() 之类的调用可以完全零配置。
// 语义：未设置的项（std::optional 为空）表示"由库自行决定"。
//
// 命名：全库其余位置一律 `XxxOptions`（MetroOptions / SankeyOptions / …），
// 这里原名 `Opts` 是唯一的例外；为"同一语义同一命名"收敛成 `ApiOptions`，
// 旧名以别名保留，因此**不破坏**任何既有调用点。
#ifndef TERMCHART_API_OPTS_HPP
#define TERMCHART_API_OPTS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "termchart/core/color_depth.hpp"
#include "termchart/export.hpp"

namespace termchart::api {

class ApiOptions {
public:
    // ── 尺寸 ──────────────────────────────────────────────────────────
    TERMCHART_API ApiOptions& width(std::uint16_t value) noexcept;
    TERMCHART_API ApiOptions& height(std::uint16_t value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> width() const noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> height() const noexcept;

    // ── 颜色 ──────────────────────────────────────────────────────────
    TERMCHART_API ApiOptions& color_depth(ColorDepth value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<ColorDepth> color_depth() const noexcept;

    // ── 文本模式 ──────────────────────────────────────────────────────
    // unicode == false 时一切绘制退化到 ASCII（制表符 → '+'、块字符 → '#'）。
    TERMCHART_API ApiOptions& unicode(bool value) noexcept;
    TERMCHART_API ApiOptions& ascii_only() noexcept;
    [[nodiscard]] TERMCHART_API bool unicode() const noexcept;

    // ── 输出 ──────────────────────────────────────────────────────────
    // newline：是否在输出末尾补一个换行（默认补）。
    TERMCHART_API ApiOptions& newline(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool newline() const noexcept;

    // reset：帧尾是否复位 SGR（默认复位，避免污染后续输出）。
    TERMCHART_API ApiOptions& reset_at_end(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool reset_at_end() const noexcept;

private:
    std::optional<std::uint16_t> width_;
    std::optional<std::uint16_t> height_;
    std::optional<ColorDepth> depth_;
    bool unicode_ = true;
    bool newline_ = true;
    bool reset_ = true;
};

// 兼容别名：老代码写 `api::Opts` 仍然成立（全库只有这一处叫 Opts）。
using Opts = ApiOptions;

}  // namespace termchart::api

#endif  // TERMCHART_API_OPTS_HPP
