#include "termchart/api/opts.hpp"

namespace termchart::api {

Opts& Opts::width(std::uint16_t value) noexcept {
    width_ = value;
    return *this;
}

Opts& Opts::height(std::uint16_t value) noexcept {
    height_ = value;
    return *this;
}

std::optional<std::uint16_t> Opts::width() const noexcept { return width_; }

std::optional<std::uint16_t> Opts::height() const noexcept { return height_; }

Opts& Opts::color_depth(ColorDepth value) noexcept {
    depth_ = value;
    return *this;
}

std::optional<ColorDepth> Opts::color_depth() const noexcept { return depth_; }

Opts& Opts::unicode(bool value) noexcept {
    unicode_ = value;
    return *this;
}

Opts& Opts::ascii_only() noexcept { return unicode(false); }

bool Opts::unicode() const noexcept { return unicode_; }

Opts& Opts::newline(bool value) noexcept {
    newline_ = value;
    return *this;
}

bool Opts::newline() const noexcept { return newline_; }

Opts& Opts::reset_at_end(bool value) noexcept {
    reset_ = value;
    return *this;
}

bool Opts::reset_at_end() const noexcept { return reset_; }

}  // namespace termchart::api
