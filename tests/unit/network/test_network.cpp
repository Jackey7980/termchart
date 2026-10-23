// test_network.cpp —— network 模块：TcpSocket 契约 + TLS 注册表
//
// 关注的不是"能不能联网"（CI/沙箱里没有外网），而是**契约**：
//   * 非法输入返回错误码，不抛异常、不死循环；
//   * 无效句柄的读写返回 InvalidArgument；
//   * 注册表：登记 → supports/create，未登记 → nullptr（fail-loud 的源头）；
//   * 对 127.0.0.1 的保留端口连接：本机无监听时应失败（IoError），
//     不依赖任何外部主机（个别环境若真占用了该端口，退化为接受两种结果）。
#include <doctest.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "termchart/network/socket.hpp"
#include "termchart/network/tls/backend.hpp"
#include "termchart/network/tls/registry.hpp"
#include "termchart/network/tls/schannel.hpp"

namespace {

using namespace termchart;
using namespace termchart::network;

// 记录生命周期的最小 TlsBackend 假实现（只验注册表 plumbing，不做真握手）。
class FakeBackend final : public tls::TlsBackend {
public:
    explicit FakeBackend(int tag = 0) noexcept : tag_(tag) {}

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
    [[nodiscard]] std::string_view name() const noexcept override { return "fake"; }

    [[nodiscard]] int tag() const noexcept { return tag_; }

private:
    int tag_;
};

TEST_CASE("network: TcpSocket 无效句柄契约") {
    TcpSocket socket;
    CHECK_FALSE(socket.valid());

    std::vector<char> buffer(16, '\0');
    const Result<std::size_t> r = socket.read(buffer);
    CHECK_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::InvalidArgument);

    const char data[] = "x";
    const Result<std::size_t> w = socket.write(data);
    CHECK_FALSE(w.has_value());
    CHECK(w.error() == ErrorCode::InvalidArgument);

    socket.close();  // 幂等，不崩
    CHECK_FALSE(socket.valid());
}

TEST_CASE("network: connect 参数校验") {
    const Result<TcpSocket> empty_host = TcpSocket::connect("", 80);
    CHECK_FALSE(empty_host.has_value());
    CHECK(empty_host.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("network: connect 127.0.0.1 保留端口（无外网依赖）") {
    // 1/tcp 几乎不可能有本地监听：期望 IoError。极个别环境占用该端口时
    // 接受 Ok（连上后立即关闭），不断言具体成功路径。
    Result<TcpSocket> r = TcpSocket::connect("127.0.0.1", 1);
    if (r.has_value()) {
        CHECK(r->valid());
        r->close();
    } else {
        CHECK(r.error() == ErrorCode::IoError);
    }
}

TEST_CASE("network: TcpSocket 移动语义") {
    TcpSocket a;
    TcpSocket b = std::move(a);  // NOLINT：测试移动语义
    CHECK_FALSE(b.valid());
    TcpSocket c;
    c = std::move(b);  // NOLINT：测试移动赋值
    CHECK_FALSE(c.valid());
}

TEST_CASE("network/tls: 注册表登记与查询") {
    tls::TlsRegistry& registry = tls::TlsRegistry::instance();

    CHECK_FALSE(registry.supports("unit-test-fake"));
    registry.register_backend("unit-test-fake", [] {
        return std::make_unique<FakeBackend>(42);
    });
    CHECK(registry.supports("unit-test-fake"));

    std::unique_ptr<tls::TlsBackend> backend = registry.create("unit-test-fake");
    REQUIRE(backend != nullptr);
    CHECK(backend->name() == "fake");
    auto* fake = dynamic_cast<FakeBackend*>(backend.get());
    REQUIRE(fake != nullptr);
    CHECK(fake->tag() == 42);
}

TEST_CASE("network/tls: 枚举三件套 names/size/empty") {
    tls::TlsRegistry& registry = tls::TlsRegistry::instance();
    registry.register_backend("unit-test-enum", [] {
        return std::make_unique<FakeBackend>(7);
    });

    const std::vector<std::string> names = registry.names();
    CHECK(std::is_sorted(names.begin(), names.end()));  // map 字典序
    CHECK(std::find(names.begin(), names.end(), "unit-test-enum") != names.end());
    CHECK(registry.size() == names.size());
    CHECK_FALSE(registry.empty());
    CHECK(registry.supports("unit-test-enum"));
}

TEST_CASE("network/tls: 未登记名字返回 nullptr（fail-loud 源头）") {
    const tls::TlsRegistry& registry = tls::TlsRegistry::instance();
    CHECK_FALSE(registry.supports("definitely-not-registered"));
    CHECK(registry.create("definitely-not-registered") == nullptr);
}

// 回归：SChannel 客户端凭据（2026-10-21 修复的 MinGW dwVersion 坑）。
//
// 根因：MinGW-w64 的 schannel.h 结构体已是 V4，但 SCH_CRED_VERSION 宏停留在
// 0x2；SChannel 对 dwVersion 精确匹配，传 2 一律 SEC_E_UNKNOWN_CREDENTIALS，
// 所有 https 握手失败。曾因现象被误判为"沙箱策略限制"。
// 本用例不联网（AcquireCredentialsHandle 只建本地凭据句柄），Windows CI
// （msys2 MinGW，同一编译器家族）会真实验证修复；POSIX 上 schannel 为 no-op
// 编译，credentials_available 恒 false。
TEST_CASE("network/tls: schannel 凭据可用（dwVersion 回归，无外网依赖）") {
#if defined(_WIN32)
    CHECK(tls::schannel::credentials_available());
#else
    CHECK_FALSE(tls::schannel::credentials_available());
#endif
}

}  // namespace
