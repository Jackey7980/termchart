// version.hpp —— 版本宏
//
// 与本轮的库版本同源：CMake 侧通过 target_compile_definitions 注入同值宏，
// 头文件侧的默认值用于"单头文件 / 直接拷 assume 头文件"场景。
#ifndef TERMCHART_VERSION_HPP
#define TERMCHART_VERSION_HPP

#define TERMCHART_VERSION_MAJOR 0
#define TERMCHART_VERSION_MINOR 9
#define TERMCHART_VERSION_PATCH 0

#define TERMCHART_VERSION_STRING "0.9.0"

// ABI 面 strip 到 major.minor：minor 之内不破坏（见 ABI_POLICY.md）。
#define TERMCHART_ABI_VERSION (TERMCHART_VERSION_MAJOR * 100 + TERMCHART_VERSION_MINOR)

#endif  // TERMCHART_VERSION_HPP
