#include "termchart/core/cell.hpp"

#include "termchart/core/style.hpp"

namespace termchart {

bool operator==(const Cell& lhs, const Cell& rhs) noexcept {
    return lhs.cp == rhs.cp && lhs.style == rhs.style;
}

bool operator!=(const Cell& lhs, const Cell& rhs) noexcept { return !(lhs == rhs); }

bool is_blank(const Cell& cell) noexcept {
    return cell.cp == U' ' && cell.style.attrs() == kNoAttrs && cell.style.fg().is_none() &&
           cell.style.bg().is_none();
}

}  // namespace termchart
