// socket_win32.cpp —— TcpSocket 的 Win32 平台实现（Winsock2）
//
// 隔离层：本文件允许出现 #ifdef 与 <winsock2.h>；其它任何文件不得包含它们。
#ifdef _WIN32

#include "src/network/dns_cache.hpp"
#include "src/network/platform_internal.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <vector>

namespace termchart::network::platform {
namespace {

// recv/send 的 len 是 int；≥2GB 的 span 必须分段，否则截断成负数/错误长度。
constexpr std::size_t kMaxIoChunk = static_cast<std::size_t>(INT_MAX);

// Winsock 需要进程级 WSAStartup；用函数内 static 守卫，首次建 socket 前初始化。
struct WsaGuard {
    bool ok = false;
    WsaGuard() noexcept {
        WSADATA data{};
        ok = (::WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }
    ~WsaGuard() noexcept {
        if (ok) {
            ::WSACleanup();
        }
    }
};

bool& wsa_ok() noexcept {
    static WsaGuard guard;
    return guard.ok;
}

ErrorCode wsa_to_code(int err) noexcept {
    switch (err) {
        case WSAETIMEDOUT:
        case WSAECONNREFUSED:
        case WSAECONNRESET:
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:
            return ErrorCode::IoError;
        default:
            return ErrorCode::PlatformError;
    }
}

void set_timeout(SOCKET fd, int opt, std::uint32_t timeout_ms) noexcept {
    const DWORD tv = static_cast<DWORD>(timeout_ms);
    (void)::setsockopt(fd, SOL_SOCKET, opt, reinterpret_cast<const char*>(&tv), sizeof(tv));
}

// 连接建立后的统一选项：关 Nagle（请求-响应模式）、按需 keepalive。
void apply_connected_options(SOCKET fd, const SocketOptions& options) noexcept {
    if (options.tcp_nodelay) {
        const DWORD one = 1;
        (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one),
                           sizeof(one));
    }
    if (options.keepalive) {
        const DWORD one = 1;
        (void)::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&one),
                           sizeof(one));
    }
}

bool set_nonblocking(SOCKET fd, bool on) noexcept {
    u_long mode = on ? 1u : 0u;
    return ::ioctlsocket(fd, FIONBIO, &mode) == 0;
}

