// http.cpp —— HTTP/1.1 客户端实现（连接 + TLS 接驳 + 重定向；解析在 http_parse.cpp）

#include "termchart/network/http.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "src/network/http_parse.hpp"
#include "termchart/network/tls/registry.hpp"

namespace termchart::network::http {
namespace {

constexpr std::size_t kIoChunk = 16u * 1024u;
// chunked 原始流的额外余量：分块大小行与 trailer 的体积不算进 max_body_bytes。
constexpr std::size_t kChunkedSlack = 64u * 1024u;

std::string to_lower(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool is_redirect(std::uint16_t status) noexcept {
    return status == 301 || status == 302 || status == 303 || status == 307 ||
           status == 308;
}

// 301/302/303 按主流实践把方法降为 GET（307/308 严格保持方法与体）。
bool redirect_demotes_to_get(std::uint16_t status) noexcept {
    return status == 301 || status == 302 || status == 303;
}

// 带了读缓冲的连接。TLS 有则记录层走后端，否则直读 socket；写完请求后服务端
// 按 Connection: close 关闭，因此读至 EOF 即响应结束，无需管连接复用。
class Connection final : public ChunkSource {
public:
    Connection(TcpSocket socket, std::unique_ptr<tls::TlsBackend> tls) noexcept
        : socket_(std::move(socket)), tls_(std::move(tls)) {}

    // 只回报成败 → Status（Result<void> 会实例化 Expected<void,E>，踩
    // std::variant<void> 与 void 形参构造，不可用；见 core/expected.hpp）。
    [[nodiscard]] Status write_all(std::string_view data) noexcept {
        if (tls_ != nullptr) {
            const Result<std::size_t> n = tls_->write(socket_, data);
            if (!n.has_value()) {
                return n.error();
            }
            return ErrorCode::Ok;
        }
        const Result<std::size_t> n = socket_.write(data);
        if (!n.has_value()) {
            return n.error();
        }
        return ErrorCode::Ok;
    }

    // ── ChunkSource ────────────────────────────────────────────────────────
    [[nodiscard]] Result<std::string> read_line() noexcept override {
        std::size_t scan = pos_;  // 从消费点起扫（ avail = pending_[pos_:] ）
        for (;;) {
            const std::size_t nl = pending_.find('\n', scan);
            if (nl != std::string::npos) {
                std::size_t end = nl;
                if (end > pos_ && pending_[end - 1] == '\r') {
                    --end;
                }
                std::string line = pending_.substr(pos_, end - pos_);
                consume(nl + 1 - pos_);
                return line;
            }
            if (pending_.size() - pos_ > kMaxLineBytes) {
                return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
            }
            scan = pending_.size() > pos_ ? pending_.size() - 1 : pos_;
            const Result<bool> more = pull();
            if (!more.has_value()) {
                return Unexpected<ErrorCode>(more.error());
            }
            if (!*more) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);  // EOF 前没有完整行
            }
        }
    }

    [[nodiscard]] Result<std::string> take(std::size_t n) noexcept override {
        while (pending_.size() - pos_ < n) {
            const Result<bool> more = pull();
            if (!more.has_value()) {
                return Unexpected<ErrorCode>(more.error());
            }
            if (!*more) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);  // EOF 截断
            }
        }
        std::string out = pending_.substr(pos_, n);
        consume(n);
        return out;
    }

    // 读到 delim 出现为止（delim 计入返回）。超 max_bytes 或 EOF 先到 → 错误。
    [[nodiscard]] Result<std::string> read_until(std::string_view delim,
                                                 std::size_t max_bytes) noexcept {
        std::size_t scan = pos_;
        for (;;) {
            const std::size_t at = pending_.find(delim, scan);
            if (at != std::string::npos) {
                std::string out = pending_.substr(pos_, at + delim.size() - pos_);
                consume(at + delim.size() - pos_);
                return out;
            }
            if (pending_.size() - pos_ > max_bytes) {
                return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
            }
            scan = pending_.size() >= delim.size() ? pending_.size() - delim.size() : pos_;
            const Result<bool> more = pull();
            if (!more.has_value()) {
                return Unexpected<ErrorCode>(more.error());
            }
            if (!*more) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 关闭前没等到 delim
            }
        }
    }

    // 读到 EOF 为止；超 max_bytes → OutOfRange。
    [[nodiscard]] Result<std::string> read_to_eof(std::size_t max_bytes) noexcept {
        for (;;) {
            if (pending_.size() - pos_ > max_bytes) {
                return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
            }
            const Result<bool> more = pull();
            if (!more.has_value()) {
                return Unexpected<ErrorCode>(more.error());
            }
            if (!*more) {
                compact();
                std::string out = std::move(pending_);
                pending_.clear();
                pos_ = 0;
                return out;
            }
        }
    }

