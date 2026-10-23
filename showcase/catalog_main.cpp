// showcase/catalog_main.cpp —— 全功能 catalog 主程序
//
// 把 charts/graphs/systems 三区登记的全部 Demo 按类别汇成一张可滚动长目录：
// 每个面板 40×14，每行 3 个；单缓冲合成后一次 render_full 出 ANSI。
// 用法：
//   catalog_tui                 打印全目录（确定性，CI 可重定向存文件）
//   catalog_tui --only 金融      只打印某类别（前缀匹配）
//   catalog_tui --list           只列出类别与计数
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "termchart/termchart_full.hpp"
#include "termchart/api/print.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/render/renderer.hpp"

#include "catalog.hpp"

namespace cat {
namespace {

constexpr std::uint32_t PW = 46;
constexpr std::uint32_t PH = 20;
constexpr std::uint32_t PER = 3;

void blit(CellBuffer& dst, const CellBuffer& src, std::uint32_t dx, std::uint32_t dy) {
    for (std::uint32_t y = 0; y < src.height(); ++y) {
        for (std::uint32_t x = 0; x < src.width(); ++x) {
            const tc::Cell& c = src.at(x, y);
            const bool blank = c.cp == U' ' && c.style.fg().is_none() && c.style.bg().is_none();
            if (blank) continue;
            if (dst.contains(dx + x, dy + y)) dst.at(dx + x, dy + y) = c;
        }
    }
}

}  // namespace
}  // namespace cat

int main(int argc, char** argv) {
    using namespace cat;
    std::string only;
    int first = 0;
    bool list_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--only" && i + 1 < argc) {
            only = argv[++i];
        } else if (a == "--first" && i + 1 < argc) {
            first = std::atoi(argv[++i]);
        } else if (a == "--list") {
            list_only = true;
        } else if (a == "--help" || a == "-h") {
            std::fprintf(stderr, "usage: catalog_tui [--only CAT] [--list]\n");
            return 0;
        }
    }

    std::vector<Demo> demos;
    register_charts(demos);
    register_graphs(demos);
    register_systems(demos);

    if (first > 0 && static_cast<std::size_t>(first) < demos.size()) {
        demos.resize(static_cast<std::size_t>(first));  // 调试：只渲染前 N 个
    }

    // 按类别有序分组
    std::vector<std::string> cats;
    std::vector<std::vector<const Demo*>> groups;
    for (const Demo& dm : demos) {
        if (!only.empty() && dm.category.find(only) == std::string::npos) continue;
        std::size_t idx = groups.size();
        for (std::size_t k = 0; k < cats.size(); ++k) {
            if (cats[k] == dm.category) {
                idx = k;
                break;
            }
        }
        if (idx == groups.size()) {
            cats.push_back(dm.category);
            groups.emplace_back();
        }
        groups[idx].push_back(&dm);
    }

    if (list_only) {
        for (std::size_t k = 0; k < cats.size(); ++k) {
            std::printf("%s (%zu)\n", cats[k].c_str(), groups[k].size());
        }
        std::printf("total %zu demos / %zu categories\n", demos.size(), cats.size());
        return 0;
    }

    // 计算总高度
    std::uint32_t total_h = 0;
    for (const auto& g : groups) {
        const std::uint32_t rows = static_cast<std::uint32_t>((g.size() + PER - 1) / PER);
        total_h += 1 + rows * PH + 1;  // 标题行 + 面板行 + 间隔
    }
    if (total_h == 0) total_h = 1;

    CellBuffer base(PER * PW, total_h);
    tc::Style hdr;
    hdr.set_fg(tc::Color::rgb(120, 210, 255)).set_attr(tc::Attr::Bold);
    std::uint32_t cur_y = 0;
    for (std::size_t k = 0; k < cats.size(); ++k) {
        draw_text(base, 0, cur_y, "■ " + cats[k] + "  (" + std::to_string(groups[k].size()) + ")", hdr);
        ++cur_y;
        const auto& g = groups[k];
        for (std::size_t i = 0; i < g.size(); ++i) {
            const std::uint32_t col = static_cast<std::uint32_t>(i % PER);
            const std::uint32_t row = static_cast<std::uint32_t>(i / PER);
            CellBuffer p = g[i]->render(PW, PH);
            blit(base, p, col * PW, cur_y + row * PH);
        }
        cur_y += static_cast<std::uint32_t>((g.size() + PER - 1) / PER) * PH + 1;
    }

    tc::render::Renderer r(tc::ColorDepth::TrueColor);
    const std::string out = r.render_full(base);
    tc::api::write_stdout(out);
    tc::api::flush_stdout();
    std::fprintf(stderr, "catalog: %zu demos / %zu categories -> %ux%u\n", demos.size(), cats.size(),
                 base.width(), base.height());
    return 0;
}
