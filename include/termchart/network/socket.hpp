// socket.hpp —— 跨平台 TCP socket 抽象（network 模块，L7）
//
// 平台差异全部落在 src/network/socket_posix.cpp 与 socket_win32.cpp，
// 公开头**没有任何 #ifdef**（与 term/terminal.hpp 同哲学）。
//
// 契约：
//   * 不抛异常：失败返回 Result（IoError / PlatformError / InvalidArgument）；
//   * 阻塞 IO + 超时：connect_timeout_ms / io_timeout_ms 经 setsockopt 实现；
//   * DNS 解析（域名 → 地址）在 connect 内完成，调用方无需自己 getaddrinfo；
//   * 移动语义（PIMPL），拷贝删除——一个 socket 只有一份底层句柄。
#ifndef TERMCHART_NETWORK_SOCKET_HPP
#define TERMCHART_NETWORK_SOCKET_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::network {

struct SocketOptions {
    std::uint32_t connect_timeout_ms = 10000;  // 连接超时
    std::uint32_t io_timeout_ms = 30000;      // 单次读/写超时
};

// 已连接的 TCP socket。默认构造为无效句柄（valid() == false）。
class TcpSocket {
public:
    TcpSocket() noexcept;
    ~TcpSocket();

    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 连接 host:port。host 可以是 IPv4/IPv6 字面量或域名（内部 getaddrinfo）。
    // 失败：InvalidArgument（参数非法）/ IoError（连接失败）/ PlatformError。
    [[nodiscard]] static Result<TcpSocket> connect(std::string_view host,
                                                   std::uint16_t port,
                                                   const SocketOptions& options = {}) noexcept;

    // 读最多 buffer.size() 字节；返回实际读到的字节数（0 = 对端关闭）。
    // 超时返回 IoError。无效句柄返回 InvalidArgument。
    [[nodiscard]] Result<std::size_t> read(std::span<char> buffer) noexcept;

    // 写 data 的全部字节（循环发送直到写完或出错）。
    // 超时返回 IoError。无效句柄返回 InvalidArgument。
    [[nodiscard]] Result<std::size_t> write(std::span<const char> data) noexcept;

    void close() noexcept;

    [[nodiscard]] bool valid() const noexcept;

private:
    struct Impl;
    explicit TcpSocket(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart::network

#endif  // TERMCHART_NETWORK_SOCKET_HPP
