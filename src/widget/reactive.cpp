// reactive.cpp —— 订阅簿记与 RAII 句柄实现（与值类型无关的部分）
#include "termchart/widget/reactive.hpp"

#include <utility>

namespace termchart::widget {

Subscription::Subscription(ListenerBook* book, ListenerId id) noexcept
    : book_(book), id_(id) {}

ListenerBook::~ListenerBook() = default;

Subscription::~Subscription() { release(); }

Subscription::Subscription(Subscription&& other) noexcept
    : book_(std::exchange(other.book_, nullptr)), id_(std::exchange(other.id_, 0)) {}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        release();
        book_ = std::exchange(other.book_, nullptr);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

void Subscription::release() noexcept {
    if (book_ != nullptr) {
        (void)book_->remove(id_);
        book_ = nullptr;
        id_ = 0;
    }
}

void Subscription::swap(Subscription& other) noexcept {
    std::swap(book_, other.book_);
    std::swap(id_, other.id_);
}

Subscription ListenerBook::add(ErasedFn fn) {
    const ListenerId id = next_id_;
    ++next_id_;
    listeners_.emplace_back(id, std::move(fn));
    return Subscription(this, id);
}

bool ListenerBook::remove(ListenerId id) noexcept {
    // noexcept 的前提：erase 只做元素析构与 stealing move（本库目标实现
    // libstdc++/libc++/MSVC STL 的 std::function 移动均不抛）。
    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
        if (it->first == id) {
            listeners_.erase(it);
            return true;
        }
    }
    return false;
}

std::size_t ListenerBook::size() const noexcept { return listeners_.size(); }

void ListenerBook::notify(const void* new_value, const void* old_value) const {
    // 快照遍历（契约见 reactive.hpp 文件头）：回调中的订阅/退订只影响
    // 下一次通知。拷贝 std::function 有开销，但通知频率 = 值变更频率，
    // 正确性优先于这次拷贝。
    const auto snapshot = listeners_;
    for (const auto& entry : snapshot) {
        if (entry.second) {
            entry.second(new_value, old_value);
        }
    }
}

}  // namespace termchart::widget