private:
    // 大小行/trailer 的合理上限：正常远小于此，超过即视为异常流。
    static constexpr std::size_t kMaxLineBytes = 64u * 1024u;
    // 惰性 compaction 阈值：消费过半且绝对量够大才搬一次，避免每次 take/read_line
    // 都 erase(0,n)（大 body 场景会把累计搬运量退化成 O(n²)）。
    static constexpr std::size_t kCompactMinBytes = 64u * 1024u;

    // 前移消费点；必要时一次性压实缓冲（保持 O(n) 累计搬运）。
    void consume(std::size_t n) noexcept {
        pos_ += n;
        if (pos_ >= kCompactMinBytes && pos_ * 2 >= pending_.size()) {
            compact();
        }
    }

    void compact() noexcept {
        if (pos_ == 0) {
            return;
        }
        pending_.erase(0, pos_);
        pos_ = 0;
    }

    // 拉一批字节进 pending_；false = 对端关闭。网络错误上抛。
    [[nodiscard]] Result<bool> pull() noexcept {
        const Result<std::size_t> n =
            tls_ != nullptr ? tls_->read(socket_, chunk_) : socket_.read(chunk_);
        if (!n.has_value()) {
            return Unexpected<ErrorCode>(n.error());
        }
        if (*n == 0) {
            return false;
        }
        pending_.append(chunk_.data(), *n);
        return true;
    }

    TcpSocket socket_;
    std::unique_ptr<tls::TlsBackend> tls_;
    std::string pending_;
    std::size_t pos_ = 0;  // 已消费偏移（紧凑化前始终 ≤ pending_.size()）
    // 复用读缓冲，免每轮堆分配（= 初始化：避开成员声明的 most-vexing-parse）。
    std::vector<char> chunk_ = std::vector<char>(kIoChunk);
};

// 解析 TLS 后端选择：显式命名缺失 → Unsupported（fail-loud，绝不静默退回明文）；
// 空名字 → 已注册里字典序第一个（唯一"默认"规则，可预测）。
Result<std::unique_ptr<tls::TlsBackend>> resolve_tls_backend(const std::string& name) noexcept {
    const tls::TlsRegistry& registry = tls::TlsRegistry::instance();
    std::unique_ptr<tls::TlsBackend> backend;
    if (!name.empty()) {
        if (!registry.supports(name)) {
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
        }
        backend = registry.create(name);
    } else {
        const std::vector<std::string> names = registry.names();
        if (names.empty()) {
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
        }
        backend = registry.create(names.front());
    }
    if (backend == nullptr) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    return backend;
}

// 建连（按需先 TLS 握手）。hostname 同时用于 SNI 与证书校验。
Result<Connection> open_connection(const Uri& uri, const HttpOptions& options) noexcept {
    const std::uint16_t port = uri.port != 0 ? uri.port : uri.default_port();
    if (port == 0) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    Result<TcpSocket> socket = TcpSocket::connect(uri.host, port, options.socket);
    if (!socket.has_value()) {
        return Unexpected<ErrorCode>(socket.error());
    }
    std::unique_ptr<tls::TlsBackend> tls;
    if (uri.scheme == "https") {
        Result<std::unique_ptr<tls::TlsBackend>> backend =
            resolve_tls_backend(options.tls_backend);
        if (!backend.has_value()) {
            return Unexpected<ErrorCode>(backend.error());
        }
        tls = std::move(*backend);
        const Status hs = tls->handshake(*socket, uri.host);
        if (hs != ErrorCode::Ok) {
            return Unexpected<ErrorCode>(hs);
        }
    }
    return Connection{std::move(*socket), std::move(tls)};
}

