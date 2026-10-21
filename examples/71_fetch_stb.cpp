// 71_fetch_stb.cpp —— 用 termchart 自己的 network 后端拉取 vendored STB 头文件
//
// 目的：演示 network 模块（http + schannel）在真实外网下的可用性，同时把
// image 模块扩容 JPEG/GIF/WebP 所需的 stb_image / stb_image_write 拉进仓库
// （third_party/stb/）。零强制依赖：默认只打印用法，**不联网**；带 --fetch 才
// 真正下载并落盘。
//
// 多镜像：两个候选域名（jsdelivr / raw.githubusercontent）在部分网络环境
// （如中国大陆的 SNI 阻断）下 TLS 会被 RST——那是网络层的事，不是库缺陷
// （2026-10-21 实证：同环境 curl/PowerShell 同样失败，而 example.com 正常）。
// 故每个文件给一组候选 URL，取第一个返回 200 的；这一层容错是 net 后端的
// "调用方责任"，库本身只对"已登记但握手不过"的 TLS 后端 fail-loud，
// 不做静默降级。受阻环境可自行换可达镜像或本地文件。
//
// 运行（从仓库根目录，保证相对路径 third_party/stb/ 可达）：
//   ./build/<preset>/examples/71_fetch_stb            # 离线，只打印用法
//   ./build/<preset>/examples/71_fetch_stb --fetch    # 联网拉取并写盘

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/network/http.hpp"

namespace net = termchart::network::http;

// 每个待拉取文件 + 一组候选镜像（按顺序尝试，第一个 200 即采用）。
struct FetchItem {
    std::string out;                       // 相对仓库根的输出路径
    std::vector<std::string> candidates;   // 候选 URL（jsdelivr 在前，raw 在后）
};

static const std::vector<FetchItem> kItems = {
    {"third_party/stb/stb_image.h",
     {"https://cdn.jsdelivr.net/gh/nothings/stb/stb_image.h",
      "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"}},
    {"third_party/stb/stb_image_write.h",
     {"https://cdn.jsdelivr.net/gh/nothings/stb/stb_image_write.h",
      "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"}},
};

// 原子写：先写临时文件再 rename，避免拉取到一半被杀留下半截文件。
static bool write_atomic(const std::string& path, const std::string& data) {
    const std::filesystem::path p(path);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
        std::fprintf(stderr, "  mkdir 失败 %s: %s\n", path.c_str(), ec.message().c_str());
        return false;
    }
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::fprintf(stderr, "  打开临时文件失败 %s\n", tmp.c_str());
            return false;
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.close();
        if (!out) {
            std::fprintf(stderr, "  写临时文件失败 %s\n", tmp.c_str());
            return false;
        }
    }
    std::filesystem::rename(tmp, p, ec);
    if (ec) {
        std::fprintf(stderr, "  rename 失败 %s: %s\n", path.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    const std::string_view cmd = argc > 1 ? std::string_view(argv[1]) : "";
    if (cmd == "--probe") {
        // 连通性探针：确认 schannel 后端能否对外网做 TLS 握手。
        const char* hosts[] = {"https://example.com",
                               "https://cdn.jsdelivr.net/gh/nothings/stb/stb_image.h",
                               "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"};
        net::HttpOptions o;
        o.socket.connect_timeout_ms = 8000;
        o.socket.io_timeout_ms = 20000;
        for (const char* h : hosts) {
            auto r = net::get(h, o);
            if (r.has_value()) {
                std::printf("%s -> HTTP %u (%zu 字节)\n", h, r->status, r->body.size());
            } else {
                std::printf("%s -> 失败（%s）\n", h, termchart::to_string(r.error()));
            }
        }
        return 0;
    }

    const bool fetch = (cmd == "--fetch");
    if (!fetch) {
        std::printf("fetch_stb: 用 termchart network 后端拉取 vendored STB（image 扩容用）。\n");
        std::printf("用法:\n");
        std::printf("  %s --probe   联网连通性探针（确认 schannel 能否握手外网）\n",
                    argc > 0 ? argv[0] : "71_fetch_stb");
        std::printf("  %s --fetch   真正下载并写盘（需从仓库根目录运行）\n",
                    argc > 0 ? argv[0] : "71_fetch_stb");
        return 0;
    }

    net::HttpOptions opts;
    opts.socket.connect_timeout_ms = 8000;
    opts.socket.io_timeout_ms = 20000;
    opts.max_body_bytes = 8u * 1024u * 1024u;  // stb_image.h ~ 280KB，留足余量

    int failed = 0;
    for (const auto& item : kItems) {
        bool ok = false;
        for (const auto& url : item.candidates) {
            std::printf("GET %s\n", url.c_str());
            auto resp = net::get(url, opts);
            if (!resp.has_value()) {
                std::fprintf(stderr, "  -> 失败（%s），试下一个镜像\n",
                             termchart::to_string(resp.error()));
                continue;
            }
            if (resp->status != 200) {
                std::fprintf(stderr,
                             "  -> HTTP %u %s；Location=%s，试下一个镜像\n",
                             resp->status, resp->reason.c_str(),
                             std::string(resp->header("Location")).c_str());
                continue;
            }
            if (write_atomic(item.out, resp->body)) {
                std::printf("  -> OK  写入 %s (%zu 字节)\n", item.out.c_str(),
                            resp->body.size());
                ok = true;
                break;
            }
            std::fprintf(stderr, "  -> 写盘失败，试下一个镜像\n");
        }
        if (!ok) {
            std::fprintf(stderr, "拉取 %s 全部镜像失败\n", item.out.c_str());
            ++failed;
        }
    }

    if (failed == 0) {
        std::printf("全部 STB 头已就位，可开启 TERMCHART_ENABLE_STB_IMAGE 接入解码。\n");
    }
    return failed == 0 ? 0 : 1;
}
