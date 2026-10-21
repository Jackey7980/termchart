// schannel.cpp —— Windows 原生 TLS 后端实现（SChannel）
//
// 隔离层：本文件允许出现 #ifdef 与 <windows.h>/<security.h>；其它任何文件
// 不得包含它们（与 src/network/socket_win32.cpp 同纪律）。
#ifdef _WIN32

#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/network/tls/registry.hpp"
#include "termchart/network/tls/schannel.hpp"

namespace termchart::network::tls::schannel {
namespace {

constexpr std::size_t kIoBufferBytes = 16u * 1024u;
constexpr DWORD kSchannelProtocols = 0;  // 0 = 系统默认（含 TLS 1.2 / 1.3）

// 凭据结构标志位。两个要点：
//   * SCH_CRED_NO_SERVERNAME_CHECK 绝不能带：主机名校验由 verify_certificate
//     显式做（MANUAL_CRED_VALIDATION + CertVerifyCertificateChainPolicy(SSL)）；
//   * SCH_CRED_NO_DEFAULT_CREDS 不能带：不提供客户端证书时它会要
//     "没有可用凭据"（SEC_E_UNKNOWN_CREDENTIALS，0x8009030D）。让它按默认
//     选证书即可——绝大多数 HTTPS 服务端不要客户端证书。
constexpr DWORD kSchannelCredFlags = SCH_CRED_MANUAL_CRED_VALIDATION;

// 握手指令位（上下文属性请求）。
constexpr DWORD kIscFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                            ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM |
                            ISC_REQ_ALLOCATE_MEMORY;

std::wstring widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int wide_len = ::MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(wide_len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          out.data(), wide_len);
    return out;
}

// SSPI 状态码 → 库错误码。证书/信任类失败一律 Unsupported（fail-loud 的
// "这个后端明确说不行"，与"后端不存在"区分不开，但都不静默退回明文）。
ErrorCode sspi_to_code(SECURITY_STATUS status) noexcept {
    switch (status) {
        case SEC_E_OK:
            return ErrorCode::Ok;
        case SEC_E_UNTRUSTED_ROOT:
        case SEC_E_CERT_EXPIRED:
        case SEC_E_CERT_UNKNOWN:
        case SEC_E_WRONG_PRINCIPAL:
        case SEC_E_ILLEGAL_MESSAGE:
        case SEC_E_ALGORITHM_MISMATCH:
        case SEC_E_INVALID_TOKEN:
            return ErrorCode::Unsupported;
        case SEC_E_INCOMPLETE_MESSAGE:
            return ErrorCode::ParseError;
        default:
            return ErrorCode::IoError;
    }
}

// 输出令牌（ClientHello / 握手各飞行）经 socket 发出。TcpSocket::write 保证全写。
bool send_token(TcpSocket& socket, const SecBuffer& token) noexcept {
    if (token.cbBuffer == 0 || token.pvBuffer == nullptr) {
        return true;
    }
    const Result<std::size_t> n = socket.write(
        std::span<const char>{static_cast<const char*>(token.pvBuffer), token.cbBuffer});
    return n.has_value();
}

// 客户端凭据配置（握手与 credentials_available 自检共用）。
//
// dwVersion 必须是 4（SCH_CRED_VERSION 的当前 ABI）——**不能直接用 MinGW-w64
// 的头宏**：它的 schannel.h 结构体已是 V4（含 dwCredFormat），但宏仍停留在
// 0x00000002（mingw-w64 头的已知疏漏）。SChannel 对 dwVersion 精确匹配，传 2
// 会被 AcquireCredentialsHandleW 以 SEC_E_UNKNOWN_CREDENTIALS（0x8009030D）
// 拒绝——**所有** https 握手失败（2026-10-21 实证：真机同样命中，曾被误判为
// "沙箱策略限制"而写进头注释）。零初始化保证 dwCredFormat =
// SCH_CRED_FORMAT_CERT_CONTEXT（0）。
SCHANNEL_CRED make_cred() noexcept {
    SCHANNEL_CRED cred{};
    cred.dwVersion = 0x00000004;
    cred.grbitEnabledProtocols = kSchannelProtocols;
    // 手动验证证书：信任判定交给 verify_certificate（链 + 主机名），不用
    // "握手过了就信"的默认行为。
    cred.dwFlags = kSchannelCredFlags;
    return cred;
}

}  // namespace

