// fuzz_input_parser.cpp —— 终端输入字节流的流式解析（规格 fuzz 目标之一）
//
// 关注点：解析器要在"序列被任意切开"的情况下保持正确 —— 鼠标流、CSI、OSC 都可能
// 跨 read() 边界。因此这里**按伪随机位置切块**喂进去，而不是一次性喂整串；
// 这正是一次性 fuzz 最容易漏掉的路径（缓冲区的拼接与超长未完成序列）。
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "fuzz_common.hpp"
#include "termchart/interaction/input_parser.hpp"

namespace {

void target(const std::uint8_t* data, std::size_t size) {
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    termchart::interaction::InputParser parser;
    fuzz::Rng rng(static_cast<std::uint64_t>(size) * 0x9E3779B97F4A7C15ull + 1u);

    std::size_t pos = 0;
    while (pos < input.size()) {
        // 块长 1..8：故意切在 CSI/OSC 中间，逼出"未完成序列"的缓冲逻辑。
        const std::size_t chunk = 1u + rng.below(8);
        const std::size_t end = std::min(input.size(), pos + chunk);
        (void)parser.feed(input.substr(pos, end - pos));
        (void)parser.pending();
        pos = end;
    }

    (void)parser.flush();
    (void)parser.pending();
    parser.reset();

    // 再来一次"整串喂 + 立即 flush"的路径，覆盖另一条分支。
    (void)parser.feed(input);
    (void)parser.flush();
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
