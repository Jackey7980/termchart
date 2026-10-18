// platform_internal.hpp —— 网络平台层内部接口（**仅 src/network 内可见**）
//
// 存在理由：network/ 的头文件必须保持"零 #ifdef"（与 term/terminal.hpp 同哲学），
// 于是平台差异（Winsock2 / POSIX sockets）只能封在 .cpp；而这些 .cpp 之间需要一个
// 共享的、不导出的签名表。这是 src/ 里不对应 include/ 同名头的文件——故意的例外，
// 与 src/term/platform_internal.hpp 同纪律。不要把它移到 include/ 下。
#ifndef TERMCHART_SRC_NETWORK_PLATFORM_INTERNAL_HPP
#define TERMCHART_SRC_NETWORK_PLATFORM_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/network/socket.hpp"

namespace termchart::network::platform {

// 平台句柄：POSIX 是 int fd，Windows 是 SOCKET（UINT_PTR），统一收窄为
// intptr_t；两边的"无效"值（-1 / INVALID_SOCKET）都归一成 kInvalidHandle。
using Handle = std::intptr_t;
constexpr Handle kInvalidHandle = -1;

// 连接 host:port（内部 getaddrinfo，IPv4/IPv6 均可）。失败：InvalidArgument /
// IoError（连接失败/DNS 失败）/ PlatformError。
[[nodiscard]] Result<Handle> connect(std::string_view host, std::uint16_t port,
                                     const SocketOptions& options) noexcept;

// 读最多 buffer.size() 字节；0 = 对端关闭。超时返回 IoError。
[[nodiscard]] Result<std::size_t> read(Handle fd, std::span<char> buffer) noexcept;

// 写全部字节（循环发送直到写完或出错）。超时返回 IoError。
[[nodiscard]] Result<std::size_t> write(Handle fd, std::span<const char> data) noexcept;

// 关闭句柄（幂等）。调用后句柄不得再用。
void close(Handle fd) noexcept;

}  // namespace termchart::network::platform

#endif  // TERMCHART_SRC_NETWORK_PLATFORM_INTERNAL_HPP
