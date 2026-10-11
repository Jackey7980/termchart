// registry.hpp —— 比例尺注册表（与 chart/raster 同一套契约）
//
// instance() 用 function-local static：**构造前注册、构造后只读**（调用方在自身构造
// 时对注册表做浅拷贝快照并冻结，运行期注册属 UB——见 docs/design/registry.md）。
// 内置 `linear` / `log` 直接在 create() 里给出；自定义比例尺用 register_scale()
// 登记，重名以最后一次为准（便于测试里临时替换）。
#ifndef TERMCHART_SCALE_REGISTRY_HPP
#define TERMCHART_SCALE_REGISTRY_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"

namespace termchart::scale {

class ScaleRegistry {
public:
    using Factory = std::function<std::unique_ptr<Scale>()>;

    static ScaleRegistry& instance() noexcept;

    ScaleRegistry(const ScaleRegistry&) = delete;
    ScaleRegistry& operator=(const ScaleRegistry&) = delete;

    void register_scale(std::string_view name, Factory factory);

    // 未知名字返回 nullptr（不抛异常）。
    [[nodiscard]] std::unique_ptr<Scale> create(std::string_view name) const;
    [[nodiscard]] bool supports(std::string_view name) const;

    // 全部可用名字（内置 + 自定义），按字典序去重；供快照/诊断/CLI 提示用。
    [[nodiscard]] std::vector<std::string> names() const;

private:
    ScaleRegistry() = default;
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_REGISTRY_HPP