// 读一个完整响应：头块 + 按传输规则读体，再整包过一次解析器（单一代码路径）。
Result<Response> read_response(Connection& conn, const Request& request,
                               const HttpOptions& options) noexcept {
    const Result<std::string> head =
        conn.read_until("\r\n\r\n", options.max_header_bytes);
    if (!head.has_value()) {
        return Unexpected<ErrorCode>(head.error());
    }
    // 头块（含结尾空行）先过一次解析：拿 headers 决定体怎么读，此时体为空。
    const Result<Response> parsed = parse_response(*head, options.max_body_bytes);
    if (!parsed.has_value()) {
        return Unexpected<ErrorCode>(parsed.error());
    }
    const bool body_less = request.method == "HEAD" || parsed->status == 204 ||
                           parsed->status == 304;
    std::string body;
    if (!body_less) {
        const std::string_view transfer = parsed->header("Transfer-Encoding");
        const std::string_view length = parsed->header("Content-Length");
        if (to_lower(transfer).find("chunked") != std::string_view::npos) {
            // chunked 原始流（含大小行/trailer）允许略超上界，其余仍守 max。
            const Result<std::string> raw =
                conn.read_to_eof(options.max_body_bytes + kChunkedSlack);
            if (!raw.has_value()) {
                return Unexpected<ErrorCode>(raw.error());
            }
            // 用内存源复用同一份解码器：流式读到的原始 chunked 字节与"一次性
            // 收到"在解码层面无区别（Connection: close 保证 EOF 即流尾）。
            struct MemorySource final : public ChunkSource {
                std::string data;
                std::size_t pos = 0;
                [[nodiscard]] Result<std::string> read_line() noexcept override {
                    const std::size_t nl = data.find('\n', pos);
                    if (nl == std::string::npos) {
                        return Unexpected<ErrorCode>(ErrorCode::ParseError);
                    }
                    std::size_t end = nl;
                    if (end > pos && data[end - 1] == '\r') {
                        --end;
                    }
                    std::string line = data.substr(pos, end - pos);
                    pos = nl + 1;
                    return line;
                }
                [[nodiscard]] Result<std::string> take(std::size_t n) noexcept override {
                    if (data.size() - pos < n) {
                        return Unexpected<ErrorCode>(ErrorCode::ParseError);
                    }
                    std::string out = data.substr(pos, n);
                    pos += n;
                    return out;
                }
            };
            MemorySource source;
            source.data = std::move(*raw);
            const Result<std::string> decoded = decode_chunked(source);
            if (!decoded.has_value()) {
                return Unexpected<ErrorCode>(decoded.error());
            }
            body = std::move(*decoded);
        } else if (!length.empty()) {
            std::size_t want = 0;
            bool digits_only = !length.empty();
            for (const char ch : length) {
                if (ch < '0' || ch > '9') {
                    digits_only = false;
                    break;
                }
                want = want * 10u + static_cast<std::size_t>(ch - '0');
            }
            if (!digits_only || want > options.max_body_bytes) {
                return Unexpected<ErrorCode>(digits_only ? ErrorCode::OutOfRange
                                                         : ErrorCode::ParseError);
            }
            const Result<std::string> taken = conn.take(want);
            if (!taken.has_value()) {
                return Unexpected<ErrorCode>(taken.error());
            }
            body = std::move(*taken);
        } else {
            // 无长度无分块：读到连接关闭为止。
            const Result<std::string> rest = conn.read_to_eof(options.max_body_bytes);
            if (!rest.has_value()) {
                return Unexpected<ErrorCode>(rest.error());
            }
            body = std::move(*rest);
        }
    }
    // 头 + 体整包再过一次解析器：响应结构（状态行/头）的校验只有一条代码路径。
    return parse_response(*head + body, options.max_body_bytes);
}

}  // namespace

Result<Response> execute(const Request& request, const HttpOptions& options) noexcept {
    Request current = request;
    if (current.uri.path.empty()) {
        current.uri.path = "/";
    }
    for (std::uint32_t redirects = 0;; ++redirects) {
        if (current.uri.scheme != "http" && current.uri.scheme != "https") {
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        Result<Connection> conn = open_connection(current.uri, options);
        if (!conn.has_value()) {
            return Unexpected<ErrorCode>(conn.error());
        }
        const Result<std::string> wire = serialize_request(current);
        if (!wire.has_value()) {
            return Unexpected<ErrorCode>(wire.error());
        }
        const Status written = conn->write_all(*wire);
        if (written != ErrorCode::Ok) {
            return Unexpected<ErrorCode>(written);
        }
        Result<Response> response = read_response(*conn, current, options);
        if (!response.has_value()) {
            return Unexpected<ErrorCode>(response.error());
        }
        if (!is_redirect(response->status) ||
            redirects >= options.max_redirects) {
            return response;
        }
        const std::string_view location = response->header("Location");
        if (location.empty()) {
            return response;  // 重定向无 Location：按普通响应上交
        }
        Result<Uri> next = Uri::parse(location);
        if (!next.has_value()) {
            // 相对 Location：拼回当前 uri 的 scheme://host[:port]。
            if (location.front() != '/') {
                return response;  // 既非绝对也非路径开头：不猜，原样上交
            }
            next = current.uri;
            next->path = std::string{location};
        }
        if (redirect_demotes_to_get(response->status)) {
            current.method = "GET";
            current.body.clear();
        }
        current.uri = std::move(*next);
    }
}

Result<Response> get(std::string_view url, const HttpOptions& options) noexcept {
    const Result<Uri> uri = Uri::parse(url);
    if (!uri.has_value()) {
        return Unexpected<ErrorCode>(uri.error());
    }
    Request request;
    request.method = "GET";
    request.uri = *uri;
    return execute(request, options);
}

}  // namespace termchart::network::http
