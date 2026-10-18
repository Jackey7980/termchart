// http_parse.hpp —— HTTP 线格式解析/串行化（**仅 src/network 内可见**）
//
// 存在理由：响应解析与 chunked 解码必须是**纯逻辑**（字节进、结构出），才能在
// 没有真实网络的单测里逐分支咬死；网络 IO / TLS / 重定向留在 http.cpp，经
// ChunkSource 注入。与 platform_internal.hpp 同纪律：不放 include/。
#ifndef TERMCHART_SRC_NETWORK_HTTP_PARSE_HPP
#define TERMCHART_SRC_NETWORK_HTTP_PARSE_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/network/http.hpp"

namespace termchart::network::http {

// chunked 解码器的数据源抽象：http.cpp 用 socket/TLS 实现，测试用内存实现，
// 从而**只有一份** chunked 解码逻辑（大小行解析 / 块尾 CRLF / trailer）。
// read_line 返回不含行尾的一行（空行 = 空字符串）；
// take(n) 恰好取 n 字节。EOF 提前到达时两者都返回 ParseError。
class ChunkSource {
public:
    virtual ~ChunkSource();
    [[nodiscard]] virtual Result<std::string> read_line() noexcept = 0;
    [[nodiscard]] virtual Result<std::string> take(std::size_t n) noexcept = 0;
};

// 串行化请求为线上字节：请求行 + Host + 调用方头 + Connection: close
// （+ 非 GET/HEAD 且带体时的 Content-Length）+ 空行 + 体。
// 调用方 headers 里的 Host / Connection 被忽略（库统一给，避免分叉）。
[[nodiscard]] std::string serialize_request(const Request& request) noexcept;

// 解析**完整**响应字节（状态行 + 头 + 空行 + 完整响应体）。
// 失败：ParseError（结构非法）/ OutOfRange（体超 max_body_bytes）。
[[nodiscard]] Result<Response> parse_response(std::string_view wire,
                                              std::size_t max_body_bytes) noexcept;

// 从 source 流式读一个 chunked 体，返回拼好的明文。
// 失败：ParseError（大小行非法 / 块后缺 CRLF / EOF 截断）/ OutOfRange（单块 > 256MB）。
[[nodiscard]] Result<std::string> decode_chunked(ChunkSource& source) noexcept;

}  // namespace termchart::network::http

#endif  // TERMCHART_SRC_NETWORK_HTTP_PARSE_HPP
