// registry.cpp —— 图表注册表实现
#include "termchart/chart/registry.hpp"

#include <map>
#include <string>

namespace termchart::chart {

ChartRegistry& ChartRegistry::instance() noexcept {
    static ChartRegistry reg;
    return reg;
}

void ChartRegistry::register_chart(std::string_view name, Factory factory) {
    factories_[std::string(name)] = std::move(factory);
}

std::unique_ptr<Chart> ChartRegistry::create(std::string_view name) const {
    auto it = factories_.find(std::string(name));
    if (it == factories_.end()) {
        return nullptr;
    }
    return it->second();
}

bool ChartRegistry::supports(std::string_view name) const {
    return factories_.find(std::string(name)) != factories_.end();
}

}  // namespace termchart::chart
