// grapheme.hpp —— 组合序列（grapheme cluster）的**数据结构**
//
// 分层约定（见 ROADMAP 阶段 1"一旦定错后续全错"）：
//   * 本文件**只定义结构，不做分割**；Splitter 在 util/grapheme.hpp（L1）；
//   * 宽度模型是"主码点 + 侧面列表"（side bearing）：Cell 存主码点，
//     CellBuffer 用稀疏表保存额外的组合记号。
//
// 为什么定长内联数组而非 vector：CellBuffer 一帧内可能有大量侧面项，
// 逐个堆分配会毁掉帧率与局部性。上限来自 UAX #15 的建议：
// 实用文本极少超过 7 个组合记号，超限则丢弃尾部（并在 add 时返回 false）。
#ifndef TERMCHART_CORE_GRAPHEME_HPP
#define TERMCHART_CORE_GRAPHEME_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "termchart/export.hpp"

namespace termchart {

inline constexpr std::size_t kMaxSideBearings = 7;

class GraphemeCluster {
public:
    constexpr GraphemeCluster() noexcept = default;
    constexpr explicit GraphemeCluster(char32_t base) noexcept : base_(base) {}

    [[nodiscard]] constexpr char32_t base() const noexcept { return base_; }
    constexpr void set_base(char32_t cp) noexcept { base_ = cp; }

    // 追加组合记号（side bearing）。已满返回 false，调用方应放弃尾部而非 "抛异常：渲染路径无异常。
    TERMCHART_API bool add_side_bearing(char32_t cp) noexcept;

    [[nodiscard]] constexpr std::size_t side_count() const noexcept { return count_; }
    [[nodiscard]] constexpr const char32_t* sides() const noexcept { return sides_.data(); }
    [[nodiscard]] constexpr bool has_sides() const noexcept { return count_ != 0; }
    constexpr void clear_sides() noexcept { count_ = 0; }

    TERMCHART_API bool operator==(const GraphemeCluster& other) const noexcept;
    TERMCHART_API bool operator!=(const GraphemeCluster& other) const noexcept;

private:
    char32_t base_ = U' ';
    std::array<char32_t, kMaxSideBearings> sides_{};
    std::uint8_t count_ = 0;
};

}  // namespace termchart

#endif  // TERMCHART_CORE_GRAPHEME_HPP
