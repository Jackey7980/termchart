// registry.hpp —— TLS 后端注册表（可扩展、构造期冻结）
//
// 与 raster/registry.hpp 同构（docs/design/registry.md 的口径）：
//   * instance() 一律 function-local static；
//   * 构造后只读（拷贝/赋值删除）；注册的唯一安全时机是 App 构造前；
//   * 注册的是**工厂**（每次 create 给新实例），不是原型；
//   * 查询：supports(name) / create(name)；未知名字返回 nullptr，
//     由调用方显式报错（fail-loud，不静默降级——绝不偷偷退回明文）。
#ifndef TERMCHART_NETWORK_TLS_REGISTRY_HPP
#define TERMCHART_NETWORK_TLS_REGISTRY_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/network/tls/backend.hpp"

namespace termchart::network::tls {

class TlsRegistry {
public:
    using Creator = std::function<std::unique_ptr<TlsBackend>()>;

    // 全局唯一实例；首次调用即冻结，之后不可增删。
    static TlsRegistry& instance() noexcept;

    TlsRegistry(const TlsRegistry&) = delete;
    TlsRegistry& operator=(const TlsRegistry&) = delete;
    TlsRegistry(TlsRegistry&&) = default;
    TlsRegistry& operator=(TlsRegistry&&) = default;

    // 扩展点：登记一个 TLS 后端工厂（重复名字以最后一次为准）。
    void register_backend(std::string_view name, Creator creator);

    // 是否已有该名字的后端工厂。
    [[nodiscard]] bool supports(std::string_view name) const;

    // 已登记的后端名字（按字典序），用于"空名字时挑默认"与诊断列举。
    [[nodiscard]] std::vector<std::string> names() const;

    // 按名字造一个后端实例；未登记返回 nullptr（调用方负责显式报错）。
    [[nodiscard]] std::unique_ptr<TlsBackend> create(std::string_view name) const;

private:
    TlsRegistry() = default;

    // 造一份登记好内置后端的注册表（见 registry.cpp 的"不能重入 instance()"注释）。
    [[nodiscard]] static TlsRegistry with_builtins();

    std::map<std::string, Creator, std::less<>> creators_;
};

}  // namespace termchart::network::tls

#endif  // TERMCHART_NETWORK_TLS_REGISTRY_HPP
