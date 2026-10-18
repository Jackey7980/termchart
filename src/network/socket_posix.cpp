// socket_posix.cpp —— TcpSocket 的 POSIX 平台实现（Linux/macOS/*BSD）
//
// 隔离层：本文件允许出现 #ifdef 与 POSIX 头；其它任何文件不得包含它们。
#if !defined(_WIN32)

#include "src/network/platform_internal.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace termchart::network::platform {
namespace {

ErrorCode errno_to_code(int err) noexcept {
    switch (err) {
        case EINTR:
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return ErrorCode::IoError;  // 超时/中断：统一 IoError，消息由调用方补
        case ECONNREFUSED:
        case ECONNRESET:
        case ENETUNREACH:
        case EHOSTUNREACH:
        case ETIMEDOUT:
            return ErrorCode::IoError;
        default:
            return ErrorCode::PlatformError;
    }
}

void set_timeout(int fd, int opt, std::uint32_t timeout_ms) noexcept {
    struct timeval tv {};
    tv.tv_sec = static_cast<long>(timeout_ms / 1000);
    tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
    (void)::setsockopt(fd, SOL_SOCKET, opt, &tv, sizeof(tv));
}

}  // namespace

Result<Handle> connect(std::string_view host, std::uint16_t port,
                       const SocketOptions& options) noexcept {
    if (host.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const std::string host_str{host};
    const std::string port_str = std::to_string(port);

    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;      // IPv4/IPv6 均可
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* result = nullptr;
    const int gai = ::getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &result);
    if (gai != 0 || result == nullptr) {
        if (result != nullptr) {
            ::freeaddrinfo(result);
        }
        return Unexpected<ErrorCode>(ErrorCode::IoError);  // DNS 失败归 IoError
    }

    int fd = -1;
    for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        set_timeout(fd, SO_SNDTIMEO, options.connect_timeout_ms);
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;  // 连接成功
        }
        const int saved = errno;
        ::close(fd);
        errno = saved;
        fd = -1;
    }
    ::freeaddrinfo(result);
    if (fd < 0) {
        // 只有连接阶段把 errno 归一成 IoError（TCP 连接失败的常规表象）。
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
    set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
    return static_cast<Handle>(fd);
}

Result<std::size_t> read(Handle fd, std::span<char> buffer) noexcept {
    if (fd == kInvalidHandle || buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const ssize_t n = ::recv(static_cast<int>(fd), buffer.data(), buffer.size(), 0);
    if (n < 0) {
        return Unexpected<ErrorCode>(errno_to_code(errno));
    }
    return static_cast<std::size_t>(n);  // 0 = 对端关闭
}

Result<std::size_t> write(Handle fd, std::span<const char> data) noexcept {
    if (fd == kInvalidHandle) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n =
            ::send(static_cast<int>(fd), data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断：重试
            }
            return Unexpected<ErrorCode>(errno_to_code(errno));
        }
        sent += static_cast<std::size_t>(n);
    }
    return sent;
}

void close(Handle fd) noexcept {
    if (fd != kInvalidHandle) {
        (void)::close(static_cast<int>(fd));
    }
}

}  // namespace termchart::network::platform

#endif  // !_WIN32
