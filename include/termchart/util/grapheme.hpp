// grapheme.hpp —— grapheme cluster 边界检测（UAX #29 简化版）
//
// 这里是**唯一做分割**的地方（core/grapheme.hpp 只定义数据结构）。
// 属性区间表来自 detail/grapheme_tables.hpp（层号 -1，任何层可用）。
//
// 实现的规则集：GB3（CR×LF）、GB4/GB5（控制符自成簇）、GB6–GB8（Hangul）、
// GB9（×Extend/ZWJ）、GB9a（×SpacingMark）、GB9b（Prepend×）、
// GB11（ZWJ×扩展象形）、GB12/GB13（地区旗标成对连缀）。
// 未实现 emoji 序列的全部细则（UAX #51 的杂项键帽等）——见 ROADMAP 阶段 7。
#ifndef TERMCHART_UTIL_GRAPHEME_HPP
#define TERMCHART_UTIL_GRAPHEME_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "termchart/core/grapheme.hpp"
#include "termchart/export.hpp"

namespace termchart::util {

// 从 pos 走到下一个簇的边界（返回增量；0 表示已到末尾）。
TERMCHART_API std::size_t next_boundary(std::string_view text, std::size_t pos) noexcept;

// 簇计数：文本 = 多少个"用户感知字符"。
TERMCHART_API std::size_t cluster_count(std::string_view text) noexcept;

// 全量分割。热路径请用 next_boundary 流式遍历，避免中间容器。
TERMCHART_API std::vector<GraphemeCluster> split_clusters(std::string_view text);

// 是否为"扩展象形"（emoji 粗判）：GB11 需要它。
TERMCHART_API bool is_extended_pictographic(char32_t cp) noexcept;

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_GRAPHEME_HPP
