// app.hpp —— App：应用壳（构造时冻结注册表快照）
//
// 职责边界：App 是宿主与库之间的**编排骨架**，不画图、不碰终端、不依赖
// widget——GUI 栈会重写这层（docs/design/gui_stack 的分工），终端栈由
// ImmediateApp / HeadlessSession 补足。
//
// 快照语义（plugin/plugin.hpp 顶部注册时序协议的宿主侧落点）：
//   * 构造时对 PluginRegistry 做一次浅拷贝快照并**冻结**；
//   * 此后（包括 App 存活期内）的注册对**本实例**不可见；
//   * create_plugin 只查自己的快照，不回查全局注册表——宿主看到的插件
//     集合在构造那一刻就确定了，不会因为别的代码注册新插件而漂移。
#ifndef TERMCHART_APP_APP_HPP
#define TERMCHART_APP_APP_HPP

#include <memory>
#include <string_view>
#include <vector>

#include "termchart/app/config.hpp"
#include "termchart/export.hpp"
#include "termchart/plugin/plugin.hpp"

namespace termchart::app {

class App {
public:
    // 取 PluginRegistry 快照（冻结）。必须在注册完成之后构造。
    TERMCHART_API App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // 从快照创建插件实例（不调用 on_load，加载由宿主驱动）。
    // 未注册 → nullptr。
    [[nodiscard]] TERMCHART_API std::unique_ptr<plugin::Plugin>
    create_plugin(std::string_view name) const;

    [[nodiscard]] TERMCHART_API bool supports_plugin(std::string_view name) const noexcept;

    // 快照内的插件清单（按注册顺序；描述符是快照副本，可安全持有）。
    [[nodiscard]] TERMCHART_API std::vector<plugin::Manifest> plugins() const;

    // 快照整体（含工厂，可用于绕过名字直接创建）。
    [[nodiscard]] TERMCHART_API const plugin::PluginRegistry::Snapshot& plugin_descriptors()
        const noexcept;

    // 全局配置（App 不持有配置，只是入口：配置天然是进程级的）。
    [[nodiscard]] static Config& config() noexcept { return Config::global(); }

private:
    plugin::PluginRegistry::Snapshot plugins_;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_APP_HPP
