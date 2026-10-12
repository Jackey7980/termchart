#include "termchart/app/post.hpp"

#include <utility>

namespace termchart::app {

void PostQueue::post(Task task) {
    if (!task) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(task));
}

std::size_t PostQueue::pending() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::vector<PostQueue::Task> PostQueue::take() {
    std::vector<Task> out;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        out.reserve(queue_.size());
        while (!queue_.empty()) {
            out.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }
    }
    return out;
}

std::size_t PostQueue::drain() {
    std::vector<Task> tasks = take();
    std::size_t executed = 0;
    for (Task& task : tasks) {
        if (task) {  // take 不会放进空 task，这里是双保险。
            task();
            ++executed;
        }
    }
    return executed;
}

}  // namespace termchart::app
