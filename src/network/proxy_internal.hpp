// proxy_internal.hpp —— 代理探测与隧道（network 模块内部，**勿入公开头**）
//
// 与 platform_internal.hpp 同纪律：这是 src/ 里不对应 include/ 同名头的私有头，
// 只有 src/network/*.cpp 与 tests 可见。公开面只有 HttpOptions::proxy 一个字符串
// 字段——新手零配置（探测链自动生效），高级用户显式指定。
//
// 支持两种代理协议：
//   * HTTP CONNECT 隧道（企业代理主流；Clash 类 mixed 端口同样支持）；
//   * SOCKS5（RFC 1928 + 1929 用户名密码；Cloudflare WARP 的 proxy 模式）。
// 探测链（显式 > 环境变量 > 系统代理 > 直连）见 resolve() 的文档。
#ifndef TERMCHART_SRC_NETWORK_PROXY_INTERNAL_HPP
#define TERMCHART_SRC_NETWORK_PROXY_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/network/socket.hpp"

namespace termchart::network {
namespace proxy {

enum class Kind : std::uint8_t {
    None,        // 直连（resolve 的"无代理"结果）
    HttpConnect, // HTTP 代理 + CONNECT 隧道
    Socks5,      // SOCKS5 代理
};

struct Config {
    Kind kind = Kind::None;
    std::string host;
    std::uint16_t port = 0;
    std::string username;  // 可空（HttpConnect: Proxy-Authorization Basic；
                           // Socks5: user/pass 方法）
    std::string password;
    // true = 环境变量/系统探测而来（失败允许回落直连的分级降级）；
    // false = 调用方显式指定（失败必须 fail-loud，不回落）。
    bool from_detect = false;
};

// 解析代理字符串："http://h:p" / "socks5://h:p"（socks5h 同义）/ "h:p"
// （裸 host:port 默认 http）。可带 userinfo："socks5://user:pass@h:p"。
// 失败：InvalidArgument（空/无端口/端口非法）/ Unsupported（其它 scheme）。
[[nodiscard]] Result<Config> parse(std::string_view text) noexcept;

// 环境变量抓取（生产路径）。键名固定六枚：HTTPS_PROXY / https_proxy /
// HTTP_PROXY / http_proxy / ALL_PROXY / all_proxy（curl 语义只认大写，
// requests 两者都认，这里都抓，由 resolve 按目标 scheme 选用）。
[[nodiscard]] std::vector<std::pair<std::string, std::string>> read_env() noexcept;

// 系统代理设置：Windows 读注册表（IE/WinINET 配置，Clash 类默认就写这里），
// macOS 读 SystemConfiguration；Linux 无系统标准 → None。target_host 用于
// ProxyOverride 匹配（<local>/localhost/127.0.0.1 等不代理）。
[[nodiscard]] Config read_system(std::string_view target_scheme,
                                 std::string_view target_host) noexcept;

// 分号/逗号分隔的代理绕过列表（no_proxy / ProxyOverride）是否命中目标 host。
// 条目语法：精确主机名、点分后缀（.example.com 匹配子域）、"*" 通配、
// "<local>"（无点名字与 localhost/回环）。proxy_sys_win32.cpp 复用。
[[nodiscard]] bool list_matches(std::string_view list, std::string_view host) noexcept;

// 探测链决策（纯函数，测试直接构造输入）：
//   1) explicit_cfg.kind != None → 原样采用（from_detect=false，调用方 fail-loud）；
//   2) 环境变量：https 目标依次 HTTPS_PROXY→ALL_PROXY；http 目标依次
//      HTTP_PROXY→ALL_PROXY（不交叉，curl 语义）；NO_PROXY 命中目标则跳过本层；
//   3) sys_proxy（系统设置，通常 HttpConnect 型，服务 http 与 https 两种目标）；
//   4) 都没有 → None（直连）。
// 环境变量格式损坏只 warn 并继续下一级（环境不是显式请求）；显式配置的
// 损坏在 parse 阶段就已 fail-loud，到不了这里。
[[nodiscard]] Config resolve(std::string_view target_scheme, std::string_view target_host,
                             const Config& explicit_cfg,
                             const std::vector<std::pair<std::string, std::string>>& env,
                             const Config& sys_proxy) noexcept;

// ── 隧道（调用方已把 socket 连到代理）────────────────────────────────────
// HTTP CONNECT：构造请求字节（含可选 Proxy-Authorization Basic）。
[[nodiscard]] std::string build_connect_request(std::string_view target_host,
                                                std::uint16_t target_port,
                                                const Config& cfg) noexcept;
// 解析 CONNECT 响应状态行首行；2xx → Ok，其余 → IoError（带码，不回落由
// 调用方按 from_detect 决定）。
[[nodiscard]] Status parse_connect_status(std::string_view status_line) noexcept;

// SOCKS5 握手字节（greet/认证/connect 三段；ATYP=03 域名——DNS 交由代理，
// 这是分流正确性与防 DNS 污染的关键）。
[[nodiscard]] std::string build_socks5_greet(const Config& cfg) noexcept;
[[nodiscard]] std::string build_socks5_auth(const Config& cfg) noexcept;
[[nodiscard]] std::string build_socks5_connect(std::string_view target_host,
                                               std::uint16_t target_port) noexcept;
// 解析 SOCKS5 方法协商/认证/连接三段响应（定长头部部分），返回 rep 状态。
[[nodiscard]] Status parse_socks5_method_reply(std::span<const char> reply) noexcept;
[[nodiscard]] Status parse_socks5_auth_reply(std::span<const char> reply) noexcept;
// 连接响应是变长（atyp 决定地址字节数）；head 至少 4 字节（ver/rep/rsv/atyp）。
[[nodiscard]] Status parse_socks5_connect_reply(std::span<const char> head) noexcept;

// 隧道总入口：按 cfg.kind 走对应握手。Ok = 隧道就绪，socket 此后透明。
[[nodiscard]] Status http_connect(TcpSocket& proxy_socket, std::string_view target_host,
                                  std::uint16_t target_port, const Config& cfg) noexcept;
[[nodiscard]] Status socks5_connect(TcpSocket& proxy_socket, std::string_view target_host,
                                    std::uint16_t target_port, const Config& cfg) noexcept;

}  // namespace proxy
}  // namespace termchart::network

#endif  // TERMCHART_SRC_NETWORK_PROXY_INTERNAL_HPP
