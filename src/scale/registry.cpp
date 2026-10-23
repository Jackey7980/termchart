// registry.cpp —— 比例尺注册表实现（内置 linear/log + 自定义登记）
#include "termchart/scale/registry.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/scale/linear.hpp"
#include "termchart/scale/log.hpp"
#include "termchart/scale/time.hpp"
#include "termchart/scale/category.hpp"
#include "termchart/scale/band.hpp"

namespace termchart::scale {

namespace {

// 内置名字：与 register_scale 的登记项一起参与 supports()/names()。
constexpr std::string_view kBuiltinLinear = "linear";
constexpr std::string_view kBuiltinLog = "log";
constexpr std::string_view kBuiltinTime = "time";
constexpr std::string_view kBuiltinCategory = "category";
constexpr std::string_view kBuiltinBand = "band";

}  // namespace

ScaleRegistry& ScaleRegistry::instance() noexcept {
    static ScaleRegistry registry;
    return registry;
}

void ScaleRegistry::register_scale(std::string_view name, Factory factory) {
    factories_[std::string(name)] = std::move(factory);
}

std::unique_ptr<Scale> ScaleRegistry::create(std::string_view name) const {
    // 自定义登记优先：允许覆盖内置名（测试与实验需要）。
    if (const auto it = factories_.find(name); it != factories_.end()) {
        return it->second();
    }
    if (name == kBuiltinLinear) {
        return std::make_unique<LinearScale>();
    }
    if (name == kBuiltinLog) {
        return std::make_unique<LogScale>();
    }
    if (name == kBuiltinTime) {
        return std::make_unique<TimeScale>();
    }
    if (name == kBuiltinCategory) {
        return std::make_unique<CategoryScale>();
    }
    if (name == kBuiltinBand) {
        return std::make_unique<BandScale>();
    }
    return nullptr;
}

bool ScaleRegistry::supports(std::string_view name) const {
    return factories_.find(name) != factories_.end() || name == kBuiltinLinear ||
           name == kBuiltinLog || name == kBuiltinTime ||
           name == kBuiltinCategory || name == kBuiltinBand;
}

std::vector<std::string> ScaleRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(factories_.size() + 2);
    out.emplace_back(kBuiltinLinear);
    out.emplace_back(kBuiltinLog);
    out.emplace_back(kBuiltinTime);
    out.emplace_back(kBuiltinCategory);
    out.emplace_back(kBuiltinBand);
    for (const auto& [name, factory] : factories_) {
        (void)factory;
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::size_t ScaleRegistry::size() const noexcept {
    return names().size();
}

bool ScaleRegistry::empty() const noexcept {
    return names().empty();
}

}  // namespace termchart::scale
