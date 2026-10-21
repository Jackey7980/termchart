# 网络：拉取数据

`network` 模块（L7）提供零第三方依赖的 TCP / HTTP 客户端：TCP socket 抽象 +
HTTP/1.1 + TLS（Windows SChannel 原生后端）。典型用途——从 URL 拉图表数据
（示例 `71_fetch_stb` 拉 vendored 头文件）。

## HTTP 客户端

```cpp
#include "termchart/network/http.hpp"

const auto r = termchart::network::http::get("https://example.com/data.csv");
if (!r.has_value()) {
    return;  // IoError / ParseError / Unsupported / OutOfRange
}
const std::string& body = r->body;
```

支持：任意 method、自定义头、请求体；响应按 Content-Length / chunked / 读至 EOF
三种规则收体；重定向跟随（默认上限 5 次，303 降 GET）；每请求独立连接
（`Connection: close`，不做连接池）。**不支持**：HTTP/2、Cookie、摘要认证、
请求体流式分块上传。

## TCP 原语

`network::TcpSocket::connect(host, port, options)`：不抛异常，超时显式实现
（非阻塞 connect + select/poll + 恢复阻塞），单次读/写超时经 SO_RCVTIMEO/
SO_SNDTIMEO。域名解析在 connect 内完成（进程级 TTL 缓存）。

## TLS

https 走 `network/tls` 注册表的后端（Windows 为 SChannel 原生实现）。证书链与
主机名**手动验证**（`CertGetCertificateChain` + `CertVerifyCertificateChainPolicy`），
失败即握手失败，绝不接受“链错了但握手过了”的连接。

```cpp
// 后端可用性自检：不联网，只验证本机 SChannel 能否取得客户端凭据。
const bool tls_ok = termchart::network::tls::schannel::credentials_available();
```

## 代理（Clash / Cloudflare WARP 场景）

**默认自动探测，零配置**。探测链（显式 > 环境变量 > 系统设置 > 直连）：

1. `HttpOptions::proxy` 显式字符串——最高优先，配了连不上**报错不回落**；
2. 环境变量：https 目标依次 `HTTPS_PROXY` → `ALL_PROXY`；http 目标依次
   `HTTP_PROXY` → `ALL_PROXY`（curl 语义，不交叉）；`NO_PROXY` 命中目标则跳过；
3. 系统代理：Windows 注册表（IE/WinINET 配置，Clash 类默认就写这里，含
   `ProxyOverride` 绕过）/ macOS SystemConfiguration；
4. 都没有 → 直连。

探测来的代理不可用 → warn 后**回落直连**（代理是可选加速路径；curl/requests
同款行为）。

```cpp
termchart::network::http::HttpOptions opts;
opts.proxy = "socks5://127.0.0.1:7890";  // 显式：支持 http:// 与 socks5://
const auto r = termchart::network::http::get(url, opts);
```

| 代理软件 | 配置 |
| --- | --- |
| Clash / ClashX / Clash Verge / Mihomo | 开“系统代理”即自动走（mixed 端口 7890）；也可显式 `http://` 或 `socks5://127.0.0.1:7890` |
| Cloudflare WARP（TUN 模式） | 无需配置：WARP 在系统网络栈层接管，库的 TCP 连接自然走隧道 |
| Cloudflare WARP（proxy 模式） | `warp-cli proxy on` 后显式 `socks5://127.0.0.1:40000` |

实现细节：HTTP CONNECT 隧道 + SOCKS5（含 RFC 1929 用户名密码）；SOCKS5 用
ATYP=03 域名，**DNS 交代理解析**（分流规则生效与防 DNS 污染的关键）；隧道对上层
透明——TLS 握手原样跑，SNI 与证书校验仍是最终目标 host。

## 已知边界

- 代理的摘要/协商类认证未做（CONNECT 遇 407 → fail-loud `Unsupported`）；
- SOCKS4 明确拒绝；`HttpOptions::tls_backend` 显式指定不存在的后端 → `Unsupported`；
- 网络阻断（如部分域名在本机被 SNI 级 RST）属网络层问题——同环境 curl 同样失败，
  非库缺陷，换可达镜像或配代理即可。
