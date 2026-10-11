#include "termchart/term/raw_mode.hpp"

#include "src/term/platform_internal.hpp"

namespace termchart::term {

void RawMode::enter() noexcept {
    if (active_) {
        return;
    }
    if (pretend_) {
        active_ = true;
        return;
    }
    active_ = platform::enable_raw_mode();
}

void RawMode::leave() noexcept {
    if (!active_) {
        return;
    }
    if (!pretend_) {
        platform::disable_raw_mode();
    }
    active_ = false;
}

RawMode::~RawMode() { leave(); }

RawMode RawMode::make_noop(bool pretend_active) noexcept { return RawMode(pretend_active); }

}  // namespace termchart::term
