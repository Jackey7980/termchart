// native_loop/loop.hpp —— 事件循环抽象 + 无头实现
//
// GUI 平行栈的事件循环自己写（不借终端 app/loop）。EventLoop 是接口；Headless
// 实现用内部队列 + 定时器，可手动注入事件与 pump，方便测试与 headless 渲染。
// 接真实 tk 时由 backend 提供一个基于 OS 消息泵的 EventLoop 实现。
#ifndef TERMCHART_GUI_NATIVE_LOOP_LOOP_HPP
#define TERMCHART_GUI_NATIVE_LOOP_LOOP_HPP

#include <cstdint>
#include <deque>
#include <functional>

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/native_loop/timer.hpp"

namespace termchart_gui {

class EventLoop {
public:
    virtual ~EventLoop() = default;

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void post(Event e) = 0;
    virtual void request_paint() = 0;
};

class HeadlessEventLoop final : public EventLoop {
public:
    void run() override {
        running_ = true;
        while (running_) {
            pump_once();
        }
    }

    void stop() override { running_ = false; }

    void post(Event e) override { queue_.push_back(std::move(e)); }

    void request_paint() override {
        Event e{};
        e.type = EventType::Paint;
        post(e);
    }

    // 测试钩子：取出下一个事件（不阻塞）。
    [[nodiscard]] bool poll(Event& out) {
        if (queue_.empty()) {
            return false;
        }
        out = queue_.front();
        queue_.pop_front();
        return true;
    }

    // 处理一个事件：先触发到期定时器，再处理队首事件（Paint 调 paint_cb_）。
    void pump_once() {
        timers_.fire_elapsed(now_);
        now_ += 0.016;
        Event e;
        if (!poll(e)) {
            running_ = false;  // 队列空则退出 run()
            return;
        }
        if (e.type == EventType::Paint && paint_cb_) {
            paint_cb_();
        }
    }

    std::uint64_t add_timer(double interval_sec, std::function<void()> cb) {
        return timers_.add(interval_sec, std::move(cb));
    }

    void set_paint_callback(std::function<void()> cb) { paint_cb_ = std::move(cb); }

private:
    std::deque<Event> queue_;
    TimerQueue timers_;
    std::function<void()> paint_cb_{};
    bool running_ = false;
    double now_ = 0.0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_NATIVE_LOOP_LOOP_HPP
