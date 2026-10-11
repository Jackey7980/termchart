// wiregrid.hpp —— 连线占位栅格（避免路由互相压线）
//
// 路由算法把候选线段先放进 WireGrid 探测：整段可用才真正占用。占用是
// **原子**的——中途碰到已占用的格，整段都不占用并返回 0，便于调用方直接换
// 下一条候选路径重试，不会留下半截占用。
#ifndef TERMCHART_GRAPH_WIREGRID_HPP
#define TERMCHART_GRAPH_WIREGRID_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::graph {

class WireGrid {
public:
    WireGrid() = default;
    WireGrid(std::uint32_t width, std::uint32_t height);

    void resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t occupied_count() const noexcept { return occupied_; }
    [[nodiscard]] bool empty() const noexcept { return occupied_ == 0; }

    [[nodiscard]] bool in_bounds(std::uint32_t x, std::uint32_t y) const noexcept {
        return in_range(x, y);
    }
    // 越界视为未占用；需要区分"越界"时先查 in_bounds。
    [[nodiscard]] bool occupied(std::uint32_t x, std::uint32_t y) const noexcept;

    // 单格占用：越界或已占用返回 false，且不改动状态。
    bool occupy(std::uint32_t x, std::uint32_t y);
    void release(std::uint32_t x, std::uint32_t y);
    void clear();

    // 只探测不占用：整段都在界内且未占用才返回 true。
    [[nodiscard]] bool can_occupy_h(std::uint32_t x0, std::uint32_t x1,
                                    std::uint32_t y) const noexcept;
    [[nodiscard]] bool can_occupy_v(std::uint32_t y0, std::uint32_t y1,
                                    std::uint32_t x) const noexcept;

    // 原子占用水平/垂直线段；成功返回占用的格数，失败返回 0 且不改动状态。
    std::size_t occupy_h(std::uint32_t x0, std::uint32_t x1, std::uint32_t y);
    std::size_t occupy_v(std::uint32_t y0, std::uint32_t y1, std::uint32_t x);

private:
    [[nodiscard]] bool in_range(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < width_ && y < height_;
    }
    [[nodiscard]] std::size_t index_of(std::uint32_t x, std::uint32_t y) const noexcept {
        return static_cast<std::size_t>(y) * width_ + x;
    }

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::size_t occupied_ = 0;
    std::vector<std::uint8_t> cells_;
};

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_WIREGRID_HPP
