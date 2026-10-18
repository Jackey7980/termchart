// http.hpp —— HTTP/1.1 客户端（network 模块，L7）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ http:// 明文与 https://（TLS 走 network/tls 注册表的后端，fail-loud）；
//   ✅ 请求：任意 method、自定义头、请求体（Content-Length 自动补）；
//   ✅ 响应：状态行 + 响应头 + 响应体；体长按 Content-Length / chunked /
//      读到连接关闭三种规则解析（HEAD 与 204/304 按规范不收体）；
//   ✅ 重定向：301/302/303/307/308 跟 Location（默认上限 5 次；303 降为 GET）；
//   ✅ 每请求独立连接，发 `Connection: close`（库内不做连接池——图表库的
//      拉取场景一次一个 URL，池化是纯负担）；
//   ⛔ 不支持：HTTP/2、Cookie 管理、代理、摘要认证、请求体流式分块上传、
//      非 ASCII 头的原始字节透传（按 std::string 收）。
// 错误：连接失败 IoError；TLS 后端缺失/握手失败 Unsupported / IoError；
// 响应超限 OutOfRange；协议解析失败 ParseError。
#ifndef TERMCHART_NETWORK_HTTP_HPP
#define TERMCHART_NETWORK_HTTP_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/network/socket.hpp"

namespace termchart::network::http {

// 解析后的 URL。path 含前导 '/' 与查询串；port == 0 表示按 scheme 取默认端口。
struct Uri {
    std::string scheme;  // "http" / "https"（小写）
    std::string host;    // 不含端口；IPv6 字面量保留方括号
    std::uint16_t port = 0;
    std::string path = "/";

    // 解析 URL。失败：InvalidArgument（空/缺 scheme/缺 host/端口非法/非 HTTP 系
    // scheme）。不做百分号解码——原样保留，交由调用方按需处理。
    [[nodiscard]] static Result<Uri> parse(std::string_view text) noexcept;

    // scheme 的有效默认端口（http=80，https=443；其余返回 0）。
    [[nodiscard]] std::uint16_t default_port() const noexcept;
};

// 请求。headers 由调用方给全（Host 与 Connection 由库补，调用方的同名项忽略）。
struct Request {
    std::string method = "GET";
    Uri uri;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

// 响应。headers 保持服务端给出的顺序，重复头不合并。
struct Response {
    std::uint16_t status = 0;
    std::string reason;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // 按名字取响应头（大小写不敏感）；多个同名取第一个；没有返回空 view。
    [[nodiscard]] std::string_view header(std::string_view name) const noexcept;
    // Content-Type 的主体部分（去掉 ';' 后的参数）；没有返回空 view。
    [[nodiscard]] std::string_view content_type() const noexcept;
};

struct HttpOptions {
    SocketOptions socket;                        // 连接/IO 超时
    std::string tls_backend;                     // https 用的 TLS 后端名；空则随便一个已注册的
    std::size_t max_header_bytes = 64u * 1024u;  // 响应头体积上限
    std::size_t max_body_bytes = 16u * 1024u * 1024u;  // 响应体体积上限
    std::uint32_t max_redirects = 5;             // 重定向跟随后跟上限
};

// 执行一次请求-响应（阻塞；内部按 options.max_redirects 跟重定向）。
[[nodiscard]] TERMCHART_API Result<Response> execute(const Request& request,
                                                     const HttpOptions& options = {}) noexcept;

// 便捷入口：GET 一个 URL（等价于填好 method/uri 的 execute）。
[[nodiscard]] TERMCHART_API Result<Response> get(std::string_view url,
                                                 const HttpOptions& options = {}) noexcept;

}  // namespace termchart::network::http

#endif  // TERMCHART_NETWORK_HTTP_HPP
