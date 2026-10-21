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
// 已知环境限制：**已无**。2026-10-21 修复了此前被误判为"沙箱策略限制"的真 bug：
// MinGW-w64 的 schannel.h 结构体已是 V4（含 dwCredFormat），但 SCH_CRED_VERSION
// 宏仍停留在 0x00000002；SChannel 对 dwVersion 精确匹配，传 2 会被
// AcquireCredentialsHandleW 以 SEC_E_UNKNOWN_CREDENTIALS（0x8009030D）拒绝，
// 导致**所有** https 握手失败（不止沙箱——真机同样命中）。修复为显式传 4。
// 若未来再遇该错误码，先查 dwVersion，勿再归因于环境。
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

// 后端可用性自检：**不联网**，只验证本机 SChannel 能否取得客户端凭据
// （AcquireCredentialsHandleW，即 dwVersion 回归的探测点）。Windows 上正常
// 应为 true；策略加固到"凭据系统整体禁用"的机器上为 false（那种环境确实
// 存在，此时后端 fail-loud，绝不静默退回明文）。非 Windows 平台恒为 false。
[[nodiscard]] TERMCHART_API bool credentials_available() noexcept;

}  // namespace termchart::network::tls::schannel

#endif  // TERMCHART_NETWORK_TLS_SCHANNEL_HPP
