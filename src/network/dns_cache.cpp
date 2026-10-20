// dns_cache.cpp —— DNS 缓存实现（平台无关；无 #ifdef）
#include "src/network/dns_cache.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace termchart::network::platform {
namespace {

constexpr std::size_t kMaxEntries = 64;   // 有界：防病态 host 洪水撑爆
constexpr std::uint64_t kTtlMs = 60000;   // 60s TTL（见头文件安全注记）

struct CacheLine {
    std::vector<NeutralAddr> addrs;
    std::uint64_t expires_ms = 0;
};

std::mutex& cache_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::string, CacheLine>& cache() {
    static std::map<std::string, CacheLine> lines;
    return lines;
}

std::string key_of(const std::string& host, std::uint16_t port) {
    std::string key = host;
    key += ':';
    key += std::to_string(port);
    return key;
}

// steady_clock 毫秒：单调，不怕系统时钟跳变。
std::uint64_t now_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

std::vector<NeutralAddr> dns_cache_get(const std::string& host,
                                       std::uint16_t port) noexcept {
    const std::string key = key_of(host, port);
    std::lock_guard<std::mutex> lock(cache_mutex());
    auto& lines = cache();
    const auto it = lines.find(key);
    if (it == lines.end()) {
        return {};
    }
    if (now_ms() >= it->second.expires_ms) {
        lines.erase(it);
        return {};
    }
    return it->second.addrs;  // 拷贝：调用方拿到即可直接用，无生命周期耦合
}

void dns_cache_put(const std::string& host, std::uint16_t port,
                   std::vector<NeutralAddr> addrs) noexcept {
    if (addrs.empty()) {
        return;
    }
    const std::string key = key_of(host, port);
    std::lock_guard<std::mutex> lock(cache_mutex());
    auto& lines = cache();
    if (lines.size() >= kMaxEntries) {
        const std::uint64_t now = now_ms();
        for (auto it = lines.begin(); it != lines.end();) {
            it = now >= it->second.expires_ms ? lines.erase(it) : std::next(it);
        }
        if (lines.size() >= kMaxEntries) {
            lines.clear();  // 全是热条目：整表让位（缓存语义允许）
        }
    }
    CacheLine line;
    line.addrs = std::move(addrs);
    line.expires_ms = now_ms() + kTtlMs;
    lines[key] = std::move(line);
}

}  // namespace termchart::network::platform
