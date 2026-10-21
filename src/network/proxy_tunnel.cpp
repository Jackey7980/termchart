// proxy_tunnel.cpp —— 代理隧道协议（HTTP CONNECT + SOCKS5）
//
// 纯字节的构造/解析全部抽成自由函数（proxy_internal.hpp 声明），单测离线
// 推演；本文件只负责 socket I/O。设计要点：
//   * CONNECT 响应的"已建立"判定只看状态行 2xx；之后 socket 透明，上层
//     TLS（SChannel）与 HTTP 原样工作，SNI/证书校验仍是最终目标 host；
//   * SOCKS5 用 ATYP=03 域名——DNS 交给代理解析，这是 Clash 分流规则生效
//     与防 DNS 污染的关键（若在本地解析，分流与隐私都输）；
//   * 两种协议都支持认证：CONNECT 用 Proxy-Authorization: Basic（util/base64），
//     SOCKS5 用 RFC 1929 user/pass。
#include "src/network/proxy_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/network/socket.hpp"
#include "termchart/util/base64.hpp"
#include "termchart/util/logging.hpp"

namespace termchart::network::proxy {
namespace {

constexpr std::size_t kMaxHeaderBytes = 64u * 1024u;

// 读满 n 字节（socket 单次读语义 → 循环）。EOF/错误 → false。
bool read_exact(TcpSocket& socket, std::span<char> buffer) noexcept {
    std::size_t got = 0;
    while (got < buffer.size()) {
        const Result<std::size_t> n =
            socket.read(std::span<char>{buffer.data() + got, buffer.size() - got});
        if (!n.has_value() || *n == 0) {
            return false;
        }
        got += *n;
    }
    return true;
}

// 读到 delim 出现（delim 计入返回）或超限/EOF。用于 CONNECT 响应头。
Result<std::string> read_until(TcpSocket& socket, std::string_view delim) noexcept {
    std::string buf;
    std::vector<char> chunk(4096);
    for (;;) {
        const std::size_t at = buf.find(delim);
        if (at != std::string::npos) {
            buf.resize(at + delim.size());
            return buf;
        }
        if (buf.size() > kMaxHeaderBytes) {
            return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
        }
        const Result<std::size_t> n = socket.read(chunk);
        if (!n.has_value()) {
            return Unexpected<ErrorCode>(n.error());
        }
        if (*n == 0) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);  // EOF 没等到 delim
        }
        buf.append(chunk.data(), *n);
    }
}

void append_port(std::string& out, std::uint16_t port) noexcept {
    out.push_back(static_cast<char>((port >> 8) & 0xFF));
    out.push_back(static_cast<char>(port & 0xFF));
}

}  // namespace

std::string build_connect_request(std::string_view target_host, std::uint16_t target_port,
                                  const Config& cfg) noexcept {
    std::string req = "CONNECT ";
    req.append(target_host);
    req.push_back(':');
    req += std::to_string(target_port);
    req += " HTTP/1.1\r\nHost: ";
    req.append(target_host);
    req.push_back(':');
    req += std::to_string(target_port);
    req += "\r\n";
    if (!cfg.username.empty()) {
        const std::string raw = cfg.username + ":" + cfg.password;
        req += "Proxy-Authorization: Basic ";
        req += util::base64_encode(raw);
        req += "\r\n";
    }
    req += "\r\n";
    return req;
}

Status parse_connect_status(std::string_view status_line) noexcept {
    // "HTTP/1.1 200 Connection established"
    const std::size_t sp1 = status_line.find(' ');
    if (sp1 == std::string_view::npos) {
        return ErrorCode::ParseError;
    }
    const std::size_t sp2 = status_line.find(' ', sp1 + 1);
    const std::string_view code_sv =
        status_line.substr(sp1 + 1, sp2 == std::string_view::npos
                                        ? std::string_view::npos
                                        : sp2 - sp1 - 1);
    std::uint32_t code = 0;
    for (const char ch : code_sv) {
        if (ch < '0' || ch > '9') {
            return ErrorCode::ParseError;
        }
        code = code * 10u + static_cast<std::uint32_t>(ch - '0');
    }
    if (code >= 200 && code < 300) {
        return ErrorCode::Ok;
    }
    // 4xx/5xx：隧道没建成。调用方按 from_detect 决定回落直连。
    return code == 407 ? ErrorCode::Unsupported : ErrorCode::IoError;
}

std::string build_socks5_greet(const Config& cfg) noexcept {
    // VER=5；方法：00 无认证；若给了凭据再宣告 02 用户名密码。
    if (cfg.username.empty()) {
        return std::string("\x05\x01\x00", 3);
    }
    return std::string("\x05\x02\x00\x02", 4);
}

std::string build_socks5_auth(const Config& cfg) noexcept {
    std::string out("\x01", 1);
    out.push_back(static_cast<char>(cfg.username.size()));
    out += cfg.username;
    out.push_back(static_cast<char>(cfg.password.size()));
    out += cfg.password;
    return out;
}

std::string build_socks5_connect(std::string_view target_host, std::uint16_t target_port) noexcept {
    std::string out("\x05\x01\x00\x03", 4);
    if (target_host.empty() || target_host.size() > 255) {
        return {};  // 域名长度超 SOCKS5 上限：调用方判空拒绝
    }
    out.push_back(static_cast<char>(target_host.size()));
    out.append(target_host);
    append_port(out, target_port);
    return out;
}

Status parse_socks5_method_reply(std::span<const char> reply) noexcept {
    if (reply.size() < 2 || static_cast<std::uint8_t>(reply[0]) != 0x05) {
        return ErrorCode::ParseError;
    }
    const auto method = static_cast<std::uint8_t>(reply[1]);
    if (method == 0x00) {
        return ErrorCode::Ok;
    }
    if (method == 0x02) {
        return ErrorCode::Ok;  // 代理选了用户名密码：由调用方继续认证段
    }
    if (method == 0xFF) {
        return ErrorCode::Unsupported;  // 无可接受方法
    }
    return ErrorCode::Unsupported;
}

