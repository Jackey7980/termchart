// buffer.hpp —— CellBuffer：连续网格 + 稀疏侧面表
//
// 全库的**中枢结构**（L0）：
//   * 终端实时路径：canvas to_buffer() → CellBuffer → render/renderer → 字节流；
//   * snapshot 导出：backend/snapshot/* 直接吃 CellBuffer；
//   * 差分开销由 prev/cur 两个 CellBuffer 逐行为单位比较（见 render/diff.hpp）。
//
// 侧面表是**稀疏**的（unordered_map<index, Sides>）：绝大多数单元没有组合记号，
// 逐单元存一个数组会让 CellBuffer 尺寸直接翻倍。
//
// 注意：CellBuffer 持有堆资源，**禁止放进 arena**（见 core/arena.hpp）。
#ifndef TERMCHART_CORE_BUFFER_HPP
#define TERMCHART_CORE_BUFFER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "termchart/core/cell.hpp"
#include "termchart/core/grapheme.hpp"
#include "termchart/export.hpp"

namespace termchart {

class CellBuffer {
public:
    // 稀疏侧面表的值类型：定长内联数组，避免侧面项逐个堆分配。
    using SideBearingList = std::array<char32_t, kMaxSideBearings>;

    CellBuffer() = default;
    TERMCHART_API CellBuffer(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();  // 全部单元置空白，清空侧面表

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return cells_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return cells_.empty(); }

    [[nodiscard]] constexpr bool contains(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < width_ && y < height_;
    }

    TERMCHART_API Cell& at(std::uint32_t x, std::uint32_t y) noexcept;
    TERMCHART_API const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept;

    // 线性索引版本：热路径避免乘法。（索引 = y * width + x）
    [[nodiscard]] constexpr std::size_t index_of(std::uint32_t x,
                                                 std::uint32_t y) const noexcept {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    // 对外暴露 span：调用方不得保存跨越 resize 的 span。
    TERMCHART_API std::span<Cell> row(std::uint32_t y) noexcept;
    TERMCHART_API std::span<const Cell> row(std::uint32_t y) const noexcept;
    TERMCHART_API std::span<Cell> cells() noexcept;
    TERMCHART_API std::span<const Cell> cells() const noexcept;

    // ── 稀疏侧面表 ──────────────────────────────────────────────────────
    [[nodiscard]] TERMCHART_API bool has_sides(std::size_t index) const noexcept;
    [[nodiscard]] TERMCHART_API const SideBearingList* sides(std::size_t index) const noexcept;
    // count 为实际有效项数（≤ kMaxSideBearings）；超出部分被截尾。
    TERMCHART_API void set_sides(std::size_t index, const char32_t* data, std::size_t count);
    TERMCHART_API void clear_sides(std::size_t index);
    [[nodiscard]] TERMCHART_API std::size_t side_entry_count() const noexcept;

    TERMCHART_API bool operator==(const CellBuffer& other) const noexcept;
    TERMCHART_API bool operator!=(const CellBuffer& other) const noexcept;

private:
    std::vector<Cell> cells_;
    std::unordered_map<std::size_t, SideBearingList> sides_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

}  // namespace termchart

#endif  // TERMCHART_CORE_BUFFER_HPP
