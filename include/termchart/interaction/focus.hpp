// focus.hpp —— 焦点管理（在给定的可聚焦顺序上前后移动）
#ifndef TERMCHART_INTERACTION_FOCUS_HPP
#define TERMCHART_INTERACTION_FOCUS_HPP

#include <cstdint>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::interaction {

class FocusChain {
public:
    // id == 0 表示"无焦点"。
    TERMCHART_API void set(std::uint32_t id) noexcept;
    [[nodiscard]] TERMCHART_API std::uint32_t current() const noexcept;
    [[nodiscard]] TERMCHART_API bool is_focused(std::uint32_t id) const noexcept;
    TERMCHART_API void clear() noexcept;

    // 在 order 内移动；当前不在 order 中时落到首/末项。order 为空则不变。
    TERMCHART_API void next(const std::vector<std::uint32_t>& order) noexcept;
    TERMCHART_API void prev(const std::vector<std::uint32_t>& order) noexcept;

private:
    std::uint32_t current_ = 0;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_FOCUS_HPP
