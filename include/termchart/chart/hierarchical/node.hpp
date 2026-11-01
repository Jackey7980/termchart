// node.hpp —— hierarchical/ 共享的树节点与布局辅助（自给，不依赖 graph/）
//
// chart 是 L12、graph 是 L13，`chart > graph` 是禁止边（cmake/TermchartModules.cmake），
// 因此层次图的数据输入（HierNode）与"聚合 / 配色 / 首码点"等小工具都在这里自给，
// 不 include graph/ 的任何布局器。规格表（结构.md 附二）只列了 6 个图种头，
// 本文件是为避免 6 处重复而增设的子目录内共享头，随 6 个图种一起交付。
#ifndef TERMCHART_CHART_HIERARCHICAL_NODE_HPP
#define TERMCHART_CHART_HIERARCHICAL_NODE_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart::hier {

// 层次图通用输入树：叶子携带度量，内部节点 value<=0 表示由子节点聚合。
struct HierNode {
    std::string name;
    double value = 0.0;
    Color color = Color::none();  // none → 按第一层分支自动配色
    std::vector<HierNode> children{};

    [[nodiscard]] bool is_leaf() const noexcept { return children.empty(); }
};

// 自底向上聚合：叶子取 max(value, 0)，内部节点 value<=0 时覆盖为子树和。
// 返回根的聚合值。
TERMCHART_API double aggregate_values(HierNode& node);

// 确定性默认配色：按分支序号循环取 ANSI 16 色（跳过黑/白，保证可见）。
TERMCHART_API Color hier_palette(std::size_t branch) noexcept;

// 给整棵树补默认色：depth==1 的节点按序取 palette，后代继承所在分支的色。
TERMCHART_API void assign_colors(HierNode& node, std::size_t branch = 0,
                                 std::size_t depth = 0);

// 取字符串首个 UTF-8 码点（非法序列容错），供"一格标签"用；空串返回空格。
TERMCHART_API char32_t first_codepoint(const std::string& text);

}  // namespace termchart::chart::hier

#endif  // TERMCHART_CHART_HIERARCHICAL_NODE_HPP