class SchannelBackend final : public TlsBackend {
public:
    SchannelBackend() noexcept = default;
    ~SchannelBackend() override { release(); }

    SchannelBackend(const SchannelBackend&) = delete;
    SchannelBackend& operator=(const SchannelBackend&) = delete;

    Status handshake(TcpSocket& socket, std::string_view hostname) noexcept override;
    Result<std::size_t> read(TcpSocket& socket, std::span<char> buffer) noexcept override;
    Result<std::size_t> write(TcpSocket& socket,
                              std::span<const char> data) noexcept override;
    void shutdown(TcpSocket& socket) noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override { return "schannel"; }

private:
    Status do_handshake(TcpSocket& socket, const std::wstring& hostname_w);
    Status verify_certificate(const std::wstring& hostname_w) noexcept;
    Result<bool> fill_wire(TcpSocket& socket);  // false = 对端关闭/EOF
    void release() noexcept;

    bool valid_ = false;  // 握手成功才能读写
    bool has_cred_ = false;
    bool has_ctx_ = false;
    CredHandle cred_{};
    CtxtHandle ctx_{};
    SecPkgContext_StreamSizes sizes_{};
    std::string hostname_;
    std::vector<char> wire_;  // 记录层：已收未解密
    std::size_t wire_pos_ = 0;
    std::string plain_;  // 已解密未交付
    std::size_t plain_pos_ = 0;
};

Status SchannelBackend::handshake(TcpSocket& socket, std::string_view hostname) noexcept {
    hostname_ = std::string{hostname};
    const std::wstring hostname_w = widen(hostname_);
    const Status st = do_handshake(socket, hostname_w);
    if (st != ErrorCode::Ok) {
        return st;
    }
    // 流尺寸必须在上下文建立后查；随后做证书链 + 主机名验证。
    if (::QueryContextAttributesW(&ctx_, SECPKG_ATTR_STREAM_SIZES, &sizes_) != SEC_E_OK) {
        return ErrorCode::PlatformError;
    }
    return verify_certificate(hostname_w);
}

