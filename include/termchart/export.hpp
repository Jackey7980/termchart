// export.hpp —— 符号可见性宏
//
// 所有需要跨库边界（或被应用程序直接链接）的符号都标注 TERMCHART_API。
// 静态库构建时为空，避免污染。
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
