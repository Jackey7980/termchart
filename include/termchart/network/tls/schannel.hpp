// schannel.hpp —— Windows 原生 TLS 后端（SChannel / Schannel SSP）
//
// 平台后端之一（另一支是 OpenSSL，随可选依赖编译）：
//   * 实现 TlsBackend（握手 + 记录层加解密 + close_notify）；
//   * 一个后端实例服务**一条连接**（握手掌状态 CtxtHandle 在实例里）——
//     每次 create() 拿新实例，勿跨连接复用；
//   * 证书链与主机名**手动验证**（SCH_CRED_MANUAL_CRED_VALIDATION +
//     CertGetCertificateChain / CertVerifyCertificateChainPolicy(SSL)），
//     失败即握手失败，绝不接受"链错了但握手过了"的连接；
//   * 注册：registry.cpp 构造注册表时直接把本后端登记进去（显式引用
//     create_backend，避免"静态库里无人引用的 TU 被链接器丢弃"陷阱）；
//     register_backend() 留给需要显式控制注册时机的调用方。
//
// 已知环境限制：某些沙箱/策略加固的 Windows 上 AcquireCredentialsHandleW
// 会一律返回 SEC_E_UNKNOWN_CREDENTIALS（0x8009030D，连带客户端证书也一样，
// 而 NTLM/证书系统库正常）——那类机器上本后端 fail-loud 报 Unsupported，
// 绝不静默退回明文。2026-10-19 在开发沙箱里实测命中该策略。
#ifndef TERMCHART_NETWORK_TLS_SCHANNEL_HPP
#define TERMCHART_NETWORK_TLS_SCHANNEL_HPP

#include <memory>
#include <string_view>

#include "termchart/export.hpp"
#include "termchart/network/tls/backend.hpp"

namespace termchart::network::tls::schannel {

// 建一个 SChannel 后端实例。非 Windows 平台返回 nullptr（本Header 仍可编译）。
[[nodiscard]] TERMCHART_API std::unique_ptr<TlsBackend> create_backend() noexcept;

// 注册进 TlsRegistry（幂等）。静态初始化已自动注册一次，重复调用无副作用。
TERMCHART_API void register_backend() noexcept;

}  // namespace termchart::network::tls::schannel

#endif  // TERMCHART_NETWORK_TLS_SCHANNEL_HPP
