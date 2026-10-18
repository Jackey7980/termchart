// registry.cpp —— TLS 后端注册表实现

#include "termchart/network/tls/registry.hpp"

namespace termchart::network::tls {

TlsRegistry& TlsRegistry::instance() noexcept {
    static TlsRegistry registry;
    return registry;
}

void TlsRegistry::register_backend(std::string_view name, Creator creator) {
    creators_[std::string(name)] = std::move(creator);
}

bool TlsRegistry::supports(std::string_view name) const {
    return creators_.find(name) != creators_.end();
}

std::unique_ptr<TlsBackend> TlsRegistry::create(std::string_view name) const {
    if (const auto it = creators_.find(name); it != creators_.end()) {
        return it->second();
    }
    return nullptr;
}

}  // namespace termchart::network::tls
