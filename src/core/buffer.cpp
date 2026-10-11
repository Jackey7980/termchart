#include "termchart/core/buffer.hpp"

#include <algorithm>

#include "termchart/core/span_utils.hpp"

namespace termchart {

CellBuffer::CellBuffer(std::uint32_t width, std::uint32_t height) { resize(width, height); }

void CellBuffer::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    cells_.assign(count, Cell{});
    sides_.clear();  // 尺寸变化后旧索引全部失效
}

void CellBuffer::clear() {
    std::fill(cells_.begin(), cells_.end(), Cell{});
    sides_.clear();
}

Cell& CellBuffer::at(std::uint32_t x, std::uint32_t y) noexcept {
    return cells_[index_of(x, y)];
}

const Cell& CellBuffer::at(std::uint32_t x, std::uint32_t y) const noexcept {
    return cells_[index_of(x, y)];
}

std::span<Cell> CellBuffer::row(std::uint32_t y) noexcept {
    if (y >= height_) {
        return {};
    }
    return std::span<Cell>(cells_.data() + static_cast<std::size_t>(y) * width_, width_);
}

std::span<const Cell> CellBuffer::row(std::uint32_t y) const noexcept {
    if (y >= height_) {
        return {};
    }
    return std::span<const Cell>(cells_.data() + static_cast<std::size_t>(y) * width_, width_);
}

std::span<Cell> CellBuffer::cells() noexcept { return std::span<Cell>(cells_); }

std::span<const Cell> CellBuffer::cells() const noexcept { return std::span<const Cell>(cells_); }

bool CellBuffer::has_sides(std::size_t index) const noexcept {
    return sides_.find(index) != sides_.end();
}

const CellBuffer::SideBearingList* CellBuffer::sides(std::size_t index) const noexcept {
    const auto it = sides_.find(index);
    return it == sides_.end() ? nullptr : &it->second;
}

void CellBuffer::set_sides(std::size_t index, const char32_t* data, std::size_t count) {
    if (index >= cells_.size() || count == 0) {
        return;
    }
    SideBearingList& list = sides_[index];
    const std::size_t n = count < list.size() ? count : list.size();
    std::copy_n(data, n, list.begin());
    for (std::size_t i = n; i < list.size(); ++i) {
        list[i] = U'\0';
    }
}

void CellBuffer::clear_sides(std::size_t index) { sides_.erase(index); }

std::size_t CellBuffer::side_entry_count() const noexcept { return sides_.size(); }

bool CellBuffer::operator==(const CellBuffer& other) const noexcept {
    if (width_ != other.width_ || height_ != other.height_) {
        return false;
    }
    if (sides_ != other.sides_) {
        return false;
    }
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        if (cells_[i] != other.cells_[i]) {
            return false;
        }
    }
    return true;
}

bool CellBuffer::operator!=(const CellBuffer& other) const noexcept { return !(*this == other); }

}  // namespace termchart
