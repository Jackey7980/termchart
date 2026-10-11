// deprecate.hpp —— 弃用标记宏
//
// 弃用流程（docs/contrib/deprecation.md）：
//   第一步加 TERMCHART_DEPRECATED("将于 vX.Y 移除，改用 xxx")，至少一个 minor 周期保留；
//   第二步在下个 major 删除。测试用 -Werror=deprecated-declarations 时，
//   内部调用点必须显式加 TERMCHART_DEPRECATED_IGNORE_BEGIN/END。
#ifndef TERMCHART_DEPRECATE_HPP
#define TERMCHART_DEPRECATE_HPP

#if defined(__GNUC__) || defined(__clang__)
#define TERMCHART_DEPRECATED(msg) __attribute__((deprecated(msg)))
#define TERMCHART_DEPRECATED_IGNORE_BEGIN \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define TERMCHART_DEPRECATED_IGNORE_END _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define TERMCHART_DEPRECATED(msg) __declspec(deprecated(msg))
#define TERMCHART_DEPRECATED_IGNORE_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
#define TERMCHART_DEPRECATED_IGNORE_END __pragma(warning(pop))
#else
#define TERMCHART_DEPRECATED(msg)
#define TERMCHART_DEPRECATED_IGNORE_BEGIN
#define TERMCHART_DEPRECATED_IGNORE_END
#endif

// 枚举值级弃用：保留原值，新增替代值。
#define TERMCHART_DEPRECATED_ENUM(msg) TERMCHART_DEPRECATED(msg)

#endif  // TERMCHART_DEPRECATE_HPP
