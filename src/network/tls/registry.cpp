// registry.cpp —— TLS 后端注册表实现

#include "termchart/network/tls/registry.hpp"

#include "termchart/network/tls/schannel.hpp"

namespace termchart::network::tls {

// 造一份登记好内置后端的注册表（TlsRegistry 的私有静态方法）。
//
// 两个不能省的要点：
//   1. **不能调 TlsRegistry::instance()**：函数局部 static 的初始化里重入
//      同一个 static 的初始化，同一线程会死在 guard 上（重入静态初始化是
//      UB，libstdc++ 的 __cxa_guard_acquire 不递归）。成员方法
//      register_backend 直接往 map 写，没有这条重入路径。
//   2. 必须**直接引用** builtin 的 create 工厂：静态库里"无人引用符号的
//      TU"整体不进最终二进制，其静态初始化不会运行（经典静态库陷阱）——
//      不能只靠 schannel.cpp 里的自注册，由这里显式拉一把。平台后端缺席时
//      （POSIX 无 OpenSSL）对应工厂缺席，https 随后 fail-loud 报 Unsupported。
TlsRegistry TlsRegistry::with_builtins() {
    TlsRegistry registry;
    registry.register_backend("schannel",
                              [] { return schannel::create_backend(); });
    // registry.register_backend("openssl", ...);  // 后续：可选依赖编入时
    return registry;
}

TlsRegistry& TlsRegistry::instance() noexcept {
    static TlsRegistry registry = with_builtins();
    return registry;
}

void TlsRegistry::register_backend(std::string_view name, Creator creator) {
    creators_[std::string(name)] = std::move(creator);
}

bool TlsRegistry::supports(std::string_view name) const {
    return creators_.find(name) != creators_.end();
}

std::vector<std::string> TlsRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(creators_.size());
    for (const auto& [name, creator] : creators_) {
        out.push_back(name);
    }
    return out;
}

std::size_t TlsRegistry::size() const noexcept {
    return creators_.size();
}

bool TlsRegistry::empty() const noexcept {
    return creators_.empty();
}

std::unique_ptr<TlsBackend> TlsRegistry::create(std::string_view name) const {
    if (const auto it = creators_.find(name); it != creators_.end()) {
        return it->second();
    }
    return nullptr;
}

}  // namespace termchart::network::tls
