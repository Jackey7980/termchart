// dns_cache.hpp —— 进程级 DNS 解析缓存（**仅 src/network 内可见**）
//
// 平台无关的"中立 sockaddr"格式（family + 原始字节），POSIX 与 Win32 共用；
// 与 platform_internal.hpp 同纪律：不放 include/。
//
// 为什么需要：layer 布局与 app 拉数据的场景是"一个 host 连多次"（重定向 2-3 跳，
// 或多轮刷新），每次 getaddrinfo 都是几十毫秒的阻塞 DNS 往返。缓存把它降到
// 建 socket 的直接成本。
//
// 安全注记：TTL（60s）意味着 DNS rebinding 有最长 60s 的窗口。图表库拉取公开
// 数据的场景可接受；https 场景另有后端层面的证书主机名验证兜底。若上层需要
// 强新鲜度，应自行在连接层绕过（本缓存只服务本进程的 connect 路径）。
#ifndef TERMCHART_SRC_NETWORK_DNS_CACHE_HPP
#define TERMCHART_SRC_NETWORK_DNS_CACHE_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace termchart::network::platform {

// 中立 sockaddr 条目：(address family, sockaddr 原始字节)。
using NeutralAddr = std::pair<int, std::vector<unsigned char>>;

// 查缓存；未命中/已过期返回空 vector。
[[nodiscard]] std::vector<NeutralAddr> dns_cache_get(const std::string& host,
                                                     std::uint16_t port) noexcept;

// 写缓存（空 addrs 忽略；超容量时先清过期项，仍满则清表——缓存是优化，
// 不是正确性依赖，任何情况下都不得让 put 失败影响连接本身）。
void dns_cache_put(const std::string& host, std::uint16_t port,
                   std::vector<NeutralAddr> addrs) noexcept;

}  // namespace termchart::network::platform

#endif  // TERMCHART_SRC_NETWORK_DNS_CACHE_HPP
