#include "termchart/render/damage.hpp"

#include <algorithm>

namespace termchart::render {

void DamageTracker::resize(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    rows_.assign(height, {});
}

void DamageTracker::clear() noexcept {
    for (auto& row : rows_) {
        row.clear();
    }
}

void DamageTracker::insert(std::uint32_t row, Run run) noexcept {
    if (row >= rows_.size() || run.empty()) {
        return;
    }
    auto& target = rows_[row];
    // 插入并合并所有相交/相邻（含端点相接）的区间。
    std::uint32_t begin = run.begin;
    std::uint32_t end = run.end;
    std::vector<Run> merged;
    merged.reserve(target.size() + 1);
    for (const Run& existing : target) {
        if (existing.end < begin || existing.begin > end) {
            merged.push_back(existing);
            continue;
        }
        begin = std::min(begin, existing.begin);
        end = std::max(end, existing.end);
    }
    merged.push_back(Run{begin, end});
    std::sort(merged.begin(), merged.end(),
              [](const Run& a, const Run& b) { return a.begin < b.begin; });
    target = std::move(merged);
}

void DamageTracker::mark(std::uint32_t x, std::uint32_t y) noexcept {
    if (x >= width_ || y >= height_) {
        return;
    }
    insert(y, Run{x, x + 1});
}

void DamageTracker::mark_run(std::uint32_t row, std::uint32_t begin, std::uint32_t end) noexcept {
    if (row >= height_ || begin >= end) {
        return;
    }
    const std::uint32_t clamped_end = std::min(end, width_);
    insert(row, Run{std::min(begin, width_), clamped_end});
}

void DamageTracker::mark_row(std::uint32_t row) noexcept { mark_run(row, 0, width_); }

void DamageTracker::mark_all() noexcept {
    for (std::uint32_t row = 0; row < height_; ++row) {
        mark_row(row);
    }
}

bool DamageTracker::dirty(std::uint32_t x, std::uint32_t y) const noexcept {
    if (y >= rows_.size()) {
        return false;
    }
    for (const Run& run : rows_[y]) {
        if (x >= run.begin && x < run.end) {
            return true;
        }
    }
    return false;
}

bool DamageTracker::dirty_row(std::uint32_t row) const noexcept {
    return row < rows_.size() && !rows_[row].empty();
}

std::vector<DamageTracker::RowRun> DamageTracker::runs() const noexcept {
    std::vector<RowRun> out;
    for (std::uint32_t row = 0; row < rows_.size(); ++row) {
        for (const Run& run : rows_[row]) {
            out.push_back(RowRun{row, run.begin, run.end});
        }
    }
    return out;
}

std::span<const DamageTracker::Run> DamageTracker::runs_of(std::uint32_t row) const noexcept {
    if (row >= rows_.size()) {
        return {};
    }
    return std::span<const Run>(rows_[row]);
}

std::size_t DamageTracker::run_count() const noexcept {
    std::size_t total = 0;
    for (const auto& row : rows_) {
        total += row.size();
    }
    return total;
}

}  // namespace termchart::render
