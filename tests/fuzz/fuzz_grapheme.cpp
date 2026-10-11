// fuzz_grapheme.cpp —— 字素簇切分与簇宽度的边界（规格 fuzz 目标之一）
//
// 关注点：簇边界算法要处理组合记号、ZWJ、区域指示符、扩展象形文字等；
// fuzz 时**任意位置**的 next_boundary 都不能越界，簇宽度也不能读到簇外。
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fuzz_common.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

void target(const std::uint8_t* data, std::size_t size) {
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    (void)termchart::util::cluster_count(input);

    // 任意位置取下一个簇边界（含末尾之后）。
    for (std::size_t pos = 0; pos <= input.size(); ++pos) {
        (void)termchart::util::next_boundary(input, pos);
    }

    // 每个码点都问一次"是否扩展象形文字"（该判定会影响 emoji 簇的宽度）。
    for (std::size_t i = 0; i < size; ++i) {
        (void)termchart::util::is_extended_pictographic(static_cast<char32_t>(data[i]));
    }

    // 簇宽度：对切分出来的每个簇求宽（含空簇与超长簇）。
    const auto clusters = termchart::util::split_clusters(input);
    for (const auto& cluster : clusters) {
        (void)termchart::util::cluster_width(cluster);
        (void)termchart::util::cluster_width(cluster, termchart::util::WidthOptions{});
    }
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
