// animation.cpp —— AnimationState 的实现（见 chart/animation.hpp）
#include "termchart/chart/animation.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/util/math.hpp"

namespace termchart::chart {

void AnimationState::tick(double dt) noexcept {
    // dt == 0 **是**有效调用：帧循环第一帧常常 dt=0，它唯一的作用就是
    // 把状态从"未启动"切到"已启动"（进度 0），不能被当成"没调用"。
    if (!util::is_finite(dt) || dt < 0.0) {
        return;  // 脏 dt / 时间倒流：不推进，也不启动
    }
    started_ = true;
    elapsed_ += dt;
}

void AnimationState::reset() noexcept {
    elapsed_ = 0.0;
    started_ = false;
}

double AnimationState::progress() const noexcept {
    if (!started_) {
        return 1.0;  // 静态路径：从没 tick 过 = 全显示
    }
    const double active = elapsed_ - track.delay;
    if (!(track.duration > 0.0)) {
        // 零时长：延迟内仍为 0，延迟一过即完成。**顺序不能反**——先判
        // `active <= 0` 会让零时长动画在首帧永远停在 0（曾写错过）。
        return active < 0.0 ? 0.0 : 1.0;
    }
    if (active <= 0.0) {
        return 0.0;
    }
    return util::ease(track.easing, std::min(1.0, active / track.duration));
}

bool AnimationState::finished() const noexcept {
    if (!started_) {
        return false;  // 未启动不算"跑完"，但 Chart::animating() 另有口径
    }
    return elapsed_ >= track.delay + std::max(track.duration, 0.0);
}

}  // namespace termchart::chart
