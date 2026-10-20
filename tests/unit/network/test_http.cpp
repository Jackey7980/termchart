// test_http.cpp —— network/http：URI / 串行化 / 响应解析 / chunked 解码
//
// 全部离线（不发真实请求）。有副意的路径（connect/TLS/重定向循环）只测
// fail-loud 错误码，不依赖任何外部主机。
#include <doctest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "src/network/http_parse.hpp"
#include "termchart/network/http.hpp"
#include "termchart/network/tls/backend.hpp"
#include "termchart/network/tls/registry.hpp"

namespace {

using namespace termchart;
using namespace termchart::network;
using termchart::network::http::ChunkSource;
using termchart::network::http::HttpOptions;
using termchart::network::http::Request;
using termchart::network::http::Response;
using termchart::network::http::Uri;

// 只测注册表 plumbing 的最小 TlsBackend（不做真握手）。
class MemoryBackendStub final : public tls::TlsBackend {
public:
    [[nodiscard]] Status handshake(TcpSocket&, std::string_view) noexcept override {
        return ErrorCode::Ok;
    }
    [[nodiscard]] Result<std::size_t> read(TcpSocket&, std::span<char>) noexcept override {
        return std::size_t{0};
    }
    [[nodiscard]] Result<std::size_t> write(TcpSocket&,
                                            std::span<const char>) noexcept override {
        return std::size_t{0};
    }
    void shutdown(TcpSocket&) noexcept override {}
    [[nodiscard]] std::string_view name() const noexcept override { return "stub"; }
};

// 内存版 ChunkSource：把"流式解码"在单测里变成确定性字节推演。
class MemorySource final : public ChunkSource {
public:
    explicit MemorySource(std::string data) noexcept : data_(std::move(data)) {}

    [[nodiscard]] Result<std::string> read_line() noexcept override {
        const std::size_t nl = data_.find('\n', pos_);
        if (nl == std::string::npos) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        std::size_t end = nl;
        if (end > pos_ && data_[end - 1] == '\r') {
            --end;
        }
        std::string line = data_.substr(pos_, end - pos_);
        pos_ = nl + 1;
        return line;
    }