Status SchannelBackend::do_handshake(TcpSocket& socket, const std::wstring& hostname_w) {
    TimeStamp expiry{};
    SCHANNEL_CRED cred = make_cred();  // pvAuthData 是非 const void*，须左值
    SECURITY_STATUS st = ::AcquireCredentialsHandleW(
        nullptr, UNISP_NAME_W, SECPKG_CRED_OUTBOUND, nullptr, &cred, nullptr, nullptr,
        &cred_, &expiry);
    if (st != SEC_E_OK) {
        return ErrorCode::PlatformError;
    }
    has_cred_ = true;

    // ISC 的 pszTargetName 需要非 const 缓冲（SNI 主机名）。
    std::wstring target = hostname_w;

    // 第一轮：无输入，产出 ClientHello。phNewContext 必须给 &ctx_（首调创建
    // 上下文句柄），之后循环都复用同一句柄。
    SecBuffer out{};
    out.BufferType = SECBUFFER_TOKEN;
    out.cbBuffer = static_cast<unsigned long>(kIoBufferBytes);
    out.pvBuffer = ::LocalAlloc(LMEM_FIXED, kIoBufferBytes);
    if (out.pvBuffer == nullptr) {
        return ErrorCode::OutOfRange;
    }
    SecBufferDesc out_desc{SECBUFFER_VERSION, 1, &out};
    DWORD attrs = 0;
    st = ::InitializeSecurityContextW(&cred_, nullptr, target.data(), kIscFlags, 0, 0,
                                      nullptr, 0, &ctx_, &out_desc, &attrs, &expiry);
    has_ctx_ = true;  // 句柄已创建（失败也走 release 统一清理）
    const bool sent = send_token(socket, out);
    ::LocalFree(out.pvBuffer);
    out.pvBuffer = nullptr;
    if (!sent) {
        return ErrorCode::IoError;
    }
    if (st != SEC_I_CONTINUE_NEEDED) {
        // 服务端在 ClientHello 前就关门/发错（不可能成功握手）。
        return sspi_to_code(st);
    }

    bool need_more = false;  // 上一轮拿到不完整记录：必须先收字节
    std::uint32_t flights = 0;  // 握手往返次数上限，防病态服务端把客户端绕死
    for (;;) {
        if (need_more || wire_pos_ >= wire_.size()) {
            need_more = false;
            const Result<bool> more = fill_wire(socket);
            if (!more.has_value()) {
                return more.error();
            }
            if (!*more) {
                return ErrorCode::ParseError;  // 握手中对端关闭
            }
            continue;
        }
        SecBuffer in[4] = {};
        in[0].pvBuffer = wire_.data() + wire_pos_;
        in[0].cbBuffer = static_cast<unsigned long>(wire_.size() - wire_pos_);
        in[0].BufferType = SECBUFFER_TOKEN;
        in[1].BufferType = SECBUFFER_EMPTY;
        in[2].BufferType = SECBUFFER_EMPTY;
        in[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc in_desc{SECBUFFER_VERSION, 4, in};

        SecBuffer out_buf{};
        out_buf.BufferType = SECBUFFER_TOKEN;
        out_buf.cbBuffer = static_cast<unsigned long>(kIoBufferBytes);
        out_buf.pvBuffer = ::LocalAlloc(LMEM_FIXED, kIoBufferBytes);
        if (out_buf.pvBuffer == nullptr) {
            return ErrorCode::OutOfRange;
        }
        SecBufferDesc out_buf_desc{SECBUFFER_VERSION, 1, &out_buf};
        TimeStamp step_expiry{};
        st = ::InitializeSecurityContextW(&cred_, &ctx_, target.data(), kIscFlags, 0, 0,
                                          &in_desc, 0, nullptr, &out_buf_desc, &attrs,
                                          &step_expiry);
        ++flights;  // 每次成功解析一条记录算一个飞行；病态服务端超过 64 个即失败
        if (flights > 64) {
            ::LocalFree(out_buf.pvBuffer);
            return ErrorCode::ParseError;
        }
        const bool step_sent = send_token(socket, out_buf);
        ::LocalFree(out_buf.pvBuffer);
        out_buf.pvBuffer = nullptr;
        if (!step_sent) {
            return ErrorCode::IoError;
        }
        if (st == SEC_E_INCOMPLETE_MESSAGE) {
            need_more = true;  // 记录不完整：**必须先收字节**，不能原地重试
            continue;
        }
        // 输入缓冲的剩余部分由 SECBUFFER_EXTRA 报告（经典 Schannel 样例口径）。
        if (in[1].BufferType == SECBUFFER_EXTRA && in[1].cbBuffer > 0) {
            wire_pos_ = wire_.size() - in[1].cbBuffer;
        } else {
            wire_pos_ = wire_.size();
        }
        if (st == SEC_E_OK) {
            valid_ = true;
            return ErrorCode::Ok;
        }
        if (st == SEC_I_CONTINUE_NEEDED) {
            continue;
        }
        return sspi_to_code(st);
    }
}

Status SchannelBackend::verify_certificate(const std::wstring& hostname_w) noexcept {
    PCCERT_CONTEXT remote = nullptr;
    // QueryContextAttributesW 的 PCtxtHandle 非 const（SSPI 的老式签名）。
    CtxtHandle ctx = ctx_;
    if (::QueryContextAttributesW(&ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remote) !=
            SEC_E_OK ||
        remote == nullptr) {
        return ErrorCode::Unsupported;
    }

    CERT_CHAIN_PARA chain_para{};
    chain_para.cbSize = sizeof(chain_para);
    chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
    // 服务器身份 OID（szOID_PKIX_KP_SERVER_AUTH = "1.3.6.1.5.5.7.3.1"）。
    LPSTR usage[] = {const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH)};
    chain_para.RequestedUsage.Usage.cUsageIdentifier = 1;
    chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = usage;

    PCCERT_CHAIN_CONTEXT chain = nullptr;
    if (::CertGetCertificateChain(nullptr, remote, nullptr, remote->hCertStore,
                                  &chain_para, 0, nullptr, &chain) != TRUE ||
        chain == nullptr) {
        ::CertFreeCertificateContext(remote);
        return ErrorCode::Unsupported;
    }

    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl{};
    ssl.cbSize = sizeof(ssl);
    ssl.dwAuthType = AUTHTYPE_SERVER;
    std::wstring server_name = hostname_w;  // pwszServerName 非 const
    ssl.pwszServerName = server_name.data();
    CERT_CHAIN_POLICY_PARA policy{};
    policy.cbSize = sizeof(policy);
    policy.pvExtraPolicyPara = &ssl;
    CERT_CHAIN_POLICY_STATUS status{};
    status.cbSize = sizeof(status);

    const BOOL policy_ok =
        ::CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &policy, &status);
    const ErrorCode code =
        (policy_ok == TRUE && status.dwError == 0) ? ErrorCode::Ok : ErrorCode::Unsupported;
    ::CertFreeCertificateChain(chain);
    ::CertFreeCertificateContext(remote);
    return code;
}

