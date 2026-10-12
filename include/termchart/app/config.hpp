// config.hpp —— 全局配置（键值对，线程安全）
//
// global() 是 function-local static（与其他注册表同一套契约）；独立实例
// 用于测试与多实例场景。
//
// 类型口径：值是 variant，**读什么类型得什么类型**——跨类型读取回退默认值，
// 不做隐式转换（字符串 "1" 不会当 bool 用）。配置错误应当显式暴露，
// 而不是被静默宽容掉。
#ifndef TERMCHART_APP_CONFIG_HPP
#define TERMCHART_APP_CONFIG_HPP

#include <cstdint>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>

#include "termchart/export.hpp"

namespace termchart::app {

using ConfigValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

class Config {
public:
    TERMCHART_API static Config& global() noexcept;

    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // 同 key 覆盖写。monostate 等价于"占位但不带值"（get 会回退默认值）。
    TERMCHART_API void set(std::string_view key, ConfigValue value);

    // 未设置 → std::monostate。
    [[nodiscard]] TERMCHART_API ConfigValue get(std::string_view key) const;

    [[nodiscard]] TERMCHART_API bool has(std::string_view key) const noexcept;
    // 返回是否真的删了（不存在 → false）。
    TERMCHART_API bool remove(std::string_view key);
    TERMCHART_API void clear() noexcept;

    // 类型化读取（带默认值）。
    [[nodiscard]] TERMCHART_API std::string get_string(std::string_view key,
                                                       std::string_view fallback = {}) const;
    [[nodiscard]] TERMCHART_API std::int64_t get_int(std::string_view key,
                                                     std::int64_t fallback = 0) const;
    [[nodiscard]] TERMCHART_API double get_double(std::string_view key, double fallback = 0.0) const;
    [[nodiscard]] TERMCHART_API bool get_bool(std::string_view key, bool fallback = false) const;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::map<std::string, ConfigValue, std::less<>> values_;
};

// 全局配置的便捷读写（等价 Config::global() 的对应方法）。
TERMCHART_API void set_config(std::string_view key, ConfigValue value);
[[nodiscard]] TERMCHART_API ConfigValue get_config(std::string_view key);

}  // namespace termchart::app

#endif  // TERMCHART_APP_CONFIG_HPP
