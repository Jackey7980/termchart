// scheduler.cpp —— 定时任务调度实现
#include "termchart/util/scheduler.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace termchart::util {

std::vector<Scheduler::Task>::iterator Scheduler::find(TaskId id) noexcept {
    return std::find_if(tasks_.begin(), tasks_.end(),
                        [id](const Task& task) { return task.id == id; });
}

Scheduler::TaskId Scheduler::after(Duration delay, Callback callback) {
    return after_at(now(), delay, std::move(callback));
}

Scheduler::TaskId Scheduler::every(Duration interval, Callback callback) {
    return every_at(now(), interval, std::move(callback));
}

Scheduler::TaskId Scheduler::after_at(TimePoint stamp, Duration delay, Callback callback) {
    const TaskId id = next_id_++;
    Task task;
    task.id = id;
    task.due = stamp + delay;
    task.callback = std::move(callback);
    tasks_.push_back(std::move(task));
    return id;
}

Scheduler::TaskId Scheduler::every_at(TimePoint stamp, Duration interval,
                                      Callback callback) {
    const TaskId id = next_id_++;
    Task task;
    task.id = id;
    task.periodic = true;
    task.interval = interval;
    task.due = stamp + interval;
    task.callback = std::move(callback);
    tasks_.push_back(std::move(task));
    return id;
}

bool Scheduler::cancel(TaskId id) {
    const auto it = find(id);
    if (it == tasks_.end()) {
        return false;
    }
    tasks_.erase(it);
    return true;
}

bool Scheduler::contains(TaskId id) const noexcept {
    return std::any_of(tasks_.begin(), tasks_.end(),
                       [id](const Task& task) { return task.id == id; });
}

std::size_t Scheduler::pending() const noexcept {
    return tasks_.size();
}

std::optional<TimePoint> Scheduler::next_due() const {
    if (tasks_.empty()) {
        return std::nullopt;
    }
    TimePoint earliest = tasks_.front().due;
    for (const Task& task : tasks_) {
        earliest = std::min(earliest, task.due);
    }
    return earliest;
}

std::size_t Scheduler::tick(TimePoint stamp) {
    // 先快照到期的 id：回调里可能增删任务，不能边遍历容器边调用。
    std::vector<TaskId> due;
    due.reserve(tasks_.size());
    for (const Task& task : tasks_) {
        if (task.due <= stamp) {
            due.push_back(task.id);
        }
    }

    std::size_t ran = 0;
    for (const TaskId id : due) {
        const auto it = find(id);
        if (it == tasks_.end()) {
            continue;  // 已被本轮更早的回调取消
        }
        // 拷贝一份再调用：回调修改容器后原迭代器不能再用。
        const Task task = *it;
        if (task.periodic) {
            it->due = stamp + task.interval;  // 固定间隔，不补偿追赶
        } else {
            tasks_.erase(it);
        }
        if (task.callback) {
            task.callback();
        }
        ++ran;
    }
    return ran;
}

}  // namespace termchart::util
