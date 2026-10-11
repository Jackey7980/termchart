// assert.hpp —— 断言宏（header-only）
//
// 错误处理的第二档（另一档是 core/error.hpp 的 Result）：
//   * TERMCHART_ASSERT       —— Release 下也生效（契约破坏必须早失败）；
//   * TERMCHART_DEBUG_ASSERT —— 仅 NDEBUG 未定义时生效（昂贵的不变量检查）；
//   * 渲染热路径**不要**用它做常规校验：那里用钳位或 Result。
//
// 失败时直接 abort 而非抛异常：不动 ODR/ABI，也不让调用方以为"能 catch 回来"。
#ifndef TERMCHART_UTIL_ASSERT_HPP
#define TERMCHART_UTIL_ASSERT_HPP

#include <cstdio>
#include <cstdlib>

namespace termchart::util {

// 放在头文件的原因：希望每个断言点都能被内联与常量折叠。
// noexcept(false) 未加：断言失败就是要带走进程。
inline void assertion_failed(const char* expr, const char* file, int line,
                             const char* message) noexcept {
    if (message != nullptr) {
        std::fprintf(stderr, "termchart: assertion failed: %s (%s), file %s, line %d\n", expr,
                     message, file, line);
    } else {
        std::fprintf(stderr, "termchart: assertion failed: %s, file %s, line %d\n", expr, file,
                     line);
    }
    std::fflush(stderr);
    std::abort();
}

}  // namespace termchart::util

#define TERMCHART_ASSERT(expr)                                                    \
    do {                                                                          \
        if (!(expr)) {                                                            \
            ::termchart::util::assertion_failed(#expr, __FILE__, __LINE__, nullptr); \
        }                                                                         \
    } while (false)

#define TERMCHART_ASSERT_MSG(expr, msg)                                            \
    do {                                                                           \
        if (!(expr)) {                                                             \
            ::termchart::util::assertion_failed(#expr, __FILE__, __LINE__, (msg)); \
        }                                                                          \
    } while (false)

#ifdef NDEBUG
#define TERMCHART_DEBUG_ASSERT(expr) \
    do {                             \
    } while (false)
#else
#define TERMCHART_DEBUG_ASSERT(expr) TERMCHART_ASSERT(expr)
#endif

// 逻辑上不可达，但编译器不知道（例如 switch 全覆盖）。
#define TERMCHART_UNREACHABLE()                                                     \
    do {                                                                            \
        ::termchart::util::assertion_failed("unreachable", __FILE__, __LINE__, nullptr); \
    } while (false)

#endif  // TERMCHART_UTIL_ASSERT_HPP
