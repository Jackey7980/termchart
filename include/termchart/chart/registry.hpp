// registry.hpp —— 图表注册表（RAII 友好、构造期冻结）
//
// 与 RasterRegistry 同一套契约：instance() 是 function-local static，构造后只读。
// 注册的是工厂而非原型，避免 clone 语义；create() 每次给新实例。
// 全仓六个注册表（chart / scale / raster / plugin / dsl / tls）统一提供
// names()/size()/empty() 枚举三件套——"已注册了什么"必须可列举，
// AutoViz 路由、CLI --help、诊断都靠它，不许各表口径不一。
#ifndef TERMCHART_CHART_REGISTRY_HPP
#define TERMCHART_CHART_REGISTRY_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class ChartRegistry {
public:
    using Factory = std::function<std::unique_ptr<Chart>()>;

    static ChartRegistry& instance() noexcept;

    ChartRegistry(const ChartRegistry&) = delete;
    ChartRegistry& operator=(const ChartRegistry&) = delete;

    // 登记图种工厂；重名以最后一次为准（与 scale/raster/tls 同口径）。
    void register_chart(std::string_view name, Factory factory);
    [[nodiscard]] std::unique_ptr<Chart> create(std::string_view name) const;
    [[nodiscard]] bool supports(std::string_view name) const;

    // 枚举三件套：已注册图种名（按字典序，确定性输出）。
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    ChartRegistry() = default;
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_REGISTRY_HPP
