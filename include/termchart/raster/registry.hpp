// registry.hpp —— 光栅器注册表（可扩展、构造期冻结）
//
// 用 function-local static 的 instance() 保证"构造前注册、构造后只读"的注册表
// 契约（见 docs/design/registry.md）。
//
// 两类来源：
//   * 内置种类（Braille/HalfBlock/...）：硬编码在编译单元里，create(kind) 直接给；
//   * 自定义种类：通过 register_raster(name, creator) 登记，create(name) 查表。
// 后者是**扩展点**——新增光栅器不必改本模块的枚举或 switch（见
// tests/integration/replace_raster_custom.cpp）。
//
// 全仓六个注册表统一提供 names()/size()/empty() 枚举三件套。
#ifndef TERMCHART_RASTER_REGISTRY_HPP
#define TERMCHART_RASTER_REGISTRY_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class RasterRegistry {
public:
    using Creator = std::function<std::unique_ptr<Rasterizer>()>;

    // 全局唯一实例；首次调用即冻结，之后不可增删。
    static RasterRegistry& instance() noexcept;

    RasterRegistry(const RasterRegistry&) = delete;
    RasterRegistry& operator=(const RasterRegistry&) = delete;

    // 扩展点：登记一个自定义光栅器工厂（重复名字以最后一次为准）。
    void register_raster(std::string_view name, Creator creator);
    [[nodiscard]] bool supports_name(std::string_view name) const;

    // 按种类造一个光栅器；未实现返回 nullptr。
    [[nodiscard]] std::unique_ptr<Rasterizer> create(RasterKind kind) const;
    // 按名字造：先查自定义登记，再查内置名字；未知返回 nullptr。
    [[nodiscard]] std::unique_ptr<Rasterizer> create(std::string_view name) const;

    [[nodiscard]] bool supports(RasterKind kind) const noexcept;
    [[nodiscard]] static const char* name(RasterKind kind) noexcept;

    // 枚举三件套：内置 + 自定义去重后的全部名字（按字典序）。
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    RasterRegistry() = default;
    std::map<std::string, Creator, std::less<>> custom_;
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_REGISTRY_HPP
