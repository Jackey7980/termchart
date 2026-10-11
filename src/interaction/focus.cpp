// focus.cpp —— 焦点链实现
#include "termchart/interaction/focus.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::interaction {

void FocusChain::set(std::uint32_t id) noexcept { current_ = id; }

std::uint32_t FocusChain::current() const noexcept { return current_; }

bool FocusChain::is_focused(std::uint32_t id) const noexcept {
    return id != 0 && current_ == id;
}

void FocusChain::clear() noexcept { current_ = 0; }

void FocusChain::next(const std::vector<std::uint32_t>& order) noexcept {
    if (order.empty()) {
        return;
    }
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == current_) {
            current_ = order[(i + 1) % order.size()];
            return;
        }
    }
    current_ = order.front();
}

void FocusChain::prev(const std::vector<std::uint32_t>& order) noexcept {
    if (order.empty()) {
        return;
    }
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == current_) {
            current_ = order[(i + order.size() - 1) % order.size()];
            return;
        }
    }
    current_ = order.back();
}

}  // namespace termchart::interaction