    [[nodiscard]] Result<std::string> take(std::size_t n) noexcept override {
        if (data_.size() - pos_ < n) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        std::string out = data_.substr(pos_, n);
        pos_ += n;
        return out;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }

private:
    std::string data_;
    std::size_t pos_ = 0;
};

TEST_CASE("network/http: Uri 解析") {
    SUBCASE("常规 URL：默认端口 / 路径 / 查询串") {
        const Result<Uri> u = Uri::parse("http://example.com/a/b?x=1&y=2");
        REQUIRE(u.has_value());
        CHECK(u->scheme == "http");
        CHECK(u->host == "example.com");
        CHECK(u->port == 0);  // 端口缺省按 scheme 取（不预先填死）
        CHECK(u->path == "/a/b?x=1&y=2");

        const Result<Uri> s = Uri::parse("https://Example.COM");
        REQUIRE(s.has_value());
        CHECK(s->scheme == "https");   // scheme 小写
        CHECK(s->host == "example.com");  // host 小写
        CHECK(s->path == "/");         // 缺省根路径
        CHECK(s->default_port() == 443);
    }

    SUBCASE("显式端口（默认端口与显式同值都保留信息）") {
        const Result<Uri> u = Uri::parse("http://h:8080/p");
        REQUIRE(u.has_value());
        CHECK(u->port == 8080);
        CHECK(u->path == "/p");
        CHECK(Uri::parse("https://h:443/")->port == 443);
    }

    SUBCASE("IPv6 字面量") {
        const Result<Uri> u = Uri::parse("http://[::1]:9000/x");
        REQUIRE(u.has_value());
        CHECK(u->host == "::1");
        CHECK(u->port == 9000);
    }

    SUBCASE("非法输入返回 InvalidArgument") {
        CHECK_FALSE(Uri::parse("").has_value());
        CHECK_FALSE(Uri::parse("example.com/x").has_value());       // 缺 scheme
        CHECK_FALSE(Uri::parse("ftp://example.com").has_value());   // 非 HTTP 系
        CHECK_FALSE(Uri::parse("http://").has_value());              // 缺 host
        CHECK_FALSE(Uri::parse("http://host:abc/").has_value());     // 端口非数字
        CHECK_FALSE(Uri::parse("http://host:99999/").has_value());   // 端口越界
        CHECK_FALSE(Uri::parse("http://user@host/").has_value());    // userinfo 不支持
        CHECK_FALSE(Uri::parse("http://[::1/x").has_value());        // IPv6 未闭合
        CHECK_FALSE(Uri::parse("http://h.com/a\rb").has_value());    // path 控制字符（注入面）
        // 注意：字符串字面量里的 \0 会被 string_view 的 const char* 构造按 strlen
        // 截断（C++ 标准行为），要测"显式含 NUL 的 view"必须走 std::string。
        const std::string nul_path = std::string{"http://h.com/a"} + '\0' + 'b';
        CHECK_FALSE(Uri::parse(std::string_view{nul_path}).has_value());
        CHECK(Uri::parse("").error() == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("network/http: 请求串行化") {
    Request request;
    request.method = "POST";
    request.uri = *Uri::parse("http://example.com/submit");
    request.headers = {{"X-Token", "abc"}, {"Host", "evil.example"},
                       {"Connection", "keep-alive"}};  // 后两项应被忽略
    request.body = "k=v";

    const Result<std::string> wire = termchart::network::http::serialize_request(request);
    REQUIRE(wire.has_value());
    CHECK(wire->starts_with("POST /submit HTTP/1.1\r\n"));
    CHECK(wire->find("Host: example.com\r\n") != std::string::npos);
    CHECK(wire->find("X-Token: abc\r\n") != std::string::npos);
    CHECK(wire->find("Connection: close\r\n") != std::string::npos);
    CHECK(wire->find("keep-alive") == std::string::npos);
    CHECK(wire->find("evil.example") == std::string::npos);
    CHECK(wire->find("Content-Length: 3\r\n") != std::string::npos);
    CHECK(wire->ends_with("\r\n\r\nk=v"));

    SUBCASE("GET 无体不带 Content-Length") {
        Request get;
        get.uri = *Uri::parse("http://h.com/");
        const Result<std::string> out = termchart::network::http::serialize_request(get);
        REQUIRE(out.has_value());
        CHECK(out->find("Content-Length") == std::string::npos);
    }

    SUBCASE("头注入拒绝：CR/LF/NUL 一律 InvalidArgument") {
        Request attack;
        attack.uri = *Uri::parse("http://h.com/");
        attack.headers = {{"X-Evil", "v\r\nX-Injected: 1"}};
        CHECK_FALSE(termchart::network::http::serialize_request(attack).has_value());

        Request name_attack;
        name_attack.uri = *Uri::parse("http://h.com/");
        name_attack.headers = {{"X-Bad\nName", "v"}};
        const Result<std::string> out = termchart::network::http::serialize_request(name_attack);
        CHECK_FALSE(out.has_value());
        CHECK(out.error() == ErrorCode::InvalidArgument);

        // 显式含 NUL 的头名（std::string → string_view 保留 NUL，不走 strlen）。
        const std::string nul_name = std::string{"X-B"} + '\0';
        Request nul_header;
        nul_header.uri = *Uri::parse("http://h.com/");
        nul_header.headers.push_back({std::string{nul_name}, std::string{"v"}});
        CHECK_FALSE(termchart::network::http::serialize_request(nul_header).has_value());
    }

    SUBCASE("body 二进制安全：NUL 原样透传且 Content-Length 定长") {
        Request bin;
        bin.uri = *Uri::parse("http://h.com/");
        bin.method = "POST";
        bin.body = std::string{"a\0b", 3};  // 3 字节，含 NUL
        const Result<std::string> out = termchart::network::http::serialize_request(bin);
        REQUIRE(out.has_value());
        CHECK(out->find("Content-Length: 3\r\n") != std::string::npos);
        CHECK(out->size() >= 3);
        CHECK(out->substr(out->size() - 3) == std::string{"a\0b", 3});
    }
}

TEST_CASE("network/http: 响应解析") {
    SUBCASE("状态行 + 头 + 体") {
        const Result<Response> r = termchart::network::http::parse_response(
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nX-K: v\r\n\r\nhello", 1024);
        REQUIRE(r.has_value());
        CHECK(r->status == 200);
        CHECK(r->reason == "OK");
        CHECK(r->header("content-type") == "text/plain");  // 大小写不敏感
        CHECK(r->header("X-K") == "v");
        CHECK(r->header("missing").empty());
        CHECK(r->content_type() == "text/plain");
        CHECK(r->body == "hello");
    }

    SUBCASE("content_type 去参数与空白") {
        const Result<Response> r = termchart::network::http::parse_response(
            "HTTP/1.1 200 OK\r\nContent-Type:  text/html ; charset=utf-8 \r\n\r\n", 1024);
        REQUIRE(r.has_value());
        CHECK(r->content_type() == "text/html");
    }

    SUBCASE("空体 / 空 reason") {
        const Result<Response> r = termchart::network::http::parse_response(
            "HTTP/1.1 204 No Content\r\n\r\n", 1024);
        REQUIRE(r.has_value());
        CHECK(r->status == 204);
        CHECK(r->body.empty());
    }

    SUBCASE("结构非法 → ParseError") {
        CHECK_FALSE(termchart::network::http::parse_response("garbage\r\n\r\n", 1024).has_value());
        CHECK_FALSE(termchart::network::http::parse_response("HTTP/1.1 20 x\r\n\r\n", 1024).has_value());
        // 头未闭合（没有空行）
        CHECK_FALSE(termchart::network::http::parse_response("HTTP/1.1 200 OK\r\nA: b\r\n", 1024)
                         .has_value());
        // 头行缺冒号
        CHECK_FALSE(termchart::network::http::parse_response("HTTP/1.1 200 OK\r\nbad\r\n\r\n", 1024)
                         .has_value());
    }

    SUBCASE("体超限 → OutOfRange") {
        const Result<Response> r = termchart::network::http::parse_response(
            "HTTP/1.1 200 OK\r\n\r\n0123456789", 4);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::OutOfRange);
    }
}

TEST_CASE("network/http: chunked 解码") {
    auto decode = [](std::string raw) {
        MemorySource source(std::move(raw));
        return termchart::network::http::decode_chunked(source);
    };

    SUBCASE("标准分块 + 扩展 + trailer") {
        const Result<std::string> r = decode(
            "5;ext=1\r\nhello\r\n6\r\n world\r\n0\r\nX-T: 1\r\n\r\n");
        REQUIRE(r.has_value());
        CHECK(*r == "hello world");
    }

    SUBCASE("空体（首个 0 块）") {
        const Result<std::string> r = decode("0\r\n\r\n");
        REQUIRE(r.has_value());
        CHECK(r->empty());
    }

    SUBCASE("大小写混合的十六进制") {
        const Result<std::string> r = decode("aB\r\n" + std::string(0xAB, 'x') + "\r\n0\r\n\r\n");
        REQUIRE(r.has_value());
        CHECK(r->size() == 171);
    }

    SUBCASE("坏输入 → ParseError") {
        CHECK_FALSE(decode("zz\r\n").has_value());          // 非十六进制
        CHECK_FALSE(decode("5\r\nhel").has_value());        // 块被截断
        CHECK_FALSE(decode("5\r\nhelloXX").has_value());    // 块后缺 CRLF
        CHECK_FALSE(decode("").has_value());                // 连大小行都没有
    }
}

TEST_CASE("network/http: execute 的 fail-loud 路径（离线）") {
    const HttpOptions options;

    SUBCASE("空 URL → InvalidArgument") {
        const Result<Response> r = termchart::network::http::get("", options);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::InvalidArgument);
    }

    SUBCASE("TLS 后端名字未登记 → Unsupported（绝不静默退回明文）") {
        HttpOptions named;
        named.tls_backend = "no-such-backend";
        const Result<Response> r = termchart::network::http::get("https://example.com/", named);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::Unsupported);
    }

#if !defined(_WIN32)
    // POSIX 未接 OpenSSL 后端时，空名字解析不到后端 → Unsupported。
    // （Windows 上 schannel 在静态初始化期已注册，必然有后端。）
    SUBCASE("无任何已注册后端时 https → Unsupported") {
        const Result<Response> r = termchart::network::http::get("https://example.com/", options);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::Unsupported);
    }
#endif

    SUBCASE("明文连不上 → IoError（127.0.0.1 保留端口）") {
        const Result<Response> r = termchart::network::http::get("http://127.0.0.1:1/", options);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::IoError);
    }
}

