// span_utils.hpp —— span 安全切片（header-only）
//
// CellBuffer 对外暴露 std::span<Cell>；所有"取子区间"必须走这里，
// 让越界行为在一处定义：**钳位而非抛异常**（渲染热路径不允许异常）。
#ifndef TERMCHART_CORE_SPAN_UTILS_HPP
#define TERMCHART_CORE_SPAN_UTILS_HPP

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>

namespace termchart {

// [offset, offset+n) 钳位到 [0, size)；返回安全切片。
template <class T>
constexpr std::span<T> safe_subspan(std::span<T> source, std::size_t offset,
                                    std::size_t count) noexcept {
    if (offset >= source.size()) {
        return source.subspan(source.size(), 0);
    }
    const std::size_t available = source.size() - offset;
    return source.subspan(offset, count < available ? count : available);
}

template <class T>
constexpr std::span<const T> safe_subspan(std::span<const T> source, std::size_t offset,
                                          std::size_t count) noexcept {
    if (offset >= source.size()) {
        return source.subspan(source.size(), 0);
    }
    const std::size_t available = source.size() - offset;
    return source.subspan(offset, count < available ? count : available);
}

// 只读下标访问：越界返回 nullopt。
template <class T>
constexpr std::optional<std::reference_wrapper<const T>> checked_at(std::span<const T> source,
                                                                    std::size_t index) noexcept {
    if (index >= source.size()) {
        return std::nullopt;
    }
    return std::cref(source[index]);
}

// 两段 span 是否逐元素相等（尺寸不同直接 false）。
template <class T, class U>
constexpr bool span_equal(std::span<const T> lhs, std::span<const U> rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace termchart

#endif  // TERMCHART_CORE_SPAN_UTILS_HPP
