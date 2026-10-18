// socket_win32.cpp —— TcpSocket 的 Win32 平台实现（Winsock2）
//
// 隔离层：本文件允许出现 #ifdef 与 <winsock2.h>；其它任何文件不得包含它们。
#ifdef _WIN32

#include "src/network/platform_internal.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>

namespace termchart::network::platform {
namespace {

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

}  // namespace

Result<Handle> connect(std::string_view host, std::uint16_t port,
                       const SocketOptions& options) noexcept {
    if (host.empty() || !wsa_ok()) {
        return Unexpected<ErrorCode>(host.empty() ? ErrorCode::InvalidArgument
                                                 : ErrorCode::PlatformError);
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

    SOCKET fd = INVALID_SOCKET;
    for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == INVALID_SOCKET) {
            continue;
        }
        set_timeout(fd, SO_SNDTIMEO, options.connect_timeout_ms);
        if (::connect(fd, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0) {
            break;  // 连接成功
        }
        ::closesocket(fd);
        fd = INVALID_SOCKET;
    }
    ::freeaddrinfo(result);
    if (fd == INVALID_SOCKET) {
        // 只有连接阶段把 WSA 错误归一成 IoError（TCP 连接失败的常规表象）。
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    set_timeout(fd, SO_RCVTIMEO, options.io_timeout_ms);
    set_timeout(fd, SO_SNDTIMEO, options.io_timeout_ms);
    // INVALID_SOCKET 是 ~0（全 1），不会与 intptr_t 的 -1 混淆：合法 fd 映射为正。
    return static_cast<Handle>(fd);
}

Result<std::size_t> read(Handle fd, std::span<char> buffer) noexcept {
    if (fd == kInvalidHandle || buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const int n =
        ::recv(static_cast<SOCKET>(fd), buffer.data(), static_cast<int>(buffer.size()), 0);
    if (n == SOCKET_ERROR) {
        return Unexpected<ErrorCode>(wsa_to_code(::WSAGetLastError()));
    }
    return static_cast<std::size_t>(n);  // 0 = 对端关闭
}

Result<std::size_t> write(Handle fd, std::span<const char> data) noexcept {
    if (fd == kInvalidHandle) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int n = ::send(static_cast<SOCKET>(fd), data.data() + sent,
                             static_cast<int>(data.size() - sent), 0);
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
