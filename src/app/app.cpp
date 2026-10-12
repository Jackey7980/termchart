#include "termchart/app/app.hpp"

#include <utility>

#include "termchart/plugin/plugin.hpp"

namespace termchart::app {

App::App() : plugins_(plugin::PluginRegistry::instance().snapshot()) {}

std::unique_ptr<plugin::Plugin> App::create_plugin(std::string_view name) const {
    for (const plugin::PluginDescriptor& descriptor : plugins_) {
        if (plugin::name_equals(descriptor.manifest.name, name) && descriptor.factory) {
            return descriptor.factory();
        }
    }
    return nullptr;
}

bool App::supports_plugin(std::string_view name) const noexcept {
    for (const plugin::PluginDescriptor& descriptor : plugins_) {
        if (plugin::name_equals(descriptor.manifest.name, name)) {
            return true;
        }
    }
    return false;
}

std::vector<plugin::Manifest> App::plugins() const {
    std::vector<plugin::Manifest> out;
    out.reserve(plugins_.size());
    for (const plugin::PluginDescriptor& descriptor : plugins_) {
        out.push_back(descriptor.manifest);
    }
    return out;
}

const plugin::PluginRegistry::Snapshot& App::plugin_descriptors() const noexcept {
    return plugins_;
}

}  // namespace termchart::app
