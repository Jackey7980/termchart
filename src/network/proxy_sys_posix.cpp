// proxy_sys_posix.cpp —— POSIX 系统代理读取
//
// macOS：SystemConfiguration 的 SCDynamicStoreCopyProxies（Surge/ClashX 等
// 开启"设置为系统代理"时写的就是这里）；其余 POSIX（Linux 等）没有系统级
// 代理设置标准，恒返回 None——环境变量是那些平台的唯一自动来源。
#include "src/network/proxy_internal.hpp"

#include <cstring>
#include <string>
#include <string_view>

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

namespace termchart::network::proxy {
namespace {

std::string cf_to_string(CFStringRef value) {
    if (value == nullptr) {
        return {};
    }
    const char* utf8 = CFStringGetCStringPtr(value, kCFStringEncodingUTF8);
    if (utf8 != nullptr) {
        return std::string{utf8};
    }
    const auto length = CFStringGetLength(value);
    const CFIndex max_size =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<std::size_t>(max_size), '\0');
    if (!CFStringGetCString(value, out.data(), max_size, kCFStringEncodingUTF8)) {
        return {};
    }
    out.resize(std::strlen(out.c_str()));
    return out;
}

std::string cf_number_to_string(CFNumberRef value) {
    if (value == nullptr) {
        return {};
    }
    int number = 0;
    if (!CFNumberGetValue(value, kCFNumberIntType, &number) || number <= 0) {
        return {};
    }
    return std::to_string(number);
}

}  // namespace

Config read_system(std::string_view target_scheme, std::string_view) noexcept {
    Config cfg;
    const CFDictionaryRef proxies = SCDynamicStoreCopyProxies(nullptr);
    if (proxies == nullptr) {
        return cfg;
    }
    // https 目标优先 HTTPS 代理项，回退 HTTP 项（都是 CONNECT 型）。
    const bool https = target_scheme == "https";
    const void* enable_key = https ? kSCPropNetProxiesHTTPSEnable : kSCPropNetProxiesHTTPEnable;
    const void* proxy_key = https ? kSCPropNetProxiesHTTPSProxy : kSCPropNetProxiesHTTPProxy;
    const void* port_key = https ? kSCPropNetProxiesHTTPSPort : kSCPropNetProxiesHTTPPort;

    const auto enabled = static_cast<CFTypeRef>(
        CFDictionaryGetValue(proxies, enable_key));
    if (enabled == nullptr || CFGetTypeID(enabled) != CFNumberGetTypeID()) {
        // 未显式启用：再看另一项（不少工具只设其中一类）。
        const void* alt_enable =
            https ? kSCPropNetProxiesHTTPEnable : kSCPropNetProxiesHTTPSEnable;
        const auto alt = static_cast<CFTypeRef>(CFDictionaryGetValue(proxies, alt_enable));
        int flag = 0;
        if (alt == nullptr || CFGetTypeID(alt) != CFNumberGetTypeID() ||
            !CFNumberGetValue(static_cast<CFNumberRef>(alt), kCFNumberIntType, &flag) ||
            flag == 0) {
            CFRelease(proxies);
            return cfg;
        }
        proxy_key = https ? kSCPropNetProxiesHTTPProxy : kSCPropNetProxiesHTTPSProxy;
        port_key = https ? kSCPropNetProxiesHTTPPort : kSCPropNetProxiesHTTPSPort;
    }

    const auto host = static_cast<CFStringRef>(CFDictionaryGetValue(proxies, proxy_key));
    const auto port = static_cast<CFNumberRef>(CFDictionaryGetValue(proxies, port_key));
    const std::string host_str = cf_to_string(host);
    const std::string port_str = cf_number_to_string(port);
    CFRelease(proxies);
    if (host_str.empty() || port_str.empty()) {
        return cfg;
    }
    const Result<Config> parsed = parse(host_str + ":" + port_str);
    if (!parsed.has_value()) {
        return cfg;
    }
    cfg = *parsed;
    cfg.kind = Kind::HttpConnect;
    cfg.from_detect = true;
    return cfg;
}

}  // namespace termchart::network::proxy

#else  // !APPLE__

namespace termchart::network::proxy {

Config read_system(std::string_view, std::string_view) noexcept { return Config{}; }

}  // namespace termchart::network::proxy

#endif  // __APPLE__
