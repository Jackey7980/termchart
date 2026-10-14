// native_loop/timer.hpp —— 定时器队列
//
// 由事件循环在每个 tick 调用 fire_elapsed(now)：到期回调就地执行。interval<=0 视为
// 一次性（触发后移除）。GUI 平行栈的计时用"秒"为单位，与 util/timer 互不耦合。
#ifndef TERMCHART_GUI_NATIVE_LOOP_TIMER_HPP
#define TERMCHART_GUI_NATIVE_LOOP_TIMER_HPP

#include <cstdint>
#include <functional>
#include <vector>

namespace termchart_gui {

class TimerQueue {
public:
    // 注册定时器，返回句柄（用于 remove）。
    [[nodiscard]] std::uint64_t add(double interval_sec, std::function<void()> cb) {
        const std::uint64_t id = next_id_++;
        entries_.push_back(Entry{id, interval_sec,
                                 now_ + (interval_sec > 0.0 ? interval_sec : 0.0),
                                 std::move(cb)});
        return id;
    }

    void remove(std::uint64_t id) {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->id == id) {
                entries_.erase(it);
                return;
            }
        }
    }

    // 触发所有到期定时器（回调在此执行）。
    void fire_elapsed(double now_sec) {
        now_ = now_sec;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (now_ >= it->next) {
                auto cb = it->cb;
                if (it->interval > 0.0) {
                    it->next += it->interval;
                    ++it;
                } else {
                    it = entries_.erase(it);
                }
                if (cb) {
                    cb();
                }
            } else {
                ++it;
            }
        }
    }

private:
    struct Entry {
        std::uint64_t id;
        double interval;
        double next;
        std::function<void()> cb;
    };
    std::vector<Entry> entries_;
    std::uint64_t next_id_ = 1;
    double now_ = 0.0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_NATIVE_LOOP_TIMER_HPP
