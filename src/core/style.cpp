#include "termchart/core/style.hpp"

namespace termchart {

bool Color::operator==(const Color& other) const noexcept {
    if (kind_ != other.kind_) {
        return false;
    }
    switch (kind_) {
        case ColorKind::None:
            return true;
        case ColorKind::Ansi16:
        case ColorKind::Indexed:
            return index_ == other.index_;
        case ColorKind::Rgb:
            return rgb_ == other.rgb_;
    }
    return false;
}

bool Color::operator!=(const Color& other) const noexcept { return !(*this == other); }

bool Style::operator==(const Style& other) const noexcept {
    return attrs_ == other.attrs_ && fg_ == other.fg_ && bg_ == other.bg_;
}

bool Style::operator!=(const Style& other) const noexcept { return !(*this == other); }

}  // namespace termchart
