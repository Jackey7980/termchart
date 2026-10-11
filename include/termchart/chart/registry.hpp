// registry.hpp —— 图表注册表（RAII 友好、构造期冻结）
//
// 与 RasterRegistry 同一套契约：instance() 是 function-local static，构造后只读。
// 注册的是工厂而非原型，避免 clone 语义；create() 每次给新实例。
#ifndef TERMCHART_CHART_REGISTRY_HPP
#define TERMCHART_CHART_REGISTRY_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class ChartRegistry {
public:
    using Factory = std::function<std::unique_ptr<Chart>()>;

    static ChartRegistry& instance() noexcept;

    ChartRegistry(const ChartRegistry&) = delete;
    ChartRegistry& operator=(const ChartRegistry&) = delete;

    void register_chart(std::string_view name, Factory factory);
    [[nodiscard]] std::unique_ptr<Chart> create(std::string_view name) const;
    [[nodiscard]] bool supports(std::string_view name) const;

private:
    ChartRegistry() = default;
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_REGISTRY_HPP
