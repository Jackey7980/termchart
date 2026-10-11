// wcwidth.hpp —— 显示宽度
//
// 宽度模型：**主码点决定宽度，组合记号 (side bearing) 宽 0**（详情见
// docs/design/utf8_handling.md）。因此宽度查询的输入可以是码点或
// GraphemeCluster，二者语义一致。emoji 宽度策略见 docs/design/emoji_width.md。
#ifndef TERMCHART_UTIL_WCWIDTH_HPP
#define TERMCHART_UTIL_WCWIDTH_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/grapheme.hpp"
#include "termchart/export.hpp"

namespace termchart::util {

struct WidthOptions {
    // East Asian Ambiguous 是否按宽处理（CJK 区域终端常见需求）。
    bool ambiguous_wide = false;
    // emoji 是否按 2 宽处理；某些终端（含部分 Windows 控制台）只给 1 宽。
    bool emoji_wide = true;
};

// 单个码点宽度：0（组合/格式/控制）、1（窄）、2（东亚宽 / emoji）。
TERMCHART_API int codepoint_width(char32_t cp, const WidthOptions& options = {}) noexcept;

// 一个簇的宽度：等于主码点宽度（侧面宽 0）。
TERMCHART_API int cluster_width(const GraphemeCluster& cluster,
                                const WidthOptions& options = {}) noexcept;

// 整串宽度：以簇为单位累加；非法 UTF-8 的替换字符按宽度 1 计。
TERMCHART_API int display_width(std::string_view text, const WidthOptions& options = {}) noexcept;

// 按显示宽度截断：返回"最多个前缀码点"，使宽度不超过 budget。
// 工具核心：**绝不切在组合记号中间**（否则会出现孤立的组合记号，渲染为乱码）。
TERMCHART_API std::size_t prefix_by_width(std::string_view text, int budget,
                                          const WidthOptions& options = {}) noexcept;

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_WCWIDTH_HPP
