// reactive.hpp —— 响应式值：变更通知（订阅/发布）
//
// 线程约束（选型）：**单线程，不加锁**。理由：
//   1) widget 树与它的 Observable 归属同一 UI 线程，锁没有保护对象；
//   2) 加锁只会给"回调里再 set 同一个值"的重入路径以虚假的安全感——
//      通知顺序与重入语义应该由本文件写明的契约管住，而不是靠锁掩盖。
//   跨线程推送：调用方自行把新值排到 UI 线程再 set()。
//
// 契约：
//   * 值未变（operator== 判等）不通知；通知顺序 = 订阅顺序；
//   * 回调里再 set 同一 Observable 是**允许**的：重入请求被挂起，
//     等本轮通知走完后再补一次判定与通知（重入多次取最后一次的值）；
//   * 通知遍历**快照**：回调中订阅/退订（含自毁）只影响下一次通知，
//     不会悬空、不会跳过本轮已在快照里的其他监听者。
#ifndef TERMCHART_WIDGET_REACTIVE_HPP
#define TERMCHART_WIDGET_REACTIVE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::widget {

using ListenerId = std::uint64_t;

class ListenerBook;  // 与值类型无关的订阅簿记（类型擦除后），reactive.cpp 实现

// Subscription —— RAII 订阅句柄：析构/被覆盖/显式 release 即退订，幂等。
// 句柄以裸指针回指簿记，因此 Observable 不可拷贝/移动（见下）。
class Subscription {
public:
    Subscription() noexcept = default;
    TERMCHART_API ~Subscription();
    TERMCHART_API Subscription(Subscription&& other) noexcept;
    TERMCHART_API Subscription& operator=(Subscription&& other) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    [[nodiscard]] bool valid() const noexcept { return book_ != nullptr; }
    [[nodiscard]] ListenerId id() const noexcept { return id_; }

    // 显式退订；未订阅时调用是 no-op（幂等）。
    TERMCHART_API void release() noexcept;
    TERMCHART_API void swap(Subscription& other) noexcept;

private:
    friend class ListenerBook;
    TERMCHART_API Subscription(ListenerBook* book, ListenerId id) noexcept;

    ListenerBook* book_ = nullptr;
    ListenerId id_ = 0;
};

// 订阅簿记：只管 (id, 回调) 的增删与快照遍历，不认识值类型。
class ListenerBook {
public:
    // 类型擦除后的回调：拿到 new/old 两个值的只读指针。
    using ErasedFn = std::function<void(const void* new_value, const void* old_value)>;

    ListenerBook() = default;
    TERMCHART_API ~ListenerBook();
    ListenerBook(const ListenerBook&) = delete;
    ListenerBook& operator=(const ListenerBook&) = delete;

    [[nodiscard]] TERMCHART_API Subscription add(ErasedFn fn);
    TERMCHART_API bool remove(ListenerId id) noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;

    // 快照遍历：先拷贝当前订阅表再逐个调用（回调中的增删不影响本轮）。
    TERMCHART_API void notify(const void* new_value, const void* old_value) const;

private:
    friend class Subscription;
    std::vector<std::pair<ListenerId, ErasedFn>> listeners_;
    ListenerId next_id_ = 1;
};

// Observable<T> —— 可观察值。T 需要可拷贝构造 + operator==。
// 不可拷贝/不可移动：Subscription 以裸指针回指内部的 ListenerBook，
// 挪动 Observable 会让在场句柄悬空——这是契约，不是实现偷懒。
template <typename T>
class Observable {
public:
    using Listener = std::function<void(const T& new_value, const T& old_value)>;

    Observable() = default;
    explicit Observable(T initial) : value_(std::move(initial)) {}

    Observable(const Observable&) = delete;
    Observable& operator=(const Observable&) = delete;
    Observable(Observable&&) = delete;
    Observable& operator=(Observable&&) = delete;

    [[nodiscard]] Subscription subscribe(Listener listener) {
        return book_.add([listener = std::move(listener)](const void* nv, const void* ov) {
            listener(*static_cast<const T*>(nv), *static_cast<const T*>(ov));
        });
    }

    // 见文件头"契约"。重入 set 在通知期间只登记挂起值（最后写入者胜），
    // 本轮走完后按同一套"值未变不通知"规则补发。
    void set(T value) {
        if (notifying_) {
            pending_ = std::move(value);
            return;
        }
        if (value == value_) {
            return;
        }
        T old = value_;
        value_ = std::move(value);
        notifying_ = true;
        book_.notify(&value_, &old);
        notifying_ = false;
        if (pending_) {
            T next = std::move(*pending_);
            pending_.reset();
            set(std::move(next));
        }
    }

    [[nodiscard]] const T& get() const noexcept { return value_; }
    [[nodiscard]] std::size_t listener_count() const noexcept { return book_.size(); }

private:
    T value_{};
    ListenerBook book_;
    std::optional<T> pending_;  // 通知期间的重入 set（最后写入者胜）
    bool notifying_ = false;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_REACTIVE_HPP
