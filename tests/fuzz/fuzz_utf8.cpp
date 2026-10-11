// fuzz_utf8.cpp —— UTF-8 解码/编码/校验与显示宽度的边界（规格 fuzz 目标之一）
//
// 关注点：**任意字节流下不得越界**。UTF-8 解析器最容易在"截断的多字节序列"
// （例如 4 字节前导后只跟 1 字节）与"过长编码/代理区码点"上出错，所以这里除了
// 整体校验，还用**每个位置**都试一遍的方式压 decode/next_boundary 的边界。
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fuzz_common.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

void target(const std::uint8_t* data, std::size_t size) {
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    (void)termchart::util::is_valid(input);
    (void)termchart::util::codepoint_count(input);
    (void)termchart::util::sanitize(input);
    (void)termchart::util::display_width(input);
    (void)termchart::util::display_width(input, termchart::util::WidthOptions{});

    // 逐位置解码：任何 pos（含"多字节序列中间"与"末尾之后"）都必须安全。
    for (std::size_t pos = 0; pos <= input.size(); ++pos) {
        (void)termchart::util::decode(input, pos);
    }

    // 宽字符截断：预算取各种边界值（负数、0、恰好等于整串宽度、超大）。
    const int width = termchart::util::display_width(input);
    for (const int budget : {-1, 0, 1, width - 1, width, width + 1, 1 << 20}) {
        (void)termchart::util::prefix_by_width(input, budget);
    }

    // 编码：把输入字节拼成的 21 位值塞进编码器（含超出 Unicode 范围的值），
    // 缓冲区只有 4 字节 —— 编码器必须自己不越界。
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint32_t low = data[i];
        const std::uint32_t high = (i + 1 < size) ? data[i + 1] : 0;
        const char32_t cp = static_cast<char32_t>(((high << 8) | low) & 0x1FFFFFu);
        char buffer[4] = {0, 0, 0, 0};
        (void)termchart::util::encode(cp, buffer);
    }
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