Result<bool> SchannelBackend::fill_wire(TcpSocket& socket) {
    if (wire_pos_ > 0) {
        wire_.erase(wire_.begin(),
                    wire_.begin() + static_cast<std::ptrdiff_t>(wire_pos_));
        wire_pos_ = 0;
    }
    std::vector<char> chunk(kIoBufferBytes);
    const Result<std::size_t> n = socket.read(chunk);
    if (!n.has_value()) {
        return Unexpected<ErrorCode>(n.error());
    }
    if (*n == 0) {
        return false;  // 对端关闭
    }
    wire_.insert(wire_.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(*n));
    return true;
}

Result<std::size_t> SchannelBackend::read(TcpSocket& socket, std::span<char> buffer) noexcept {
    if (!valid_) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    if (buffer.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    // 先交付已解密缓冲。
    if (plain_pos_ < plain_.size()) {
        const std::size_t n =
            std::min(buffer.size(), plain_.size() - plain_pos_);
        std::memcpy(buffer.data(), plain_.data() + plain_pos_, n);
        plain_pos_ += n;
        return n;
    }
    plain_.clear();
    plain_pos_ = 0;

    for (;;) {
        if (wire_pos_ >= wire_.size()) {
            const Result<bool> more = fill_wire(socket);
            if (!more.has_value()) {
                return Unexpected<ErrorCode>(more.error());
            }
            if (!*more) {
                return std::size_t{0};  // 对端关闭：EOF
            }
        }
        SecBuffer bufs[4] = {};
        bufs[0].pvBuffer = wire_.data() + wire_pos_;
        bufs[0].cbBuffer = static_cast<unsigned long>(wire_.size() - wire_pos_);
        bufs[0].BufferType = SECBUFFER_DATA;
        bufs[1].BufferType = SECBUFFER_EMPTY;
        bufs[2].BufferType = SECBUFFER_EMPTY;
        bufs[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc{SECBUFFER_VERSION, 4, bufs};

        const SECURITY_STATUS st = ::DecryptMessage(&ctx_, &desc, 0, nullptr);
        if (st == SEC_E_INCOMPLETE_MESSAGE) {
            continue;  // 记录不完整：收更多
        }
        if (st == SEC_I_CONTEXT_EXPIRED) {
            wire_pos_ = wire_.size();
            return std::size_t{0};  // 服务端发来 close_notify
        }
        if (st != SEC_E_OK) {
            return Unexpected<ErrorCode>(sspi_to_code(st));
        }
        if (bufs[1].BufferType == SECBUFFER_DATA && bufs[1].cbBuffer > 0) {
            plain_.append(static_cast<const char*>(bufs[1].pvBuffer), bufs[1].cbBuffer);
        }
        // 未解密字节由 SECBUFFER_EXTRA（bufs[3]）报告；无则本记录已全消费。
        const std::size_t extra =
            bufs[3].BufferType == SECBUFFER_EXTRA ? bufs[3].cbBuffer : 0;
        wire_pos_ = wire_.size() - extra;

        if (!plain_.empty()) {
            const std::size_t n = std::min(buffer.size(), plain_.size());
            std::memcpy(buffer.data(), plain_.data(), n);
            plain_pos_ = n;
            return n;
        }
        // 空记录（如握手后服务端先发的 alert 前的 keepalive）：继续读。
    }
}

Result<std::size_t> SchannelBackend::write(TcpSocket& socket,
                                           std::span<const char> data) noexcept {
    if (!valid_) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const std::size_t chunk =
            std::min(data.size() - sent, static_cast<std::size_t>(sizes_.cbMaximumMessage));
        std::vector<char> buf(sizes_.cbHeader + chunk + sizes_.cbTrailer);
        char* base = buf.data();
        SecBuffer bufs[3] = {};
        bufs[0].pvBuffer = base;
        bufs[0].cbBuffer = sizes_.cbHeader;
        bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
        bufs[1].pvBuffer = base + sizes_.cbHeader;
        bufs[1].cbBuffer = static_cast<unsigned long>(chunk);
        bufs[1].BufferType = SECBUFFER_DATA;
        bufs[2].pvBuffer = base + sizes_.cbHeader + chunk;
        bufs[2].cbBuffer = sizes_.cbTrailer;
        bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
        std::memcpy(bufs[1].pvBuffer, data.data() + sent, chunk);
        SecBufferDesc desc{SECBUFFER_VERSION, 3, bufs};

        const SECURITY_STATUS st = ::EncryptMessage(&ctx_, 0, &desc, 0);
        if (st != SEC_E_OK) {
            return Unexpected<ErrorCode>(sspi_to_code(st));
        }
        const std::size_t total =
            static_cast<std::size_t>(bufs[0].cbBuffer) + bufs[1].cbBuffer + bufs[2].cbBuffer;
        const Result<std::size_t> n =
            socket.write(std::span<const char>{base, total});
        if (!n.has_value()) {
            return Unexpected<ErrorCode>(n.error());
        }
        sent += chunk;
    }
    return sent;
}

void SchannelBackend::shutdown(TcpSocket& socket) noexcept {
    if (!valid_) {
        release();
        return;
    }
    // best-effort close_notify：控制令牌 + 一条空记录。
    DWORD type = SCHANNEL_SHUTDOWN;
    SecBuffer token{};
    token.BufferType = SECBUFFER_TOKEN;
    token.cbBuffer = sizeof(type);
    token.pvBuffer = &type;
    SecBufferDesc token_desc{SECBUFFER_VERSION, 1, &token};
    ::ApplyControlToken(&ctx_, &token_desc);

    SecBuffer out{};
    out.BufferType = SECBUFFER_TOKEN;
    out.cbBuffer = static_cast<unsigned long>(kIoBufferBytes);
    out.pvBuffer = ::LocalAlloc(LMEM_FIXED, kIoBufferBytes);
    if (out.pvBuffer != nullptr) {
        SecBufferDesc out_desc{SECBUFFER_VERSION, 1, &out};
        if (::EncryptMessage(&ctx_, 0, &out_desc, 0) == SEC_E_OK) {
            (void)send_token(socket, out);
        }
        ::LocalFree(out.pvBuffer);
    }
    release();
}

void SchannelBackend::release() noexcept {
    valid_ = false;
    if (has_ctx_) {
        ::DeleteSecurityContext(&ctx_);
        has_ctx_ = false;
    }
    if (has_cred_) {
        ::FreeCredentialsHandle(&cred_);
        has_cred_ = false;
    }
}

}  // namespace termchart::network::tls::schannel

