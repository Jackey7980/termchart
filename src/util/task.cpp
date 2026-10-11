// task.cpp —— 可取消任务句柄实现
#include "termchart/util/task.hpp"

#include <atomic>
#include <memory>
#include <utility>

namespace termchart::util {

// 共享状态：两个原子标志，够了。不需要互斥量——没有需要一起更新的复合状态。
// 用 acquire/release 而不是 relaxed：取消标志常常用来"放行"别的线程即将读取的
// 数据，release/acquire 这对语义正是为此而设，且在本平台零额外开销。
class TaskState {
public:
    std::atomic<bool> cancelled{false};
    std::atomic<bool> finished{false};
};

CancellationToken::CancellationToken(std::shared_ptr<const TaskState> state)
    : state_(std::move(state)) {}

bool CancellationToken::cancelled() const noexcept {
    return state_ != nullptr && state_->cancelled.load(std::memory_order_acquire);
}

bool CancellationToken::finished() const noexcept {
    return state_ != nullptr && state_->finished.load(std::memory_order_acquire);
}

bool CancellationToken::valid() const noexcept {
    return state_ != nullptr;
}

TaskHandle::TaskHandle() : state_(std::make_shared<TaskState>()) {}

CancellationToken TaskHandle::token() const noexcept {
    return CancellationToken(state_);
}

void TaskHandle::cancel() noexcept {
    if (state_ != nullptr) {
        state_->cancelled.store(true, std::memory_order_release);
    }
}

bool TaskHandle::cancelled() const noexcept {
    return state_ != nullptr && state_->cancelled.load(std::memory_order_acquire);
}

void TaskHandle::finish() noexcept {
    if (state_ != nullptr) {
        state_->finished.store(true, std::memory_order_release);
    }
}

bool TaskHandle::finished() const noexcept {
    return state_ != nullptr && state_->finished.load(std::memory_order_acquire);
}

}  // namespace termchart::util
