#include "termchart/plugin/plugin.hpp"

#include <utility>

namespace termchart::plugin {
namespace {

char lower(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

}  // namespace

bool name_equals(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lower(lhs[i]) != lower(rhs[i])) {
            return false;
        }
    }
    return true;
}

PluginRegistry& PluginRegistry::instance() noexcept {
    static PluginRegistry registry;
    return registry;
}

bool PluginRegistry::add(const Manifest& manifest, PluginFactory factory) {
    if (!manifest_valid(manifest) || !factory) {
        return false;
    }
    for (const PluginDescriptor& entry : entries_) {
        if (name_equals(entry.manifest.name, manifest.name)) {
            return false;
        }
    }
    entries_.push_back(PluginDescriptor{manifest, std::move(factory)});
    return true;
}

const Manifest* PluginRegistry::find(std::string_view name) const noexcept {
    for (const PluginDescriptor& entry : entries_) {
        if (name_equals(entry.manifest.name, name)) {
            return &entry.manifest;
        }
    }
    return nullptr;
}

std::unique_ptr<Plugin> PluginRegistry::create(std::string_view name) const {
    for (const PluginDescriptor& entry : entries_) {
        if (name_equals(entry.manifest.name, name) && entry.factory) {
            return entry.factory();
        }
    }
    return nullptr;
}

bool PluginRegistry::supports(std::string_view name) const noexcept {
    for (const PluginDescriptor& entry : entries_) {
        if (name_equals(entry.manifest.name, name)) {
            return true;
        }
    }
    return false;
}

PluginRegistry::Snapshot PluginRegistry::snapshot() const { return entries_; }

std::vector<std::string> PluginRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const PluginDescriptor& entry : entries_) {
        out.push_back(entry.manifest.name);
    }
    return out;  // 注册序（add 拒绝重名，天然无重复）
}

void PluginRegistry::clear_for_test() noexcept { entries_.clear(); }

}  // namespace termchart::plugin
