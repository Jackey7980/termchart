// http_parse.cpp —— HTTP 线格式解析/串行化实现（无 IO；IO 经 ChunkSource 注入）

#include "src/network/http_parse.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace termchart::network::http {
namespace {

constexpr std::string_view kCrlf = "\r\n";
constexpr std::size_t kMaxChunkSize = std::size_t{1} << 28;  // 单块 256MB 上限

std::string to_lower(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

// 从缓冲切一行（不含行尾），consumed 前移；行尾容忍 \r\n 与裸 \n。
// 没有行尾（缓冲耗尽）时返回 false。
bool next_line(std::string_view& in, std::string_view& line) noexcept {
    const std::size_t nl = in.find('\n');
    if (nl == std::string_view::npos) {
        return false;
    }
    line = in.substr(0, nl);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    in.remove_prefix(nl + 1);
    return true;
}

bool is_header_controlled(std::string_view name) noexcept {
    // Host 与 Connection 由库统一给；调用方的同名项忽略（见 serialize_request）。
    const std::string lower = to_lower(name);
    return lower == "host" || lower == "connection";
}

std::size_t hex_digits_value(std::string_view digits, bool& ok) noexcept {
    std::size_t value = 0;
    for (const char ch : digits) {
        std::uint32_t digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = static_cast<std::uint32_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = static_cast<std::uint32_t>(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            digit = static_cast<std::uint32_t>(ch - 'A' + 10);
        } else {
            ok = false;
            return 0;
        }
        value = value * 16u + digit;
        if (value > kMaxChunkSize) {
            ok = false;
            return 0;
        }
    }
    return value;
}

}  // namespace

ChunkSource::~ChunkSource() = default;

// ── Uri ─────────────────────────────────────────────────────────────────────

Result<Uri> Uri::parse(std::string_view text) noexcept {
    Uri uri;
    const std::size_t scheme_end = text.find("://");
    if (scheme_end == std::string_view::npos) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    uri.scheme = to_lower(text.substr(0, scheme_end));
    if (uri.scheme != "http" && uri.scheme != "https") {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::string_view rest = text.substr(scheme_end + 3);
    if (rest.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    // userinfo@host：本库不做代理认证，显式拒绝而非静默丢弃凭证。
    if (rest.find('@') != std::string_view::npos) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const std::size_t authority_end = rest.find('/');
    const std::string_view authority =
        authority_end == std::string_view::npos ? rest : rest.substr(0, authority_end);
    uri.path = authority_end == std::string_view::npos
                   ? std::string{"/"}
                   : std::string{rest.substr(authority_end)};
    if (authority.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }

    // 端口：host 或 [v6] 之后切（IPv6 字面量含多个冒号，不能按第一个冒号分）。
    std::string_view host_part = authority;
    std::string_view port_part;
    if (authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos) {
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') {
                return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
            }
            port_part = authority.substr(close + 2);
        }
        // host 含方括号原样保留（getaddrinfo 接受 "::1"；带括号版由平台层处理）。
        host_part = authority.substr(1, close - 1);
    } else {
        const std::size_t colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            host_part = authority.substr(0, colon);
            port_part = authority.substr(colon + 1);
        }
    }
    uri.host = to_lower(host_part);
    if (uri.host.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    if (!port_part.empty()) {
        std::uint32_t port = 0;
        for (const char ch : port_part) {
            if (ch < '0' || ch > '9') {
                return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
            }
            port = port * 10u + static_cast<std::uint32_t>(ch - '0');
            if (port > 65535u) {
                return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
            }
        }
        uri.port = static_cast<std::uint16_t>(port);
    }
    return uri;
}

std::uint16_t Uri::default_port() const noexcept {
    if (scheme == "https") {
        return 443;
    }
    if (scheme == "http") {
        return 80;
    }
    return 0;
}

// ── Response 访问器 ──────────────────────────────────────────────────────────

std::string_view Response::header(std::string_view name) const noexcept {
    const std::string lower = to_lower(name);
    for (const auto& [key, value] : headers) {
        if (to_lower(key) == lower) {
            return value;
        }
    }
    return {};
}

std::string_view Response::content_type() const noexcept {
    const std::string_view raw = header("Content-Type");
    if (raw.empty()) {
        return {};
    }
    const std::size_t semicolon = raw.find(';');
    const std::string_view type =
        semicolon == std::string_view::npos ? raw : raw.substr(0, semicolon);
    // 去首尾空白（token 前后允许 LWS）。
    std::size_t begin = 0;
    std::size_t end = type.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(type[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(type[end - 1])) != 0) {
        --end;
    }
    return type.substr(begin, end - begin);
}

// ── 串行化 ──────────────────────────────────────────────────────────────────

std::string serialize_request(const Request& request) noexcept {
    std::string out;
    out.reserve(128 + request.body.size());
    out += request.method;
    out += ' ';
    out += request.uri.path;
    out += " HTTP/1.1";
    out += kCrlf;

    const std::uint16_t port =
        request.uri.port != 0 ? request.uri.port : request.uri.default_port();
    const bool is_default = port != 0 && port == request.uri.default_port();
    out += "Host: ";
    out += request.uri.host;
    if (!is_default) {
        out += ':';
        out += std::to_string(port);
    }
    out += kCrlf;

    for (const auto& [key, value] : request.headers) {
        if (is_header_controlled(key)) {
            continue;
        }
        out += key;
        out += ": ";
        out += value;
        out += kCrlf;
    }
    out += "Connection: close";
    out += kCrlf;
    const bool has_body = !request.body.empty() ||
                          (request.method != "GET" && request.method != "HEAD");
    if (has_body) {
        out += "Content-Length: ";
        out += std::to_string(request.body.size());
        out += kCrlf;
    }
    out += kCrlf;
    if (!request.body.empty()) {
        out += request.body;
    }
    return out;
}

// ── 解析 ────────────────────────────────────────────────────────────────────

Result<Response> parse_response(std::string_view wire, std::size_t max_body_bytes) noexcept {
    Response response;
    std::string_view rest = wire;

    std::string_view status_line;
    if (!next_line(rest, status_line) || status_line.size() < 12 ||
        status_line.compare(0, 5, "HTTP/") != 0) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    // "HTTP/1.x SSS reason"（reason 可空，但其前的 SP 必须在）。
    const std::size_t first_space = status_line.find(' ');
    const std::size_t second_space = status_line.find(' ', first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    const std::string_view code =
        status_line.substr(first_space + 1, second_space - first_space - 1);
    if (code.size() != 3) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    std::uint32_t status = 0;
    for (const char ch : code) {
        if (ch < '0' || ch > '9') {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        status = status * 10u + static_cast<std::uint32_t>(ch - '0');
    }
    response.status = static_cast<std::uint16_t>(status);
    response.reason = std::string{status_line.substr(second_space + 1)};

    for (;;) {
        std::string_view line;
        if (!next_line(rest, line)) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 头部未闭合
        }
        if (line.empty()) {
            break;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        std::string_view name = line.substr(0, colon);
        std::string_view value = line.substr(colon + 1);
        std::size_t begin = 0;  // 去掉值的前导 LWS（"Name:  value" 合法）
        while (begin < value.size() &&
               std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
            ++begin;
        }
        value.remove_prefix(begin);
        response.headers.emplace_back(std::string{name}, std::string{value});
    }

    if (rest.size() > max_body_bytes) {
        return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
    }
    response.body = std::string{rest};
    return response;
}

Result<std::string> decode_chunked(ChunkSource& source) noexcept {
    std::string out;
    for (;;) {
        const Result<std::string> line = source.read_line();
        if (!line.has_value()) {
            return Unexpected<ErrorCode>(line.error());
        }
        // 大小行形如 "1f" 或 "1f;ext=value"（扩展部分按规范忽略）。
        const std::size_t semicolon = line->find(';');
        const std::string_view digits =
            semicolon == std::string_view::npos ? *line : std::string_view{*line}.substr(0, semicolon);
        if (digits.empty()) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        bool ok = true;
        const std::size_t chunk_size = hex_digits_value(digits, ok);
        if (!ok) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        if (chunk_size == 0) {
            // trailer：读到空行为止（宽容对待缺失的结尾空行）。
            for (;;) {
                const Result<std::string> trailer = source.read_line();
                if (!trailer.has_value()) {
                    break;  // EOF：收
                }
                if (trailer->empty()) {
                    break;
                }
            }
            return out;
        }
        const Result<std::string> chunk = source.take(chunk_size + 2);  // 数据 + CRLF
        if (!chunk.has_value()) {
            return Unexpected<ErrorCode>(chunk.error());
        }
        if (chunk->size() < chunk_size + 2 || chunk->substr(chunk_size, 2) != kCrlf) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        out.append(*chunk, 0, chunk_size);
    }
}

}  // namespace termchart::network::http
