// test_proxy.cpp —— network/proxy：解析 / 探测链决策 / 隧道字节（全部离线）
//
// 不联网、不起 socket：代理的纯逻辑（字符串解析、环境变量决策、协议字节
// 构造与响应解析）在这里做确定性推演；真机隧道由示例与调用方环境验证。
#include <doctest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "src/network/proxy_internal.hpp"

namespace {
using termchart::ErrorCode;
using termchart::Result;
using termchart::network::proxy::Config;
using termchart::network::proxy::Kind;
using termchart::network::proxy::build_connect_request;
using termchart::network::proxy::build_socks5_auth;
using termchart::network::proxy::build_socks5_connect;
using termchart::network::proxy::build_socks5_greet;
using termchart::network::proxy::list_matches;
using termchart::network::proxy::parse;
using termchart::network::proxy::parse_connect_status;
using termchart::network::proxy::parse_socks5_auth_reply;
using termchart::network::proxy::parse_socks5_connect_reply;
using termchart::network::proxy::parse_socks5_method_reply;
using termchart::network::proxy::read_env;
using termchart::network::proxy::resolve;

using Env = std::vector<std::pair<std::string, std::string>>;

void put_env(const char* name, const char* value) {
#if defined(_WIN32)
    (void)_putenv_s(name, value);  // value 为空串即删除该变量
#else
    if (value[0] == '\0') {
        (void)unsetenv(name);
    } else {
        (void)setenv(name, value, 1);
    }
#endif
}

class EnvGuard {
public:
    explicit EnvGuard(const char* name) : name_(name) {
        if (const char* old = std::getenv(name)) {
            old_ = old;
            had_ = true;
        }
    }
    ~EnvGuard() { put_env(name_.c_str(), had_ ? old_.c_str() : ""); }
    void set(const char* value) { put_env(name_.c_str(), value); }

private:
    std::string name_;
    std::string old_;
    bool had_ = false;
};

TEST_CASE("proxy::parse：scheme / userinfo / 裸 host:port") {
    const Result<Config> http = parse("http://127.0.0.1:7890");
    REQUIRE(http.has_value());
    CHECK(http->kind == Kind::HttpConnect);
    CHECK(http->host == "127.0.0.1");
    CHECK(http->port == 7890);

    const Result<Config> socks = parse("socks5://127.0.0.1:7891");
    REQUIRE(socks.has_value());
    CHECK(socks->kind == Kind::Socks5);

    const Result<Config> bare = parse("127.0.0.1:7890");  // 裸 host:port 默认 http
    REQUIRE(bare.has_value());
    CHECK(bare->kind == Kind::HttpConnect);

    const Result<Config> auth = parse("socks5://user:pass@10.0.0.1:1080");
    REQUIRE(auth.has_value());
    CHECK(auth->username == "user");
    CHECK(auth->password == "pass");
    CHECK(auth->port == 1080);
}

TEST_CASE("proxy::parse：坏输入 fail-loud") {
    CHECK_FALSE(parse("").has_value());
    CHECK(parse("").error() == ErrorCode::InvalidArgument);
    CHECK_FALSE(parse("127.0.0.1").has_value());          // 无端口
    CHECK(parse("127.0.0.1:abc").error() == ErrorCode::InvalidArgument);
    CHECK(parse("127.0.0.1:99999").error() == ErrorCode::InvalidArgument);
    CHECK(parse("socks4://h:1080").error() == ErrorCode::Unsupported);
    CHECK(parse("ftp://h:21").error() == ErrorCode::Unsupported);
}

TEST_CASE("proxy::resolve：显式优先，其余链路依次") {
    const Config none;  // None = 直连
    Config explicit_cfg;
    REQUIRE(explicit_cfg.kind == Kind::None);

    // 显式 socks5：压倒环境变量与系统代理。
    const Result<Config> ex = parse("socks5://127.0.0.1:7890");
    REQUIRE(ex.has_value());
    const Config chosen = resolve("https", "example.com", *ex, Env{{"HTTPS_PROXY", "http://other:8080"}},
                                  [] {
                                      Config sys;
                                      const Result<Config> p = parse("http://sys:3128");
                                      if (p.has_value()) sys = *p;
                                      return sys;
                                  }());
    CHECK(chosen.kind == Kind::Socks5);
    CHECK(chosen.host == "127.0.0.1");
    CHECK_FALSE(chosen.from_detect);  // 显式：失败时 fail-loud

    // 无显式无环境无系统 → 直连。
    const Config direct = resolve("https", "example.com", none, Env{}, Config{});
    CHECK(direct.kind == Kind::None);
}

TEST_CASE("proxy::resolve：环境变量按目标 scheme 选键（curl 语义）") {
    const Config none;
    const Env env{{"HTTPS_PROXY", "http://https-proxy:8080"},
                  {"HTTP_PROXY", "http://http-proxy:8080"},
                  {"ALL_PROXY", "socks5://all-proxy:1080"}};

    const Config https_cfg = resolve("https", "example.com", none, env, Config{});
    CHECK(https_cfg.kind == Kind::HttpConnect);
    CHECK(https_cfg.host == "https-proxy");  // 不交叉：不用 HTTP_PROXY
    CHECK(https_cfg.from_detect);

    const Config http_cfg = resolve("http", "example.com", none, env, Config{});
    CHECK(http_cfg.host == "http-proxy");

    // 只有 ALL_PROXY 时两者都兜底。
    const Env only_all{{"ALL_PROXY", "socks5://all-proxy:1080"}};
    CHECK(resolve("https", "example.com", none, only_all, Config{}).host == "all-proxy");
    CHECK(resolve("http", "example.com", none, only_all, Config{}).host == "all-proxy");

    // NO_PROXY 命中目标 → 连系统代理也跳过（用户明示不走代理）。
    const Env with_no{{"NO_PROXY", "example.com"}, {"HTTPS_PROXY", "http://p:8080"}};
    Config sys;
    const Result<Config> sp = parse("http://sys:3128");
    REQUIRE(sp.has_value());
    sys = *sp;
    CHECK(resolve("https", "example.com", none, with_no, sys).kind == Kind::None);
    // 未命中的目标仍走代理。
    CHECK(resolve("https", "other.com", none, with_no, sys).kind == Kind::HttpConnect);
}

TEST_CASE("proxy::resolve：系统代理兜底 + 坏环境变量只 warn 继续") {
    const Config none;
    Config sys;
    const Result<Config> sp = parse("http://127.0.0.1:7890");
    REQUIRE(sp.has_value());
    sys = *sp;
    sys.from_detect = true;

    // 环境变量损坏：不 fail-loud，落到系统代理。
    const Env broken{{"HTTPS_PROXY", "not-a-proxy"}};
    const Config cfg = resolve("https", "example.com", none, broken, sys);
    CHECK(cfg.kind == Kind::HttpConnect);
    CHECK(cfg.host == "127.0.0.1");
    CHECK(cfg.from_detect);
}

TEST_CASE("proxy::list_matches：no_proxy / ProxyOverride 口径") {
    CHECK(list_matches("<local>;127.0.0.1", "127.0.0.1"));
    CHECK(list_matches("<local>", "localhost"));           // 无点名字
    CHECK(list_matches("<local>", "intranet"));
    CHECK(list_matches("example.com,.foo.com", "x.foo.com"));  // 点分后缀匹配子域
    CHECK(list_matches("*", "anything.example"));
    CHECK_FALSE(list_matches("example.com", "notexample.com"));  // 后缀边界
    CHECK_FALSE(list_matches("example.com", "example.org"));
}

TEST_CASE("proxy::CONNECT 字节与状态行解析") {
    const Config plain;
    const std::string req = build_connect_request("example.com", 443, plain);
    CHECK(req.starts_with("CONNECT example.com:443 HTTP/1.1\r\n"));
    CHECK(req.find("Host: example.com:443\r\n") != std::string::npos);
    CHECK(req.find("Proxy-Authorization") == std::string::npos);
    CHECK(req.ends_with("\r\n\r\n"));

    // 带凭据：Basic 头在（base64 由 util 负责，只验证存在性与载荷前缀）。
    Config auth;
    auth.username = "u";
    auth.password = "p";
    const std::string req_auth = build_connect_request("h", 443, auth);
    CHECK(req_auth.find("Proxy-Authorization: Basic ") != std::string::npos);

    CHECK(parse_connect_status("HTTP/1.1 200 Connection established") == ErrorCode::Ok);
    CHECK(parse_connect_status("HTTP/1.0 200 OK") == ErrorCode::Ok);
    CHECK(parse_connect_status("HTTP/1.1 407 Proxy Authentication Required") ==
          ErrorCode::Unsupported);
    CHECK(parse_connect_status("HTTP/1.1 502 Bad Gateway") == ErrorCode::IoError);
    CHECK(parse_connect_status("garbage") == ErrorCode::ParseError);
}

TEST_CASE("proxy::SOCKS5 字节与三段响应解析") {
    CHECK(build_socks5_greet(Config{}) == std::string("\x05\x01\x00", 3));
    Config auth;
    auth.username = "u";
    auth.password = "p";
    CHECK(build_socks5_greet(auth) == std::string("\x05\x02\x00\x02", 4));

    const std::string auth_bytes = build_socks5_auth(auth);
    // 01 VER | 01 ulen | 'u' | 01 plen | 'p'（5 字节；\x01p 转义只吃 01）
    CHECK(auth_bytes == std::string("\x01\x01u\x01p", 5));
    CHECK(auth_bytes.size() == 5);

    const std::string conn = build_socks5_connect("example.com", 443);
    CHECK(conn.substr(0, 5) == std::string("\x05\x01\x00\x03\x0b", 5));
    CHECK(conn.substr(5, 11) == "example.com");
    CHECK(static_cast<std::uint8_t>(conn[16]) == 0x01);  // 443 = 0x01BB
    CHECK(static_cast<std::uint8_t>(conn[17]) == 0xBB);

    // 方法协商：00 无认证 / 02 要认证 / FF 无共识。
    CHECK(parse_socks5_method_reply(std::string("\x05\x00", 2)) == ErrorCode::Ok);
    CHECK(parse_socks5_method_reply(std::string("\x05\x02", 2)) == ErrorCode::Ok);
    CHECK(parse_socks5_method_reply(std::string("\x05\xff", 2)) ==
          ErrorCode::Unsupported);
    // 认证：status 00 成功。
    CHECK(parse_socks5_auth_reply(std::string("\x01\x00", 2)) == ErrorCode::Ok);
    CHECK(parse_socks5_auth_reply(std::string("\x01\x01", 2)) == ErrorCode::IoError);
    // CONNECT：rep 00 成功；02 规则不允许（Unsupported）；05 被拒（IoError）。
    CHECK(parse_socks5_connect_reply(std::string("\x05\x00\x00\x01", 4)) == ErrorCode::Ok);
    CHECK(parse_socks5_connect_reply(std::string("\x05\x02\x00\x01", 4)) ==
          ErrorCode::Unsupported);
    CHECK(parse_socks5_connect_reply(std::string("\x05\x05\x00\x01", 4)) ==
          ErrorCode::IoError);
    CHECK(parse_socks5_connect_reply(std::string("\x04\x00", 2)) == ErrorCode::ParseError);
}

TEST_CASE("proxy::read_env：形状（不依赖具体环境有无代理）") {
    // 只验契约：返回 vector<pair>，每个键非空、值非空；不预设机器环境。
    const auto env = read_env();
    for (const auto& kv : env) {
        CHECK_FALSE(kv.first.empty());
        CHECK_FALSE(kv.second.empty());
    }
}

TEST_CASE("proxy::read_env：必须收 NO_PROXY（resolve 的绕过判定依赖它）") {
    EnvGuard upper("NO_PROXY");
    EnvGuard lower("no_proxy");
    upper.set("example.invalid");
    lower.set("example.invalid");

    const Env env = read_env();
    const auto has = [&env](std::string_view key) {
        return std::any_of(env.begin(), env.end(),
                           [key](const auto& kv) { return kv.first == key; });
    };
    CHECK(has("NO_PROXY"));
    CHECK(has("no_proxy"));

    // 端到端判定：绕过表中命中的 host 必须直连，即使系统代理已配。
    const auto sys = parse("http://sys-proxy:3128");
    REQUIRE(sys.has_value());
    CHECK(resolve("http", "example.invalid", Config{}, env, *sys).kind == Kind::None);
    CHECK(resolve("http", "other.invalid", Config{}, env, *sys).kind == Kind::HttpConnect);
}

}  // namespace
