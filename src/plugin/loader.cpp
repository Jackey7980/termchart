#include "termchart/plugin/loader.hpp"

#include <utility>
#include <vector>

namespace termchart::plugin {
namespace {

// 引导队列：登记发生在静态初始化期或 main 之前的单线程阶段，
// 因此刻意不加锁（与 PluginRegistry 同一时序契约）。
std::vector<BootstrapFn>& boot_queue() noexcept {
    static std::vector<BootstrapFn> queue;
    return queue;
}

}  // namespace

ChartPlugin::ChartPlugin(std::string_view name, std::string_view version, ChartFactory factory)
    : manifest_(std::string(name), std::string(version), "termchart plugin", "chart plugin"),
      factory_(std::move(factory)) {}

Status ChartPlugin::on_load() noexcept {
    if (!factory_) {
        return ErrorCode::NotInitialized;
    }
    chart::ChartRegistry::instance().register_chart(manifest_.name, factory_);
    return ErrorCode::Ok;
}

std::unique_ptr<chart::Chart> ChartPlugin::create_chart() const {
    return factory_ ? factory_() : nullptr;
}

bool register_chart_plugin(std::string_view name, std::string_view version, ChartFactory factory) {
    if (name.empty() || !factory) {
        return false;
    }
    // 先进注册表（重名在这里被拒绝），成功后再挂静态引导，保证"两边都登记
    // 或两边都不登记"。
    ChartFactory persisted = factory;
    if (!PluginRegistry::instance().add(
            Manifest{std::string(name), std::string(version), "termchart plugin", "chart plugin"},
            [name, version, persisted]() -> std::unique_ptr<Plugin> {
                return std::make_unique<ChartPlugin>(name, version, persisted);
            })) {
        return false;
    }
    return add_static([name, version, persisted]() noexcept {
        ChartPlugin plugin(name, version, persisted);
        return plugin.on_load() == ErrorCode::Ok;
    });
}

bool add_static(BootstrapFn bootstrap) {
    if (!bootstrap) {
        return false;
    }
    boot_queue().push_back(std::move(bootstrap));
    return true;
}

std::size_t load_static_plugins() noexcept {
    std::size_t ok = 0;
    // 逐个弹出执行：引导函数执行期间新登记的条目留在队列里（下次再跑）。
    std::vector<BootstrapFn>& queue = boot_queue();
    while (!queue.empty()) {
        BootstrapFn fn = std::move(queue.back());
        queue.pop_back();
        if (fn()) {
            ++ok;
        }
    }
    return ok;
}

}  // namespace termchart::plugin