// connect() 带超时：非阻塞发起 → select 等待可写 → SO_ERROR 拿真实结果。
Status connect_with_timeout(SOCKET fd, const sockaddr* addr, int addr_len,
                            std::uint32_t timeout_ms) noexcept {
    if (!set_nonblocking(fd, true)) {
        return ErrorCode::PlatformError;
    }
    const int rc = ::connect(fd, addr, addr_len);
    if (rc == 0) {
        (void)set_nonblocking(fd, false);  // 立即连通：恢复阻塞
        return ErrorCode::Ok;
    }
    const int err = ::WSAGetLastError();
    if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
        return ErrorCode::IoError;
    }
    fd_set wr;
    FD_ZERO(&wr);
    FD_SET(fd, &wr);
    fd_set ex;
    FD_ZERO(&ex);
    FD_SET(fd, &ex);
    struct timeval tv {};
    tv.tv_sec = static_cast<long>(static_cast<unsigned long>(timeout_ms) / 1000ul);
    tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
    for (;;) {
        // writable 或 except（连接失败也置可写，靠 SO_ERROR 区分）
        const int ready = ::select(0, nullptr, &wr, &ex, &tv);
        if (ready > 0) {
            break;
        }
        if (ready == 0) {
            return ErrorCode::IoError;  // 超时
        }
        const int sel_err = ::WSAGetLastError();
        if (sel_err == WSAEINTR) {
            continue;  // 信号打断（WSAEventSelect 干扰等）：重试
        }
        return ErrorCode::IoError;
    }
    int so_error = 0;
    int len = sizeof(so_error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) != 0) {
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
    if (host.empty() || !wsa_ok()) {
        return Unexpected<ErrorCode>(host.empty() ? ErrorCode::InvalidArgument
                                                 : ErrorCode::PlatformError);
    }
    const std::string host_str{host};
    const std::string port_str = std::to_string(port);

    // 1) 缓存命中：免 getaddrinfo 的阻塞 DNS 往返，直接用中立地址建连。
    const std::vector<NeutralAddr> cached = dns_cache_get(host_str, port);
    if (!cached.empty()) {
        SOCKET fd = INVALID_SOCKET;
        for (const NeutralAddr& entry : cached) {
            const auto [family, bytes] = entry;
            if ((family != AF_INET && family != AF_INET6) || bytes.empty()) {
                continue;
            }
            fd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
            if (fd == INVALID_SOCKET) {
                continue;
            }
            const Status status = connect_with_timeout(
                fd, reinterpret_cast<const sockaddr*>(bytes.data()),
                static_cast<int>(bytes.size()), options.connect_timeout_ms);
            if (status == ErrorCode::Ok) {
                set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
                set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
                apply_connected_options(fd, options);
                return static_cast<Handle>(fd);
            }
            ::closesocket(fd);
            fd = INVALID_SOCKET;
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

    SOCKET fd = INVALID_SOCKET;
    Status status = ErrorCode::IoError;
    for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == INVALID_SOCKET) {
            continue;
        }
        status = connect_with_timeout(fd, ai->ai_addr, static_cast<int>(ai->ai_addrlen),
                                      options.connect_timeout_ms);
        if (status == ErrorCode::Ok) {
            break;  // 连接成功
        }
        ::closesocket(fd);
        fd = INVALID_SOCKET;
    }
    if (fd != INVALID_SOCKET) {
        dns_cache_put(host_str, port, to_neutral(result));
    }
    ::freeaddrinfo(result);
    if (fd == INVALID_SOCKET) {
        // 只有连接阶段把 WSA 错误归一成 IoError（TCP 连接失败的常规表象）。
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
    set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
    apply_connected_options(fd, options);
    // INVALID_SOCKET 是 ~0（全 1），不会与 intptr_t 的 -1 混淆：合法 fd 映射为正。
    return static_cast<Handle>(fd);
}

Result<std::size_t> read(Handle fd, std::span<char> buffer) noexcept {
    if (fd == kInvalidHandle || buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    // 单次读语义：与 POSIX 一致（返回 ≤ buffer.size() 的实际字节，0 = 关闭）。
    const int want = buffer.size() > kMaxIoChunk ? static_cast<int>(kMaxIoChunk)
                                                 : static_cast<int>(buffer.size());
    for (;;) {
        const int n = ::recv(static_cast<SOCKET>(fd), buffer.data(), want, 0);
        if (n != SOCKET_ERROR) {
            return static_cast<std::size_t>(n);  // 0 = 对端关闭
        }
        const int err = ::WSAGetLastError();
        if (err == WSAEINTR) {
            continue;  // 信号打断：重试（仍是单次读语义）
        }
        return Unexpected<ErrorCode>(wsa_to_code(err));
    }
}

Result<std::size_t> write(Handle fd, std::span<const char> data) noexcept {
    if (fd == kInvalidHandle) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const std::size_t left = data.size() - sent;
        const int want = left > kMaxIoChunk ? static_cast<int>(kMaxIoChunk)
                                            : static_cast<int>(left);
        const int n = ::send(static_cast<SOCKET>(fd), data.data() + sent, want, 0);
        if (n == SOCKET_ERROR) {
            const int err = ::WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
            return Unexpected<ErrorCode>(wsa_to_code(err));
        }
        sent += static_cast<std::size_t>(n);
    }
    return sent;
}

void close(Handle fd) noexcept {
    if (fd != kInvalidHandle) {
        ::closesocket(static_cast<SOCKET>(fd));
    }
}

}  // namespace termchart::network::platform

#endif  // _WIN32
