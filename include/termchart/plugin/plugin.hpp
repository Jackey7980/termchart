// plugin.hpp —— 插件基类与插件注册表（L19）
//
// 注册时序协议（与 chart / raster / scale / dsl 注册表同构，docs/design/registry.md
// 的统一模式；api/quick.hpp 顶部"注册时机另有协议"指的就是这一条）：
//   * PluginRegistry::instance() 是 **function-local static**，规避静态初始化顺序问题；
//   * 注册的唯一安全时机是 **App 构造之前**（单线程）：App 构造时对注册表做
//     浅拷贝快照并冻结，之后注册的插件对已构造的 App 不可见；
//   * 注册的是**工厂而非实例**：create() 每次返回新实例，生命周期归调用方，
//     没有原型 clone、没有共享所有权；
//   * 本注册表不跨线程：注册与创建都假定发生在宿主线程（跨线程数据走
//     app/post.hpp，不经过注册表）。
#ifndef TERMCHART_PLUGIN_PLUGIN_HPP
#define TERMCHART_PLUGIN_PLUGIN_HPP

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/plugin/manifest.hpp"

namespace termchart::plugin {

// 插件基类：宿主经由 manifest() 认识插件，经由 on_load/on_unload 驱动生命周期。
class Plugin {
public:
    Plugin() = default;
    virtual ~Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    [[nodiscard]] virtual const Manifest& manifest() const noexcept = 0;

    // 加载钩子：返回非 Ok 表示拒绝启用，宿主据此决定丢弃实例。
    // 默认实现是"什么都不做的加载"，多数插件只需要其中一端。
    virtual Status on_load() noexcept { return ErrorCode::Ok; }
    virtual void on_unload() noexcept {}
};

// 工厂签名：create() 每次调用都应产出新实例。
using PluginFactory = std::function<std::unique_ptr<Plugin>()>;

// 注册表条目：描述符 + 工厂。快照 = 一次 vector 拷贝（工厂可拷贝）。
struct PluginDescriptor {
    Manifest manifest;
    PluginFactory factory;
};

// 不区分大小写的名字比较（注册键口径，全模块统一用它）。
[[nodiscard]] TERMCHART_API bool name_equals(std::string_view lhs, std::string_view rhs) noexcept;

class PluginRegistry {
public:
    using Snapshot = std::vector<PluginDescriptor>;

    TERMCHART_API static PluginRegistry& instance() noexcept;

    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    // 注册：重名（不区分大小写）、空名或空工厂 → 拒绝并返回 false。
    TERMCHART_API bool add(const Manifest& manifest, PluginFactory factory);

    // 未注册 → nullptr。返回的指针指向注册表内部，注册表存活期内有效，
    // 但**不得**跨"可能触发注册"的调用持有。
    [[nodiscard]] TERMCHART_API const Manifest* find(std::string_view name) const noexcept;

    // 未注册或工厂为空 → nullptr；否则产出新实例（不调用 on_load，加载由宿主驱动）。
    [[nodiscard]] TERMCHART_API std::unique_ptr<Plugin> create(std::string_view name) const;

    [[nodiscard]] bool supports(std::string_view name) const noexcept;

    // 快照语义：App 构造时调用一次，此后只读快照（浅拷贝：工厂可拷贝）。
    [[nodiscard]] TERMCHART_API Snapshot snapshot() const;

    // 枚举三件套（全仓注册表统一口径）：names() 按注册序（与 snapshot 一致）。
    [[nodiscard]] TERMCHART_API std::vector<std::string> names() const;

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    // 仅供测试：清空注册表。
    TERMCHART_API void clear_for_test() noexcept;

private:
    PluginRegistry() = default;
    std::vector<PluginDescriptor> entries_;
};

}  // namespace termchart::plugin

#endif  // TERMCHART_PLUGIN_PLUGIN_HPP
