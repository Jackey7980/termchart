// dispatcher.hpp —— 事件分发（订阅顺序即优先级，被消费即停）
#ifndef TERMCHART_INTERACTION_DISPATCHER_HPP
#define TERMCHART_INTERACTION_DISPATCHER_HPP

#include <cstddef>
#include <functional>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/interaction/event.hpp"

namespace termchart::interaction {

class Dispatcher {
public:
    // 返回 true 表示"已消费"，分发立即停止（冒泡终止）。
    using Handler = std::function<bool(const Event&)>;

    // 订阅某一类事件。
    void subscribe(EventKind kind, Handler handler);
    // 订阅全部事件（作为兜底/全局观察者）。
    void subscribe_all(Handler handler);

    // 按订阅顺序调用匹配的 handler；返回是否被消费。
    TERMCHART_API bool dispatch(const Event& event);

    [[nodiscard]] TERMCHART_API std::size_t handler_count() const noexcept;
    void clear();

private:
    struct Entry {
        bool all = false;
        EventKind kind = EventKind::Key;
        Handler handler;
    };
    std::vector<Entry> entries_;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_DISPATCHER_HPP
