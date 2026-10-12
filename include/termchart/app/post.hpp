// post.hpp —— 跨线程数据投递
//
// 模型：生产线程 post()，消费线程（通常是主循环）take()/drain()。
// 一把互斥锁护一个队列——这不是 lock-free 队列：投递频率是"用户事件"级
// （键盘、网络回调、其他线程的刷新请求），正确性与可读性优先，吞吐不是目标。
//
// 两个原语各管一种形态：
//   * PostQueue  —— 闭包队列：每个消息自带"怎么处理"，适合事件投递；
//   * Mailbox<T> —— 类型化单值信箱：最新值语义（后写覆盖未消费旧值），
//     适合"最新状态"型数据（鼠标位置、当前尺寸）。模板部分只能住在头文件。
#ifndef TERMCHART_APP_POST_HPP
#define TERMCHART_APP_POST_HPP

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::app {

class PostQueue {
public:
    using Task = std::function<void()>;

    PostQueue() = default;
    PostQueue(const PostQueue&) = delete;
    PostQueue& operator=(const PostQueue&) = delete;

    // 线程安全。空 task 被忽略（无意义投递不占队列位）。
    TERMCHART_API void post(Task task);

    [[nodiscard]] std::size_t pending() const noexcept;

    // 消费接口（1）：把当前队列整体取走，由消费线程自行决定怎么执行。
    // （不加 noexcept：搬移元素时 vector 扩容可能 bad_alloc，不在库内吞掉。）
    [[nodiscard]] TERMCHART_API std::vector<Task> take();

    // 消费接口（2）：取走并**在本线程**依次执行，返回执行数。
    // Task 内部抛出的异常向上传播（消费线程的职责边界，不在这里吞）。
    TERMCHART_API std::size_t drain();

private:
    mutable std::mutex mutex_;
    std::deque<Task> queue_;
};

template <class T>
class Mailbox {
public:
    Mailbox() = default;
    Mailbox(const Mailbox&) = delete;
    Mailbox& operator=(const Mailbox&) = delete;

    // 线程安全。覆盖语义：未消费的旧值被丢弃。
    void post(T value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        slot_ = std::move(value);
    }

    // 取走当前值；无值 → false。
    [[nodiscard]] bool consume(T& out) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!slot_.has_value()) {
            return false;
        }
        out = std::move(*slot_);
        slot_.reset();
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const std::lock_guard<std::mutex> lock(mutex_);
        return !slot_.has_value();
    }

private:
    mutable std::mutex mutex_;
    std::optional<T> slot_;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_POST_HPP
