// socket.cpp —— TcpSocket 的平台无关实现（平台调用走 src/network/platform_internal.hpp）

#include "termchart/network/socket.hpp"

#include <memory>
#include <utility>

#include "src/network/platform_internal.hpp"

namespace termchart::network {
namespace {

// DNS 名的实际上限（RFC 1035：253 字符的呈现形式）。超长输入直接拒，不劳驾
// getaddrinfo——既快也免去各平台对病态 host 的行为差异。
constexpr std::size_t kMaxHostLength = 253;

}  // namespace

struct TcpSocket::Impl {
    platform::Handle fd = platform::kInvalidHandle;
    std::uint32_t io_timeout_ms = 30000;
};

TcpSocket::TcpSocket() noexcept = default;

TcpSocket::TcpSocket(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

TcpSocket::~TcpSocket() { close(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept = default;

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

Result<TcpSocket> TcpSocket::connect(std::string_view host, std::uint16_t port,
                                     const SocketOptions& options) noexcept {
    if (host.empty() || host.size() > kMaxHostLength) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const Result<platform::Handle> fd = platform::connect(host, port, options);
    if (!fd.has_value()) {
        return Unexpected<ErrorCode>(fd.error());
    }
    auto impl = std::make_unique<Impl>();
    impl->fd = *fd;
    impl->io_timeout_ms = options.io_timeout_ms;
    return TcpSocket{std::move(impl)};
}

Result<std::size_t> TcpSocket::read(std::span<char> buffer) noexcept {
    if (!valid() || buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    return platform::read(impl_->fd, buffer);
}

Result<std::size_t> TcpSocket::write(std::span<const char> data) noexcept {
    if (!valid()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    return platform::write(impl_->fd, data);
}

void TcpSocket::close() noexcept {
    if (impl_ != nullptr && impl_->fd != platform::kInvalidHandle) {
        platform::close(impl_->fd);
        impl_->fd = platform::kInvalidHandle;
    }
}

bool TcpSocket::valid() const noexcept {
    return impl_ != nullptr && impl_->fd != platform::kInvalidHandle;
}

}  // namespace termchart::network
