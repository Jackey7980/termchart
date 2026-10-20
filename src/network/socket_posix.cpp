// socket_posix.cpp —— TcpSocket 的 POSIX 平台实现（Linux/macOS/*BSD）
//
// 隔离层：本文件允许出现 #ifdef 与 POSIX 头；其它任何文件不得包含它们。
#if !defined(_WIN32)

#include "src/network/dns_cache.hpp"
#include "src/network/platform_internal.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

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

// EINTR 只是信号打断（如 SIGWINCH），语义上必须重试而不是上抛。
bool interrupted(int err) noexcept { return err == EINTR; }

void set_timeout(int fd, int opt, std::uint32_t timeout_ms) noexcept {
    struct timeval tv {};
    tv.tv_sec = static_cast<long>(timeout_ms / 1000);
    tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
    (void)::setsockopt(fd, SOL_SOCKET, opt, &tv, sizeof(tv));
}

// 连接建立后的统一选项：关 Nagle（请求-响应模式）、按需 keepalive。
void apply_connected_options(int fd, const SocketOptions& options) noexcept {
    if (options.tcp_nodelay) {
        const int one = 1;
        (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }
    if (options.keepalive) {
        const int one = 1;
        (void)::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    }
}

bool set_nonblocking(int fd, bool on) noexcept {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    const int wanted = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(fd, F_SETFL, wanted) == 0;
}

// connect() 带超时：非阻塞发起 → poll 等待可写 → SO_ERROR 拿真实结果。
// SO_SNDTIMEO 在 Linux 上对 connect() 无效，这是唯一可靠做法。
Status connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addr_len,
                            std::uint32_t timeout_ms) noexcept {
    if (!set_nonblocking(fd, true)) {
        return ErrorCode::PlatformError;
    }
    const int rc = ::connect(fd, addr, addr_len);
    if (rc == 0) {
        (void)set_nonblocking(fd, false);  // 立即连通（本机回环常见）：恢复阻塞
        return ErrorCode::Ok;
    }
    if (errno != EINPROGRESS) {
        return ErrorCode::IoError;
    }
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLOUT;
    for (;;) {
        const int ready = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
        if (ready > 0) {
            break;
        }
        if (ready == 0) {
            return ErrorCode::IoError;  // 超时
        }
        if (!interrupted(errno)) {
            return ErrorCode::IoError;  // poll 本身失败
        }
        // EINTR：重启 poll（重新计算等待时间代价可接受——窗口期极短）
    }
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) {
        return ErrorCode::PlatformError;
    }
    if (so_error != 0) {
        return ErrorCode::IoError;  // 连接被拒/网络不可达等
    }
    if (!set_nonblocking(fd, false)) {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Ok;
}

// addrinfo 链 → 中立 sockaddr 格式（只收 AF_INET/AF_INET6）。
std::vector<NeutralAddr> to_neutral(const struct addrinfo* result) noexcept {
    std::vector<NeutralAddr> out;
    for (const struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        if (ai->ai_addr == nullptr || ai->ai_addrlen == 0) {
            continue;
        }
        if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) {
            continue;
        }
        const auto* base = reinterpret_cast<const unsigned char*>(ai->ai_addr);
        out.emplace_back(ai->ai_family,
                         std::vector<unsigned char>(base, base + ai->ai_addrlen));
    }
    return out;
}

}  // namespace

Result<Handle> connect(std::string_view host, std::uint16_t port,
                       const SocketOptions& options) noexcept {
    if (host.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const std::string host_str{host};
    const std::string port_str = std::to_string(port);

    // 1) 缓存命中：免 getaddrinfo 的阻塞 DNS 往返，直接用中立地址建连。
    const std::vector<NeutralAddr> cached = dns_cache_get(host_str, port);
    if (!cached.empty()) {
        int fd = -1;
        for (const NeutralAddr& entry : cached) {
            const auto [family, bytes] = entry;
            if ((family != AF_INET && family != AF_INET6) || bytes.empty()) {
                continue;
            }
            fd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
            if (fd < 0) {
                continue;
            }
            const Status status = connect_with_timeout(
                fd, reinterpret_cast<const struct sockaddr*>(bytes.data()),
                static_cast<socklen_t>(bytes.size()), options.connect_timeout_ms);
            if (status == ErrorCode::Ok) {
                set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
                set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
                apply_connected_options(fd, options);
                return static_cast<Handle>(fd);
            }
            ::close(fd);
            fd = -1;
        }
        // 缓存条目全连不上（地址可能已变）：落到线下重新解析。
    }

    // 2) getaddrinfo 线下路径；结果回填缓存供后续连接复用。
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
    Status status = ErrorCode::IoError;
    for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        status = connect_with_timeout(fd, ai->ai_addr, ai->ai_addrlen,
                                      options.connect_timeout_ms);
        if (status == ErrorCode::Ok) {
            break;  // 连接成功
        }
        ::close(fd);
        fd = -1;
    }
    if (fd >= 0) {
        dns_cache_put(host_str, port, to_neutral(result));
    }
    ::freeaddrinfo(result);
    if (fd < 0) {
        // 只有连接阶段把 errno 归一成 IoError（TCP 连接失败的常规表象）。
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
    set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
    apply_connected_options(fd, options);
    return static_cast<Handle>(fd);
}

Result<std::size_t> read(Handle fd, std::span<char> buffer) noexcept {
    if (fd == kInvalidHandle || buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    for (;;) {
        const ssize_t n = ::recv(static_cast<int>(fd), buffer.data(), buffer.size(), 0);
        if (n >= 0) {
            return static_cast<std::size_t>(n);  // 0 = 对端关闭
        }
        if (interrupted(errno)) {
            continue;  // 信号打断：重试
        }
        return Unexpected<ErrorCode>(errno_to_code(errno));
    }
}

Result<std::size_t> write(Handle fd, std::span<const char> data) noexcept {
    if (fd == kInvalidHandle) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n =
            ::send(static_cast<int>(fd), data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n >= 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (interrupted(errno)) {
            continue;  // 被信号中断：重试
        }
        return Unexpected<ErrorCode>(errno_to_code(errno));
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
