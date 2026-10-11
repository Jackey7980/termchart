// task.hpp —— 可取消任务句柄
//
// 用途：把"这件事还要不要继续做"从执行体里解耦出来。执行体只持有只读的
// `CancellationToken`（随时问一句 cancelled()），发起方持有 `TaskHandle`
// （可 cancel、可在收尾时 finish）。两者共享一份原子状态，因此 cancel 可以在
// 别的线程发生。
//
// 边界说明：这里**不建线程、不排队、不调度**——只提供取消语义。要定时请用
// util/scheduler，要线程请用 app/（L20）。
#ifndef TERMCHART_UTIL_TASK_HPP
#define TERMCHART_UTIL_TASK_HPP

#include <memory>

#include "termchart/export.hpp"

namespace termchart::util {

// 内部共享状态。实现细节，外部只通过下面两个句柄访问。
class TaskState;

// 只读观察端。
class CancellationToken {
public:
    CancellationToken() = default;

    [[nodiscard]] TERMCHART_API bool cancelled() const noexcept;
    [[nodiscard]] TERMCHART_API bool finished() const noexcept;
    // 是否绑定到一个真实任务。默认构造的空 token 恒为 false，便于把
    // "没有任务" 与 "有任务且未取消" 区分开。
    [[nodiscard]] TERMCHART_API bool valid() const noexcept;

private:
    friend class TaskHandle;
    explicit CancellationToken(std::shared_ptr<const TaskState> state);

    std::shared_ptr<const TaskState> state_;
};

// 控制端。
class TaskHandle {
public:
    // 构造即创建一份独立状态：不同句柄互不影响。
    TERMCHART_API TaskHandle();

    [[nodiscard]] TERMCHART_API CancellationToken token() const noexcept;
    TERMCHART_API void cancel() noexcept;
    [[nodiscard]] TERMCHART_API bool cancelled() const noexcept;
    // 执行体收尾时调用，便于发起方判断"是取消还是跑完了"。
    TERMCHART_API void finish() noexcept;
    [[nodiscard]] TERMCHART_API bool finished() const noexcept;

private:
    std::shared_ptr<TaskState> state_;
};

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_TASK_HPP
