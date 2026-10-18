// backend.hpp —— TLS 后端抽象接口（network/tls，L7）
//
// 可插拔契约（与 raster/chart/scale 注册表同构，见 docs/design/registry.md）：
//   * 内置后端（network/tls/builtin/，自主实现）与第三方后端（BearSSL/mbedTLS…）
//     实现同一接口，经 TlsRegistry 注册，调用方按名字取用；
//   * 命名项缺失时**显式报错**（fail-loud），不静默降级——请求 "builtin" 而
//     未链接该后端即 Unsupported，绝不偷偷退回明文；
//   * 后端只做"客户端 TLS 记录层"：握手 + 加解密读写 + close_notify。
//     证书链验证由后端负责（hostname 同时用于 SNI 与主机名校验）。
#ifndef TERMCHART_NETWORK_TLS_BACKEND_HPP
#define TERMCHART_NETWORK_TLS_BACKEND_HPP

#include <span>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/network/socket.hpp"

namespace termchart::network::tls {

class TlsBackend {
public:
    // 声明与定义分离：虚表锚点放 src/network/tls/backend.cpp（基类也要有 cpp）。
    virtual ~TlsBackend();

    // 客户端握手：对已连接的 socket 完成 TLS 握手。
    // hostname 用于 SNI 扩展与证书主机名校验。失败返回 IoError/ParseError/
    // Unsupported（证书不受信等，具体语义由后端定义，消息可经 diagnostic 观察）。
    // 只回报成败 → Status（core/expected.hpp 约定，不用 Result<void>：
    // Expected<void,E> 依赖"void 形参构造函数"的方言容忍度，不跨编译器可靠）。
    [[nodiscard]] virtual Status handshake(TcpSocket& socket,
                                          std::string_view hostname) noexcept = 0;

    // 记录层读：解密后最多 buffer.size() 字节明文；0 = 对端关闭。
    [[nodiscard]] virtual Result<std::size_t> read(TcpSocket& socket,
                                                  std::span<char> buffer) noexcept = 0;

    // 记录层写：加密后发送全部字节。
    [[nodiscard]] virtual Result<std::size_t> write(TcpSocket& socket,
                                                   std::span<const char> data) noexcept = 0;

    // 关闭 TLS 会话（best-effort 发 close_notify；不关闭底层 socket）。
    virtual void shutdown(TcpSocket& socket) noexcept = 0;

    // 后端名字（如 "builtin"、"bearssl"），用于注册表键与诊断。
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

}  // namespace termchart::network::tls

#endif  // TERMCHART_NETWORK_TLS_BACKEND_HPP
