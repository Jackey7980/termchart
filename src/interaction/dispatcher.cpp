// dispatcher.cpp —— 事件分发实现
#include "termchart/interaction/dispatcher.hpp"

#include <cstddef>

namespace termchart::interaction {

void Dispatcher::subscribe(EventKind kind, Handler handler) {
    entries_.push_back(Entry{false, kind, std::move(handler)});
}

void Dispatcher::subscribe_all(Handler handler) {
    entries_.push_back(Entry{true, EventKind::Key, std::move(handler)});
}

bool Dispatcher::dispatch(const Event& event) {
    const EventKind kind = kind_of(event);
    for (const Entry& entry : entries_) {
        if (entry.all || entry.kind == kind) {
            if (entry.handler && entry.handler(event)) {
                return true;  // 已消费
            }
        }
    }
    return false;
}

std::size_t Dispatcher::handler_count() const noexcept { return entries_.size(); }

void Dispatcher::clear() { entries_.clear(); }

}  // namespace termchart::interaction
