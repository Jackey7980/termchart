// loader.hpp —— 静态插件注册与加载
//
// 两种静态供给方式：
//   1. 直接向 PluginRegistry::instance().add(...) 登记描述符；
//   2. 用 add_static(bootstrap) 登记"引导函数"（往下游注册表做一次性注入、
//      一次性初始化），由 load_static_plugins() 统一执行。
//
// 引导函数只执行一次（执行后即移出队列，失败的也不重试）：重复调用
// load_static_plugins() 只会执行新登记的引导。
//
// 图表插件：register_chart_plugin() 把"一个 Chart 工厂"包装成插件——
// 加载时把工厂注入 chart::ChartRegistry，同时插件本身可按名字 create()。
// chart::Chart 的子类即可注册（examples/31"注册自定义图表"走这条路）。
// plugin(L19) → chart(L12) 的依赖由分层规则允许：插件的本质就是往
// L12–L18 的注册表里注入能力。
#ifndef TERMCHART_PLUGIN_LOADER_HPP
#define TERMCHART_PLUGIN_LOADER_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>

#include "termchart/chart/registry.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/plugin/plugin.hpp"

namespace termchart::plugin {

// 图表工厂：与 chart::ChartRegistry::Factory 同型。
using ChartFactory = std::function<std::unique_ptr<chart::Chart>()>;

// 图表插件：on_load() 把工厂注入 chart::ChartRegistry。重复加载安全
// （register_chart 对同名是覆盖语义，见 chart/registry.hpp 的契约）。
class ChartPlugin final : public Plugin {
public:
    ChartPlugin(std::string_view name, std::string_view version, ChartFactory factory);

    [[nodiscard]] const Manifest& manifest() const noexcept override { return manifest_; }

    Status on_load() noexcept override;

    // 经插件创建图表实例。工厂本身无状态，未 on_load 也可用。
    [[nodiscard]] std::unique_ptr<chart::Chart> create_chart() const;

private:
    Manifest manifest_;
    ChartFactory factory_;
};

// 登记一个图表插件：同时进入 PluginRegistry（可 create）与静态加载队列
// （load_static_plugins() 时注入 chart::ChartRegistry）。
// 空名 / 空工厂 / 重名 → false（此时两边都不登记）。
TERMCHART_API bool register_chart_plugin(std::string_view name, std::string_view version,
                                         ChartFactory factory);

// 静态引导：返回 false = 该条加载失败（宿主可据此打日志）。
using BootstrapFn = std::function<bool()>;

// 登记引导函数（App 构造之前、单线程）。空函数 → false。
TERMCHART_API bool add_static(BootstrapFn bootstrap);

// 执行所有未执行的引导，返回成功的条数。队列随即清空：失败的条目同样
// 移除——静态引导不重试，重试与否应由宿主重新登记。
TERMCHART_API std::size_t load_static_plugins() noexcept;

}  // namespace termchart::plugin

#endif  // TERMCHART_PLUGIN_LOADER_HPP