namespace termchart::network::tls::schannel {

std::unique_ptr<TlsBackend> create_backend() noexcept {
    return std::make_unique<SchannelBackend>();
}

void register_backend() noexcept {
    TlsRegistry::instance().register_backend("schannel", [] {
        return std::make_unique<SchannelBackend>();
    });
}

bool credentials_available() noexcept {
    CredHandle handle{};
    TimeStamp expiry{};
    SCHANNEL_CRED cred = make_cred();  // pvAuthData 是非 const void*，须左值
    const SECURITY_STATUS st = ::AcquireCredentialsHandleW(
        nullptr, UNISP_NAME_W, SECPKG_CRED_OUTBOUND, nullptr, &cred, nullptr,
        nullptr, &handle, &expiry);
    if (st == SEC_E_OK) {
        ::FreeCredentialsHandle(&handle);
        return true;
    }
    return false;
}

}  // namespace termchart::network::tls::schannel

#else  // !_WIN32

// POSIX：无 SChannel。本 TU 保持可编译（单头/跨平台构建），create 返回
// nullptr，register 空操作——https 在 POSIX 上要靠 OpenSSL 后端，否则
// http.cpp 显式 Unsupported（fail-loud，不静默退回明文）。
#include "termchart/network/tls/schannel.hpp"

namespace termchart::network::tls::schannel {

std::unique_ptr<TlsBackend> create_backend() noexcept { return nullptr; }

void register_backend() noexcept {}

bool credentials_available() noexcept { return false; }

}  // namespace termchart::network::tls::schannel

#endif  // _WIN32
