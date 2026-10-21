// proxy_sys_win32.cpp —— Windows 系统代理读取（注册表 IE/WinINET 配置）
//
// 隔离层：本文件允许 #ifdef 与 <windows.h>（与 socket_win32.cpp 同纪律）。
// Clash for Windows / Clash Verge 等默认开启"系统代理"时写的就是这里：
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings
//     ProxyEnable  (DWORD) = 1
//     ProxyServer  (SZ)    = "127.0.0.1:7890"
//                         或 "http=127.0.0.1:7890;https=127.0.0.1:7890;socks=..."
//     ProxyOverride (SZ)   = "<local>;localhost;127.0.0.1;..."
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <string_view>

#include "src/network/proxy_internal.hpp"

namespace termchart::network::proxy {
namespace {

constexpr const wchar_t* kSettingsKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

std::wstring widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                               static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(wide_len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          out.data(), wide_len);
    return out;
}

std::string narrow(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int narrow_len = ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                                 static_cast<int>(text.size()), nullptr, 0,
                                                 nullptr, nullptr);
    if (narrow_len <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(narrow_len), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          out.data(), narrow_len, nullptr, nullptr);
    return out;
}

bool read_dword(const wchar_t* name, DWORD& out) noexcept {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (::RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_DWORD, nullptr,
                       &value, &size) != ERROR_SUCCESS) {
        return false;
    }
    out = value;
    return true;
}

bool read_string(const wchar_t* name, std::string& out) noexcept {
    wchar_t buffer[1024] = {};
    DWORD size = sizeof(buffer);
    if (::RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_SZ, nullptr,
                       buffer, &size) != ERROR_SUCCESS ||
        size < sizeof(wchar_t)) {
        return false;
    }
    const auto chars = static_cast<std::size_t>(size) / sizeof(wchar_t);
    buffer[chars > 0 ? chars - 1 : 0] = L'\0';  // 去 RegGetValue 自带的 NUL
    out = narrow(buffer);
    return !out.empty();
}

// 从 ProxyServer 里取某协议的 "host:port"。
//   全局格式 "127.0.0.1:7890"            → proto 任意都返回它；
//   分协议 "http=h:p;https=h:p;..."      → 只返回指定 proto 的项。
bool server_entry(std::string_view server, std::string_view proto, std::string& out) noexcept {
    if (server.find('=') == std::string_view::npos) {
        out = std::string{server};  // 全局格式
        return !out.empty();
    }
    std::size_t pos = 0;
    while (pos < server.size()) {
        std::size_t end = server.find(';', pos);
        if (end == std::string_view::npos) {
            end = server.size();
        }
        const std::string_view item = server.substr(pos, end - pos);
        const std::size_t eq = item.find('=');
        if (eq != std::string_view::npos) {
            const std::string_view key = item.substr(0, eq);
            const std::string_view value = item.substr(eq + 1);
            if (key == proto) {
                out = std::string{value};
                return !out.empty();
            }
        }
        pos = end + 1;
    }
    return false;
}

}  // namespace

Config read_system(std::string_view target_scheme, std::string_view target_host) noexcept {
    Config cfg;
    DWORD enable = 0;
    if (!read_dword(L"ProxyEnable", enable) || enable == 0) {
        return cfg;  // 系统代理未开
    }
    std::string server;
    if (!read_string(L"ProxyServer", server)) {
        return cfg;
    }
    // ProxyOverride 命中 → 本目标不走系统代理。
    std::string override_list;
    if (read_string(L"ProxyOverride", override_list) && list_matches(override_list, target_host)) {
        return cfg;
    }
    // https 目标优先 https= 项，回退全局；http 目标同理。
    std::string entry;
    if (!server_entry(server, target_scheme, entry)) {
        if (!server_entry(server, "http", entry)) {
            return cfg;
        }
    }
    const Result<Config> parsed = parse(entry);
    if (!parsed.has_value()) {
        return cfg;  // 系统配置坏：不当真，退回直连（系统设置不是显式请求）
    }
    cfg = *parsed;
    cfg.kind = Kind::HttpConnect;  // WinINET 的代理对 https 目标一律 CONNECT
    cfg.from_detect = true;
    return cfg;
}

}  // namespace termchart::network::proxy

#else  // !_WIN32

#include "src/network/proxy_internal.hpp"

namespace termchart::network::proxy {

Config read_system(std::string_view, std::string_view) noexcept { return Config{}; }

}  // namespace termchart::network::proxy

#endif  // _WIN32
