// proxy_detect.cpp —— 代理探测链（环境变量 + 决策；系统代理走平台文件）
#include "src/network/proxy_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/util/logging.hpp"

namespace termchart::network::proxy {
namespace {

// host 是否匹配 no_proxy/ProxyOverride 的一个条目。条目语法：精确主机名、
// 点分后缀（".example.com" 匹配子域）、"*" 通配一切；额外认 "<local>"
// （WinINET 惯例：无点主机名与 localhost/127.0.0.1 都算本地）。
bool entry_matches(std::string_view entry, std::string_view host) noexcept {
    while (!entry.empty() && entry.front() == '.') {
        entry.remove_prefix(1);
    }
    if (entry.empty()) {
        return false;
    }
    if (entry == "*") {
        return true;
    }
    if (entry == "<local>") {
        // 本地：无点名字 / localhost / 回环字面量。
        return host.find('.') == std::string_view::npos || host == "localhost" ||
               host == "127.0.0.1" || host == "::1" || host == "[::1]";
    }
    // 精确或子域后缀。
    if (host == entry) {
        return true;
    }
    return host.size() > entry.size() + 1 && host.substr(host.size() - entry.size()) == entry &&
           host[host.size() - entry.size() - 1] == '.';
}

// 分号/逗号分隔的列表里找匹配项（no_proxy 与 ProxyOverride 同口径）。
const std::string* find_env(const std::vector<std::pair<std::string, std::string>>& env,
                            const char* key) noexcept {
    for (const auto& kv : env) {
        if (kv.first == key) {
            return &kv.second;
        }
    }
    return nullptr;
}

}  // namespace

// 见 proxy_internal.hpp：供 proxy_sys_win32.cpp 复用（WinINET 的 ProxyOverride
// 与 no_proxy 同口径）。
bool list_matches(std::string_view list, std::string_view host) noexcept {
    std::size_t pos = 0;
    while (pos <= list.size()) {
        std::size_t end = list.find_first_of(";,", pos);
        if (end == std::string_view::npos) {
            end = list.size();
        }
        std::string_view entry = list.substr(pos, end - pos);
        while (!entry.empty() && (entry.front() == ' ' || entry.front() == '\t')) {
            entry.remove_prefix(1);
        }
        while (!entry.empty() && (entry.back() == ' ' || entry.back() == '\t')) {
            entry.remove_suffix(1);
        }
        if (entry_matches(entry, host)) {
            return true;
        }
        if (end == list.size()) {
            break;
        }
        pos = end + 1;
    }
    return false;
}

Result<Config> parse(std::string_view text) noexcept {
    Config cfg;
    std::string_view rest = text;
    while (!rest.empty() && rest.front() == ' ') {
        rest.remove_prefix(1);
    }
    if (rest.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }

    Kind kind = Kind::HttpConnect;
    const std::size_t sep = rest.find("://");
    if (sep != std::string_view::npos) {
        const std::string_view scheme = rest.substr(0, sep);
        if (scheme == "http" || scheme == "https") {
            kind = Kind::HttpConnect;  // https:// 代理地址同样用 CONNECT
        } else if (scheme == "socks5" || scheme == "socks5h") {
            kind = Kind::Socks5;
        } else {
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
        }
        rest = rest.substr(sep + 3);
    }

    // userinfo@host:port
    const std::size_t at = rest.rfind('@');
    if (at != std::string_view::npos) {
        const std::string_view userinfo = rest.substr(0, at);
        rest = rest.substr(at + 1);
        const std::size_t colon = userinfo.find(':');
        if (colon != std::string_view::npos) {
            cfg.username = std::string{userinfo.substr(0, colon)};
            cfg.password = std::string{userinfo.substr(colon + 1)};
        } else {
            cfg.username = std::string{userinfo};
        }
    }

    // host:port（IPv6 字面量代理从简不支持：本库的代理场景无此需求，
    // 显式报错胜过猜）。
    const std::size_t colon = rest.rfind(':');
    if (colon == std::string_view::npos) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    cfg.host = std::string{rest.substr(0, colon)};
    const std::string_view port_sv = rest.substr(colon + 1);
    std::uint32_t port = 0;
    for (const char ch : port_sv) {
        if (ch < '0' || ch > '9') {
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        port = port * 10u + static_cast<std::uint32_t>(ch - '0');
        if (port > 65535u) {
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
    }
    if (cfg.host.empty() || port == 0) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    cfg.port = static_cast<std::uint16_t>(port);
    cfg.kind = kind;
    return cfg;
}

std::vector<std::pair<std::string, std::string>> read_env() noexcept {
    static const char* kKeys[] = {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY",
                                  "http_proxy",  "ALL_PROXY",   "all_proxy"};
    std::vector<std::pair<std::string, std::string>> out;
    for (const char* key : kKeys) {
        const char* value = std::getenv(key);
        if (value != nullptr && *value != '\0') {
            out.emplace_back(key, value);
        }
    }
    return out;
}

Config resolve(std::string_view target_scheme, std::string_view target_host,
               const Config& explicit_cfg,
               const std::vector<std::pair<std::string, std::string>>& env,
               const Config& sys_proxy) noexcept {
    // 1) 显式优先（parse 阶段已 fail-loud，这里直接采用）。
    if (explicit_cfg.kind != Kind::None) {
        return explicit_cfg;
    }

    // 2) 环境变量（按目标 scheme 选键；NO_PROXY 命中则本层整体跳过）。
    const std::string* no_proxy = find_env(env, "NO_PROXY");
    if (no_proxy == nullptr) {
        no_proxy = find_env(env, "no_proxy");
    }
    if (no_proxy != nullptr && list_matches(*no_proxy, target_host)) {
        // 命中断言：连系统代理也不看（用户明确说这个目标不走代理）。
        return Config{};
    }
    const bool https = target_scheme == "https";
    const std::string* raw = nullptr;
    if (https) {
        raw = find_env(env, "HTTPS_PROXY");
        if (raw == nullptr) raw = find_env(env, "https_proxy");
    } else {
        raw = find_env(env, "HTTP_PROXY");
        if (raw == nullptr) raw = find_env(env, "http_proxy");
    }
    if (raw == nullptr) raw = find_env(env, "ALL_PROXY");
    if (raw == nullptr) raw = find_env(env, "all_proxy");
    if (raw != nullptr) {
        const Result<Config> parsed = parse(*raw);
        if (parsed.has_value()) {
            Config cfg = *parsed;
            cfg.from_detect = true;
            return cfg;
        }
        TERMCHART_LOG_WARN("network/proxy: 环境变量代理格式损坏，忽略并继续探测");
    }

    // 3) 系统代理（read_system 内部已做 ProxyOverride 判断，None 即不代理）。
    if (sys_proxy.kind != Kind::None) {
        Config cfg = sys_proxy;
        cfg.from_detect = true;
        return cfg;
    }

    // 4) 直连。
    return Config{};
}

}  // namespace termchart::network::proxy
