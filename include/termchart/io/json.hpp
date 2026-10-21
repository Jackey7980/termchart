// io/json.hpp —— JSON 解析（L2）
//
// 本文件于 2026-10-21 从 dsl/json.hpp 下沉收编（"数据入口归 io"）：解析核心与
// 方言无关，dsl 及其余上层模块直接复用；dsl/json.hpp 现为 using 别名 shim，
// 既有 dsl::JsonValue / dsl::parse_json 调用点零改动。
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 完整 JSON 值：null / true / false / 数字 / 字符串 / 数组 / 对象，任意嵌套；
//   ✅ 数字：负号、小数、科学计数法（1e5、-2.5E-3）；按 double 存储（大整数
//      会丢精度，如实记录而非假装精确）；
//   ✅ 字符串转义：\" \\ \/ \b \f \n \r \t \uXXXX（含代理对 → UTF-8）；
//   ✅ 对象保持**插入序**（vector<pair> 而非 map），重复键后者覆盖前者；
//   ✅ 嵌套深度上限（默认 64）与输入长度上限（默认 16MB），超限返回错误而非
//      栈爆/内存爆；
//   ✅ 诊断：首个错误带行列与字节偏移（与 io/csv 同口径）。
//   ⛔ 不支持：注释、尾逗号、单引号字符串、NaN/Infinity、数字前导 + 号、
//      控制字符未转义直出（按 JSON 规范拒绝）。
//
// 错误：解析失败返回 ErrorCode::ParseError / OutOfRange / InvalidArgument，
// 可选地把**首个**诊断写到 out_diagnostic。
#ifndef TERMCHART_IO_JSON_HPP
#define TERMCHART_IO_JSON_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/io/diagnostic.hpp"

namespace termchart::io {

// JSON 值：带 tag 的结构体（递归容器用 vector 成员，C++17 起对不完整类型合法）。
// 不用 variant：array/object 递归嵌套时 variant 需要额外一层间接，可读性更差。
class JsonValue {
public:
    enum class Type : std::uint8_t {
        Null = 0,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    JsonValue() noexcept = default;  // Null

    // 工厂
    static JsonValue null() noexcept;
    static JsonValue boolean(bool value) noexcept;
    static JsonValue number(double value) noexcept;
    static JsonValue string(std::string value);
    static JsonValue array();
    static JsonValue object();

    // ── 类型判定 ──────────────────────────────────────────────────────────
    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool is_null() const noexcept { return type_ == Type::Null; }
    [[nodiscard]] bool is_bool() const noexcept { return type_ == Type::Bool; }
    [[nodiscard]] bool is_number() const noexcept { return type_ == Type::Number; }
    [[nodiscard]] bool is_string() const noexcept { return type_ == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type_ == Type::Object; }

    // ── 取值（类型不匹配返回 fallback，不抛异常）────────────────────────────
    [[nodiscard]] bool as_bool(bool fallback = false) const noexcept;
    [[nodiscard]] double as_number(double fallback = 0.0) const noexcept;
    [[nodiscard]] std::string_view as_string(std::string_view fallback = {}) const noexcept;

    // ── 数组访问 ──────────────────────────────────────────────────────────
    // size()：array/object 的元素数，其它类型为 0。
    [[nodiscard]] std::size_t size() const noexcept;
    // 越界返回 Null 单例（与"缺失即 null"的 JSON 语义一致，调用方无需判空）。
    [[nodiscard]] const JsonValue& at(std::size_t index) const noexcept;
    [[nodiscard]] const JsonValue& operator[](std::size_t index) const noexcept;

    // ── 对象访问 ──────────────────────────────────────────────────────────
    // 找不到返回 nullptr（与 at 的 Null 单例互补：需要区分"缺失"与"显式 null"时用）。
    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;
    // 找不到返回 Null 单例。
    [[nodiscard]] const JsonValue& at(std::string_view key) const noexcept;

    // ── 构建（程序化构造；解析产物不需要）──────────────────────────────────
    void push(JsonValue value);                    // 非 array 时忽略
    void set(std::string key, JsonValue value);    // 非 object 时忽略；同名键覆盖

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;                              // Type::Array
    std::vector<std::pair<std::string, JsonValue>> object_;     // Type::Object（保序）
};

struct JsonOptions {
    std::size_t max_depth = 64;        // 嵌套深度上限（数组/对象各算一层）
    std::size_t max_bytes = 16u * 1024u * 1024u;  // 输入长度上限
};

// 解析 JSON 文本为 JsonValue。顶层必须是完整值（对象/数组/标量均可），
// 尾部只允许空白。失败返回错误码，可选地把首个诊断写到 out_diagnostic。
[[nodiscard]] TERMCHART_API Result<JsonValue> parse_json(
    std::string_view source, const JsonOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::io

#endif  // TERMCHART_IO_JSON_HPP
