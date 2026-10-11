// expected.hpp —— Expected<T, E> 与 Result<T> 别名
//
// 为什么不直接用 std::expected：它是 C++23 库设施，本库要求 C++20；
// 而且我们只需要"值或错误码"这个小子集。语义对齐 std::expected：
//   * has_value() / operator bool()；
//   * value() 在无值时**抛 std::logic_error**（调用方已是 bug）；
//   * error() 在有值时属 UB（与 std::expected 一致，此处以异常兜底）。
#ifndef TERMCHART_CORE_EXPECTED_HPP
#define TERMCHART_CORE_EXPECTED_HPP

#include <utility>
#include <variant>

#include "termchart/core/error.hpp"
#include "termchart/export.hpp"

namespace termchart {

// 无值访问时的异常来源（实现在 src/core/expected.cpp，避免头文件带 throw 模板膨胀）。
[[noreturn]] TERMCHART_API void throw_bad_expected_access(const char* what) noexcept(false);

template <class E>
class Unexpected {
public:
    explicit constexpr Unexpected(E error) : error_(std::move(error)) {}
    constexpr const E& error() const noexcept { return error_; }

private:
    E error_;
};

template <class E>
Unexpected(E) -> Unexpected<E>;

template <class T, class E>
class Expected {
public:
    using value_type = T;
    using error_type = E;

    constexpr Expected(T value) : store_(std::in_place_index<0>, std::move(value)) {}
    constexpr Expected(Unexpected<E> err) : store_(std::in_place_index<1>, std::move(err.error())) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return store_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr const T& value() const noexcept(false) {
        if (!has_value()) {
            throw_bad_expected_access("termchart::Expected::value() 无值");
        }
        return *std::get_if<0>(&store_);
    }

    [[nodiscard]] constexpr T& value() noexcept(false) {
        if (!has_value()) {
            throw_bad_expected_access("termchart::Expected::value() 无值");
        }
        return *std::get_if<0>(&store_);
    }

    [[nodiscard]] constexpr const E& error() const noexcept(false) {
        if (has_value()) {
            throw_bad_expected_access("termchart::Expected::error() 无错误");
        }
        return *std::get_if<1>(&store_);
    }

    // std::expected 兼容的解引用：无值时抛（与 value() 同）。
    // 非 const 重载必须存在：否则 `*result` 永远是 const 引用，调用方拿到
    // "改不了的结果"——写 `parse(x).value().add_...()` 这类代码时会卡住。
    [[nodiscard]] constexpr const T& operator*() const noexcept(false) { return value(); }
    [[nodiscard]] constexpr T& operator*() noexcept(false) { return value(); }
    [[nodiscard]] constexpr const T* operator->() const noexcept(false) { return &value(); }
    [[nodiscard]] constexpr T* operator->() noexcept(false) { return &value(); }

    [[nodiscard]] constexpr T value_or(T fallback) const noexcept {
        return has_value() ? *std::get_if<0>(&store_) : std::move(fallback);
    }

    // 链式：成功时变换值，失败时原样传递错误。
    template <class Fn>
    [[nodiscard]] constexpr auto and_then(Fn&& fn) const {
        using R = decltype(fn(std::declval<const T&>()));
        return has_value() ? fn(*std::get_if<0>(&store_)) : R(Unexpected<E>(*std::get_if<1>(&store_)));
    }

private:
    std::variant<T, E> store_;
};

// Result<T>：错误类型固定为 ErrorCode，覆盖绝大多数场景。
template <class T>
using Result = Expected<T, ErrorCode>;

// 无返回值：成功与否只用错误码表达。用 Result<std::monostate> 太饶，这里给专用alias。
using Status = ErrorCode;

}  // namespace termchart

#endif  // TERMCHART_CORE_EXPECTED_HPP
