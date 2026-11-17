// bench_diff.cpp —— 差分重绘基线
//
// 度量 `render::diff`（两个 CellBuffer 逐行比较 → 变化段）在典型终端尺寸下的
// 耗时，分三档变化率：全同（无段）、单行改动、全屏改动。关注点：diff 是 O(格数)
// 且"全同"应最快（无变化段分配）。
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bench.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/render/diff.hpp"

namespace {

using namespace termchart;

CellBuffer make_buffer(std::uint32_t w, std::uint32_t h, char32_t cp) {
    CellBuffer buffer(w, h);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            buffer.at(x, y) = Cell(cp);
        }
    }
    return buffer;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes =
        quick ? std::vector<std::pair<std::uint32_t, std::uint32_t>>{{80u, 24u}}
              : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
                    {80u, 24u}, {120u, 40u}, {200u, 60u}};
    const int rounds = quick ? 3 : 15;

    bench::header("bench_diff", "render::diff（CellBuffer 差分重绘）");
    for (const auto& s : sizes) {
        const CellBuffer base = make_buffer(s.first, s.second, U'a');
        const std::string size = std::to_string(s.first) + "x" + std::to_string(s.second);

        // 全同：无变化段。
        {
            const double ns = bench::median_ns(
                [&] { (void)render::diff(base, base); }, rounds);
            bench::report("diff/identical", size, ns);
        }
        // 单行改动。
        {
            CellBuffer one_row = base;
            for (std::uint32_t x = 0; x < s.first; ++x) {
                one_row.at(x, 0) = Cell(U'b');
            }
            const double ns = bench::median_ns(
                [&] { (void)render::diff(base, one_row); }, rounds);
            bench::report("diff/one-row", size, ns);
        }
        // 全屏改动。
        {
            const CellBuffer all_changed = make_buffer(s.first, s.second, U'z');
            const double ns = bench::median_ns(
                [&] { (void)render::diff(base, all_changed); }, rounds);
            bench::report("diff/full", size, ns);
        }
    }
    return 0;
}
