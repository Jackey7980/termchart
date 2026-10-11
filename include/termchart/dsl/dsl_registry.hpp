// dsl_registry.hpp —— 方言注册表（L17）
//
// 与 chart / raster / scale 注册表同构（docs/design/registry.md，五注册表统一模式）：
//   * `instance()` 用 **function-local static**，规避静态初始化顺序问题；
//   * 注册表不持有对象所有权，只持有**函数指针 + 字符串视图**（方言入口是纯函数）；
//   * 注册的唯一安全时机是 `App` 构造之前；`App` 构造时做浅拷贝快照并冻结；
//   * 内置方言由 `register_builtin_dsls()` 在静态初始化期登记（幂等，std::call_once 守卫）。
//
// 与另四个注册表的差别：方言入口签名是"纯函数"，因此描述符是 trivially copyable 的，
// 快照即一次 vector 拷贝——没有工厂对象生命周期问题。
#ifndef TERMCHART_DSL_DSL_REGISTRY_HPP
#define TERMCHART_DSL_DSL_REGISTRY_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct DslOptions {
    std::size_t max_tokens = 200000;  // 词法上限（fuzz 输入可能极长）
    std::size_t max_nodes = 10000;    // 图规模上限：防止一句话吃光内存
};

// 解析入口指针：纯函数，无捕获、无状态。
using DslParseFn = Result<AstGraph> (*)(std::string_view source, const DslOptions& options);

struct DslDescriptor {
    std::string_view name;         // 注册名（"mermaid"、"flowchart"…），小写
    std::string_view description;  // 一行说明（给人看，也进 --help）
    std::string_view directive;    // 典型首行指令（"flowchart"、"digraph"…），识别用
    DslParseFn parse = nullptr;    // 非空是注册的有效性条件
};

class DslRegistry {
public:
    TERMCHART_API static DslRegistry& instance() noexcept;

    // 重名（不区分大小写）或 parse 为空 → 拒绝并返回 false。
    TERMCHART_API bool add(const DslDescriptor& descriptor) noexcept;

    // 未注册返回 nullptr。
    [[nodiscard]] TERMCHART_API const DslDescriptor* find(std::string_view name) const noexcept;

    // 未注册 → ErrorCode::Unsupported；已注册但解析失败 → 解析器给出的错误码。
    [[nodiscard]] TERMCHART_API Result<AstGraph> parse(std::string_view name,
                                                      std::string_view source,
                                                      const DslOptions& options = {}) const;

    // 注册名列表（按注册顺序，稳定）。
    [[nodiscard]] TERMCHART_API std::vector<std::string_view> names() const;

    // 快照语义：App 构造时调用一次，此后只读快照。
    [[nodiscard]] TERMCHART_API std::vector<DslDescriptor> snapshot() const;

    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;

    // 仅供测试：清空注册表（清空后 register_builtin_dsls 会重新登记）。
    TERMCHART_API void clear_for_test() noexcept;

private:
    DslRegistry() = default;
    std::vector<DslDescriptor> entries_;
};

// 幂等登记内置方言（mermaid 等）。首次调用发生在静态初始化期或首次解析前。
TERMCHART_API bool register_builtin_dsls() noexcept;

// 内置描述符表（定义在 dsl_registry.cpp；方言实现分散在自己的 TU 里）。
[[nodiscard]] TERMCHART_API std::vector<DslDescriptor> builtin_descriptors();

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_DSL_REGISTRY_HPP
