// data_zoom.cpp —— 数据轴范围滑块实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/data_zoom.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::interaction {

DataZoom::DataZoom(double data_min, double data_max, double window_lo,
                   double window_hi, Rect track, ZoomAxis axis,
                   DataZoomOptions opts) noexcept
    : dmin_(data_min), dmax_(data_max), track_(track), axis_(axis), opts_(opts) {
    if (!(opts_.min_window > 0.0)) {
        opts_.min_window = 1e-9;
    }
    set_window(window_lo, window_hi);
}

void DataZoom::set_data_range(double lo, double hi) noexcept {
    dmin_ = lo;
    dmax_ = hi;
    clamp_window();
}

void DataZoom::set_track(Rect track) noexcept { track_ = track; }

void DataZoom::set_axis(ZoomAxis axis) noexcept { axis_ = axis; }

void DataZoom::set_options(DataZoomOptions opts) noexcept {
    if (!(opts.min_window > 0.0)) {
        opts.min_window = 1e-9;
    }
    if (!(opts.handle_frac > 0.0)) {
        opts.handle_frac = 0.04;
    }
    opts_ = opts;
    clamp_window();
}

void DataZoom::set_window(double lo, double hi) noexcept {
    wlo_ = lo;
    whi_ = hi;
    clamp_window();
}

void DataZoom::reset() noexcept {
    wlo_ = dmin_;
    whi_ = dmax_;
    clamp_window();
}

double DataZoom::value_at(double screen_pos) const noexcept {
    const double span = track_span();
    if (!(span > 0.0)) {
        return dmin_;
    }
    const double t = (screen_pos - track_start()) / span;
    return dmin_ + t * (dmax_ - dmin_);
}

double DataZoom::screen_pos_of(double value) const noexcept {
    const double dspan = dmax_ - dmin_;
    const double t = (dspan > 0.0) ? (value - dmin_) / dspan : 0.0;
    return track_start() + t * track_span();
}

DataZoom::Grab DataZoom::hit_test(double screen_pos, double screen_other) const noexcept {
    // 带宽判定：Horizontal 看垂直方向（screen_other 落在 track 的 y..y+h），
    // Vertical 看水平方向（screen_other 落在 track 的 x..x+w）。缺省 0.0 视为通过。
    if (axis_ == ZoomAxis::Horizontal) {
        if (screen_other != 0.0 &&
            (screen_other < track_.y || screen_other > track_.y + track_.h)) {
            return Grab::None;
        }
    } else {
        if (screen_other != 0.0 &&
            (screen_other < track_.x || screen_other > track_.x + track_.w)) {
            return Grab::None;
        }
    }

    const double span = track_span();
    const double handle_px = opts_.handle_frac * span;
    if (std::fabs(screen_pos - screen_pos_of(wlo_)) <= handle_px) {
        return Grab::Left;
    }
    if (std::fabs(screen_pos - screen_pos_of(whi_)) <= handle_px) {
        return Grab::Right;
    }
    const double v = value_at(screen_pos);
    if (v > wlo_ && v < whi_) {
        return Grab::Body;
    }
    return Grab::None;
}

void DataZoom::begin(double screen_pos) noexcept {
    grab_ = hit_test(screen_pos);
    dragging_ = (grab_ != Grab::None);
    if (grab_ == Grab::Body) {
        body_width_ = whi_ - wlo_;
        grab_offset_ = value_at(screen_pos) - wlo_;
    }
}

void DataZoom::move(double screen_pos) noexcept {
    if (!dragging_) {
        return;
    }
    const double v = value_at(screen_pos);
    const double span = dmax_ - dmin_;
    const double min_w = std::min(opts_.min_window, span > 0.0 ? span : 0.0);

    switch (grab_) {
        case Grab::Left:
            wlo_ = v;
            if (wlo_ > whi_ - min_w) {
                wlo_ = whi_ - min_w;
            }
            if (wlo_ < dmin_) {
                wlo_ = dmin_;
            }
            break;
        case Grab::Right:
            whi_ = v;
            if (whi_ < wlo_ + min_w) {
                whi_ = wlo_ + min_w;
            }
            if (whi_ > dmax_) {
                whi_ = dmax_;
            }
            break;
        case Grab::Body: {
            double width = body_width_;
            double new_lo = v - grab_offset_;
            // 窗口比数据界还宽：钉满数据界。
            if (width > span) {
                new_lo = dmin_;
                width = span;
            } else {
                if (new_lo < dmin_) {
                    new_lo = dmin_;
                }
                if (new_lo + width > dmax_) {
                    new_lo = dmax_ - width;
                }
            }
            wlo_ = new_lo;
            whi_ = new_lo + width;
            break;
        }
        case Grab::None:
            break;
    }
}

void DataZoom::end() noexcept {
    dragging_ = false;
    grab_ = Grab::None;
}

bool DataZoom::dragging() const noexcept { return dragging_; }

double DataZoom::window_lo_frac() const noexcept {
    const double dspan = dmax_ - dmin_;
    return dspan > 0.0 ? (wlo_ - dmin_) / dspan : 0.0;
}

double DataZoom::window_hi_frac() const noexcept {
    const double dspan = dmax_ - dmin_;
    return dspan > 0.0 ? (whi_ - dmin_) / dspan : 1.0;
}

double DataZoom::track_start() const noexcept {
    return axis_ == ZoomAxis::Horizontal ? track_.x : track_.y;
}

double DataZoom::track_span() const noexcept {
    return axis_ == ZoomAxis::Horizontal ? track_.w : track_.h;
}

void DataZoom::clamp_window() noexcept {
    const double span = dmax_ - dmin_;
    // 数据界退化：钉成一点，窗口 == 数据界。
    if (!(span > 0.0)) {
        wlo_ = dmin_;
        whi_ = dmax_;
        return;
    }
    double min_w = opts_.min_window;
    if (!(min_w > 0.0)) {
        min_w = 1e-9;
    }
    if (min_w > span) {
        min_w = span;
    }
    // 窗内跨度不足最小跨度：以窗口中心为锚对称撑开。
    if (whi_ - wlo_ < min_w) {
        const double c = 0.5 * (wlo_ + whi_);
        wlo_ = c - min_w / 2.0;
        whi_ = c + min_w / 2.0;
    }
    // 夹进数据界：先保证宽度，再整体平移（贴边优先）。
    const double width = whi_ - wlo_;
    if (wlo_ < dmin_) {
        wlo_ = dmin_;
        whi_ = wlo_ + width;
    }
    if (whi_ > dmax_) {
        whi_ = dmax_;
        wlo_ = whi_ - width;
    }
    // 最终安全网：顺序与越界兜底。
    if (wlo_ < dmin_) {
        wlo_ = dmin_;
    }
    if (whi_ > dmax_) {
        whi_ = dmax_;
    }
    if (wlo_ > whi_) {
        const double t = wlo_;
        wlo_ = whi_;
        whi_ = t;
    }
}

}  // namespace termchart::interaction
