#include "termchart/api/opts.hpp"

namespace termchart::api {

ApiOptions& ApiOptions::width(std::uint16_t value) noexcept {
    width_ = value;
    return *this;
}

ApiOptions& ApiOptions::height(std::uint16_t value) noexcept {
    height_ = value;
    return *this;
}

std::optional<std::uint16_t> ApiOptions::width() const noexcept { return width_; }

std::optional<std::uint16_t> ApiOptions::height() const noexcept { return height_; }

ApiOptions& ApiOptions::color_depth(ColorDepth value) noexcept {
    depth_ = value;
    return *this;
}

std::optional<ColorDepth> ApiOptions::color_depth() const noexcept { return depth_; }

ApiOptions& ApiOptions::unicode(bool value) noexcept {
    unicode_ = value;
    return *this;
}

ApiOptions& ApiOptions::ascii_only() noexcept { return unicode(false); }

bool ApiOptions::unicode() const noexcept { return unicode_; }

ApiOptions& ApiOptions::newline(bool value) noexcept {
    newline_ = value;
    return *this;
}

bool ApiOptions::newline() const noexcept { return newline_; }

ApiOptions& ApiOptions::reset_at_end(bool value) noexcept {
    reset_ = value;
    return *this;
}

bool ApiOptions::reset_at_end() const noexcept { return reset_; }

}  // namespace termchart::api
