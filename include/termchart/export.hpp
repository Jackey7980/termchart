// export.hpp —— 符号可见性宏
//
// 所有需要跨库边界（或被应用程序直接链接）的符号都标注 TERMCHART_API。
// 静态库构建时为空，避免污染。
//
// 使用纪律（2026-11-13 由 `TERMCHART_BUILD_SHARED=ON` 实测确立）：
//   * 只标【定义在 src/*.cpp】的声明。头文件里的内联定义（含 `inline` 自由函数、
//     类内 `{ return ...; }` 访问器）**绝不标** —— MinGW 下 `dllimport` + 函数体
//     会直接报 "definition is marked dllimport"；且 inline 函数在每个 TU 各生成
//     一份，导出本就无意义。
//   * 静态构建下本宏为空，标错不会暴露，必须用 shared 构建做回归（CI 已有该 job）。
#ifndef TERMCHART_EXPORT_HPP
#define TERMCHART_EXPORT_HPP

#if defined(TERMCHART_STATIC)
#define TERMCHART_API
#define TERMCHART_NO_EXPORT
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(TERMCHART_BUILD_SHARED)
#define TERMCHART_API __declspec(dllexport)
#else
#define TERMCHART_API __declspec(dllimport)
#endif
#define TERMCHART_NO_EXPORT
#else
#define TERMCHART_API __attribute__((visibility("default")))
#define TERMCHART_NO_EXPORT __attribute__((visibility("hidden")))
#endif

// 仅用于注释性标注：该 ABI 面承诺稳定（见 ABI_POLICY.md）。
#define TERMCHART_ABI_STABLE

#endif  // TERMCHART_EXPORT_HPP
