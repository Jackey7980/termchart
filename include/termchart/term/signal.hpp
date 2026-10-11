// signal.hpp —— SIGWINCH 转发给主线程
//
// 铁律：**信号处理器里只做两件事**——置 atomic 标志 + 唤醒 waker。
// 其余（重新布局、重绘、QString 那样的任何分配）一律带回主线程做。
// 违反这条的后果是" resize 时偶发死锁/堆损坏"，且不可复现。
//
// 平台差异：Windows 没有 SIGWINCH（靠 ReadConsoleInput 的 WINDOW_BUFFER_SIZE_EVENT），
// 因此本头文件导出的是"安装/卸载 resize 转发"的**抽象动作**而非具体信号。
#ifndef TERMCHART_TERM_SIGNAL_HPP
#define TERMCHART_TERM_SIGNAL_HPP

#include "termchart/export.hpp"
#include "termchart/term/waker.hpp"

namespace termchart::term {

// 安装转发：把 resize 事件转成 "标志 + waker.notify()"。
// 重复安装会替换旧 waker；返回是否成功。
TERMCHART_API bool install_resize_forwarding(Waker& waker) noexcept;

// 卸载并恢复原处理方式（可以在任意线程调用一次）。
TERMCHART_API void uninstall_resize_forwarding() noexcept;

// 主线程消费：读到 true 表示"发生过 resize，需要重新查询尺寸"。
// **读取即清零**，避免重复布局。
TERMCHART_API bool take_resize_flag() noexcept;

// 测试探针：模拟收到 resize（不真的发信号）。
TERMCHART_API void simulate_resize_for_test() noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_SIGNAL_HPP
