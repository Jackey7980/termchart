// damage.hpp —— 脏区域跟踪
//
// 与 diff.hpp 的分工：
//   * diff.hpp 回答"两个 buffer 之间哪里变了"（纯函数，可测）；
//   * 本文件维护"本帧累计的脏区"（有状态），供 renderer 决定重绘范围与
//     将来 canvas 的部分重绘。
// 存储形态：每行若干条**已合并**的半开区间 [begin, end)。
#ifndef TERMCHART_RENDER_DAMAGE_HPP
#define TERMCHART_RENDER_DAMAGE_HPP

#include <cstdint>
#include <span>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::render {

class DamageTracker {
public:
    struct Run {
        std::uint32_t begin = 0;
        std::uint32_t end = 0;

        constexpr bool operator==(const Run&) const noexcept = default;
        [[nodiscard]] constexpr bool empty() const noexcept { return end <= begin; }
    };

    struct RowRun {
        std::uint32_t row = 0;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;

        constexpr bool operator==(const RowRun&) const noexcept = default;
    };

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear() noexcept;

    TERMCHART_API void mark(std::uint32_t x, std::uint32_t y) noexcept;
    TERMCHART_API void mark_run(std::uint32_t row, std::uint32_t begin, std::uint32_t end) noexcept;
    TERMCHART_API void mark_row(std::uint32_t row) noexcept;
    TERMCHART_API void mark_all() noexcept;

    [[nodiscard]] TERMCHART_API bool dirty(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] TERMCHART_API bool dirty_row(std::uint32_t row) const noexcept;

    // 展平后的脏区列表（按行升序、区间升序）。
    [[nodiscard]] TERMCHART_API std::vector<RowRun> runs() const noexcept;

    [[nodiscard]] TERMCHART_API std::span<const Run> runs_of(std::uint32_t row) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t run_count() const noexcept;

    [[nodiscard]] TERMCHART_API std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] TERMCHART_API std::uint32_t height() const noexcept { return height_; }

private:
    TERMCHART_API void insert(std::uint32_t row, Run run) noexcept;

    std::vector<std::vector<Run>> rows_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

}  // namespace termchart::render

#endif  // TERMCHART_RENDER_DAMAGE_HPP
