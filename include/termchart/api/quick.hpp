// quick.hpp —— 总入口
//
// 两件事：
//   1. init_core()：进程内一次性初始化（**std::call_once 守卫**，可重复调用）。
//      初始化内容是"全局副作用"的最后容身处：日志级别、默认能力快照。
//      注意：**注册表不在这里** —— 注册时机另有协议（见 docs/design/registry.md）。
//   2. 一行出图的便捷接口：给定文本与样式，走完整的
//      CellBuffer → Renderer → stdout 链路。
#ifndef TERMCHART_API_QUICK_HPP
#define TERMCHART_API_QUICK_HPP

#include <string>
#include <string_view>

#include "termchart/api/opts.hpp"
#include "termchart/api/print.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::api {

// 幂等。首次调用完成初始化；后续调用直接返回。
TERMCHART_API void init_core() noexcept;
[[nodiscard]] TERMCHART_API bool core_initialized() noexcept;

// 渲染成字节流：**不写 stdout**。headless / golden / 单元测试用这个。
TERMCHART_API Result<std::string> render_line(std::string_view text, const Style& style,
                                              const Opts& opts = {}) noexcept;

// 渲染 + 写出。返回写状态；渲染错误（如宽度非法）直接透传。
TERMCHART_API Status print_line(std::string_view text, const Style& style,
                                const Opts& opts = {}) noexcept;

// 零配置演示：一行调用即看到彩色输出（examples/32_quick_one_liner.cpp）。
TERMCHART_API Status hello(const Opts& opts = {}) noexcept;

}  // namespace termchart::api

#endif  // TERMCHART_API_QUICK_HPP
