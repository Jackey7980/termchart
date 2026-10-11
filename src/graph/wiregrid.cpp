// wiregrid.cpp —— 连线占位栅格实现
#include "termchart/graph/wiregrid.hpp"

#include <algorithm>
#include <cstddef>

namespace termchart::graph {

WireGrid::WireGrid(std::uint32_t width, std::uint32_t height) { resize(width, height); }

void WireGrid::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    cells_.assign(static_cast<std::size_t>(width) * height, 0);
    occupied_ = 0;
}

bool WireGrid::occupied(std::uint32_t x, std::uint32_t y) const noexcept {
    return in_range(x, y) && cells_[index_of(x, y)] != 0;
}

bool WireGrid::occupy(std::uint32_t x, std::uint32_t y) {
    if (!in_range(x, y) || cells_[index_of(x, y)] != 0) {
        return false;
    }
    cells_[index_of(x, y)] = 1;
    ++occupied_;
    return true;
}

void WireGrid::release(std::uint32_t x, std::uint32_t y) {
    if (in_range(x, y) && cells_[index_of(x, y)] != 0) {
        cells_[index_of(x, y)] = 0;
        --occupied_;
    }
}

void WireGrid::clear() {
    std::fill(cells_.begin(), cells_.end(), static_cast<std::uint8_t>(0));
    occupied_ = 0;
}

bool WireGrid::can_occupy_h(std::uint32_t x0, std::uint32_t x1,
                            std::uint32_t y) const noexcept {
    if (x0 > x1) {
        std::swap(x0, x1);
    }
    if (y >= height_ || x1 >= width_) {
        return false;
    }
    for (std::uint32_t x = x0; x <= x1; ++x) {
        if (cells_[index_of(x, y)] != 0) {
            return false;
        }
    }
    return true;
}

bool WireGrid::can_occupy_v(std::uint32_t y0, std::uint32_t y1,
                            std::uint32_t x) const noexcept {
    if (y0 > y1) {
        std::swap(y0, y1);
    }
    if (x >= width_ || y1 >= height_) {
        return false;
    }
    for (std::uint32_t y = y0; y <= y1; ++y) {
        if (cells_[index_of(x, y)] != 0) {
            return false;
        }
    }
    return true;
}

std::size_t WireGrid::occupy_h(std::uint32_t x0, std::uint32_t x1, std::uint32_t y) {
    if (!can_occupy_h(x0, x1, y)) {
        return 0;
    }
    if (x0 > x1) {
        std::swap(x0, x1);
    }
    for (std::uint32_t x = x0; x <= x1; ++x) {
        cells_[index_of(x, y)] = 1;
        ++occupied_;
    }
    return static_cast<std::size_t>(x1 - x0) + 1;
}

std::size_t WireGrid::occupy_v(std::uint32_t y0, std::uint32_t y1, std::uint32_t x) {
    if (!can_occupy_v(y0, y1, x)) {
        return 0;
    }
    if (y0 > y1) {
        std::swap(y0, y1);
    }
    for (std::uint32_t y = y0; y <= y1; ++y) {
        cells_[index_of(x, y)] = 1;
        ++occupied_;
    }
    return static_cast<std::size_t>(y1 - y0) + 1;
}

}  // namespace termchart::graph