TEST_CASE("network/http: 真实 TLS 握手（外网；默认跳过，TERMCHART_NETWORK_TESTS=1 时启用）") {
#if defined(_WIN32)
    const char* gate = std::getenv("TERMCHART_NETWORK_TESTS");
    if (gate == nullptr || std::string{gate} != "1") {
        return;  // CI / 沙箱无外网：默认跳过
    }
    HttpOptions options;  // 收紧超时：真网用例不应把套件拖到分钟级
    options.socket.connect_timeout_ms = 5000;
    options.socket.io_timeout_ms = 8000;

    SUBCASE("example.com：有效链 + 主机名匹配 → 200") {
        const Result<Response> r = termchart::network::http::get("https://example.com", options);
        REQUIRE(r.has_value());
        CHECK(r->status == 200);
        CHECK(r->content_type() == "text/html");
        CHECK(r->body.find("Example Domain") != std::string::npos);
    }

    SUBCASE("badssl 自签证书 → Unsupported（握手成功但证书验证拒绝）") {
        const Result<Response> r =
            termchart::network::http::get("https://self-signed.badssl.com", options);
        CHECK_FALSE(r.has_value());
        if (!r.has_value()) {
            CHECK(r.error() == ErrorCode::Unsupported);
        }
    }

    SUBCASE("badssl 主机名不匹配 → Unsupported") {
        const Result<Response> r =
            termchart::network::http::get("https://wrong.host.badssl.com", options);
        CHECK_FALSE(r.has_value());
        if (!r.has_value()) {
            CHECK(r.error() == ErrorCode::Unsupported);
        }
    }

    SUBCASE("重定向跟随：公共重定向服务落地后取 200（服务不可用时容忍失败）") {
        const Result<Response> r =
            termchart::network::http::get("http://httpstat.us/302", options);
        if (r.has_value()) {
            CHECK(r->status == 200);  // 跟过 302 落地
        }
    }
#endif
}

TEST_CASE("network/tls: 注册表 names（默认后端规则的依据）") {
    tls::TlsRegistry& registry = tls::TlsRegistry::instance();
    registry.register_backend("zzz-test-backend", [] {
        return std::make_unique<MemoryBackendStub>();
    });
    const std::vector<std::string> names = registry.names();
    CHECK(std::find(names.begin(), names.end(), "zzz-test-backend") != names.end());
}

}  // namespace