Status parse_socks5_auth_reply(std::span<const char> reply) noexcept {
    if (reply.size() < 2 || static_cast<std::uint8_t>(reply[0]) != 0x01) {
        return ErrorCode::ParseError;
    }
    return static_cast<std::uint8_t>(reply[1]) == 0x00 ? ErrorCode::Ok : ErrorCode::IoError;
}

Status parse_socks5_connect_reply(std::span<const char> head) noexcept {
    if (head.size() < 4 || static_cast<std::uint8_t>(head[0]) != 0x05) {
        return ErrorCode::ParseError;
    }
    const auto rep = static_cast<std::uint8_t>(head[1]);
    if (rep == 0x00) {
        return ErrorCode::Ok;
    }
    // 常见 rep：01 一般失败 / 02 规则不允许 / 05 连接被拒 / 07 地址类型不支持。
    return rep == 0x02 ? ErrorCode::Unsupported : ErrorCode::IoError;
}

Status http_connect(TcpSocket& proxy_socket, std::string_view target_host,
                    std::uint16_t target_port, const Config& cfg) noexcept {
    const std::string request = build_connect_request(target_host, target_port, cfg);
    const Result<std::size_t> sent = proxy_socket.write(
        std::span<const char>{request.data(), request.size()});
    if (!sent.has_value() || *sent != request.size()) {
        return ErrorCode::IoError;
    }
    const Result<std::string> head = read_until(proxy_socket, "\r\n\r\n");
    if (!head.has_value()) {
        return head.error();
    }
    // 状态行 = 首行（到第一个 \n）。
    const std::size_t nl = head->find('\n');
    const std::string_view status_line =
        head->substr(0, nl == std::string::npos ? head->size() : nl);
    const Status st = parse_connect_status(status_line);
    if (st != ErrorCode::Ok) {
        TERMCHART_LOG_WARN("network/proxy: CONNECT 隧道未建立（状态行非 2xx）");
    }
    return st;
}

Status socks5_connect(TcpSocket& proxy_socket, std::string_view target_host,
                      std::uint16_t target_port, const Config& cfg) noexcept {
    // 1) 方法协商。
    const std::string greet = build_socks5_greet(cfg);
    if (greet.empty()) {
        return ErrorCode::InvalidArgument;
    }
    const Result<std::size_t> gsent = proxy_socket.write(
        std::span<const char>{greet.data(), greet.size()});
    if (!gsent.has_value() || *gsent != greet.size()) {
        return ErrorCode::IoError;
    }
    char method_reply[2] = {};
    if (!read_exact(proxy_socket, std::span<char>{method_reply, 2})) {
        return ErrorCode::IoError;
    }
    const Status mst =
        parse_socks5_method_reply(std::span<const char>{method_reply, 2});
    if (mst != ErrorCode::Ok) {
        return mst;
    }
    const auto chosen = static_cast<std::uint8_t>(method_reply[1]);
    // 2) 用户名密码认证（代理选了 02）。
    if (chosen == 0x02) {
        if (cfg.username.empty()) {
            return ErrorCode::Unsupported;  // 代理要认证而没给
        }
        const std::string auth = build_socks5_auth(cfg);
        const Result<std::size_t> asent = proxy_socket.write(
            std::span<const char>{auth.data(), auth.size()});
        if (!asent.has_value() || *asent != auth.size()) {
            return ErrorCode::IoError;
        }
        char auth_reply[2] = {};
        if (!read_exact(proxy_socket, std::span<char>{auth_reply, 2})) {
            return ErrorCode::IoError;
        }
        const Status ast =
            parse_socks5_auth_reply(std::span<const char>{auth_reply, 2});
        if (ast != ErrorCode::Ok) {
            return ast;
        }
    }
    // 3) CONNECT（ATYP=03 域名，DNS 交代理）。
    const std::string connect = build_socks5_connect(target_host, target_port);
    if (connect.empty()) {
        return ErrorCode::InvalidArgument;
    }
    const Result<std::size_t> csent = proxy_socket.write(
        std::span<const char>{connect.data(), connect.size()});
    if (!csent.has_value() || *csent != connect.size()) {
        return ErrorCode::IoError;
    }
    // 4) 读响应：先 4 字节定长头，按 ATYP 补读地址与端口。
    char head[4] = {};
    if (!read_exact(proxy_socket, std::span<char>{head, 4})) {
        return ErrorCode::IoError;
    }
    const auto atyp = static_cast<std::uint8_t>(head[3]);
    std::size_t rest = 0;
    switch (atyp) {
        case 0x01: rest = 4 + 2; break;  // IPv4
        case 0x03: {                   // 域名：1 字节长度 + 域名 + 端口
            char len = 0;
            if (!read_exact(proxy_socket, std::span<char>{&len, 1})) {
                return ErrorCode::IoError;
            }
            rest = static_cast<std::uint8_t>(len) + 2;
            break;
        }
        case 0x04: rest = 16 + 2; break;  // IPv6
        default: return ErrorCode::Unsupported;
    }
    std::vector<char> tail(rest);
    if (!read_exact(proxy_socket, std::span<char>{tail.data(), tail.size()})) {
        return ErrorCode::IoError;
    }
    const Status cst = parse_socks5_connect_reply(std::span<const char>{head, 4});
    if (cst != ErrorCode::Ok) {
        TERMCHART_LOG_WARN("network/proxy: SOCKS5 CONNECT 被代理拒绝");
    }
    return cst;
}

}  // namespace termchart::network::proxy
