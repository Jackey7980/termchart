// d2.hpp —— D2 语法（L17）
//
// ✅ 支持
//   `name: Type` 节点声明（`Type` 映射到图元形状，原值记进节点属性 `d2_shape`）
//   `name: { … }` 块：容器节点（属性 `container=true`），块内子节点记 `parent`
//   `label: 文字` / `shape: 圆` / `width|height|near|tooltip|icon|link|classes: v`
//       —— 作用于当前块（或顶层最近声明的节点）
//   `name.prop: value` 形式（同上，但显式指定节点）
//   连接：`a -> b`、`a <- b`、`a -- b`（无箭头）、链 `a -> b -> c`、`a -> b: 标签`
//   `direction: right|left|up|down` → AstGraph::direction（LR/RL/BT/TB）
// ⛔ 认出但忽略：`style: { … }` 块（连同其 `}`）、`#` 注释、未知键值对
// ⛔ 不支持：`...` 常量节点、泛型 `**a**` 强调语法、SQL/class/markdown 等嵌入块、
//   `a -> b: { label: x }` 这种"边上的属性块"（按普通边解析，标签取不到）
//
// 与 graphviz_dot 的关系：两者都是"文本 → 图"，但 D2 的**默认形状语义**不同
// （`name: Type` 是形状声明而不是属性），因此不复用 DOT 的解析器。
#ifndef TERMCHART_DSL_D2_HPP
#define TERMCHART_DSL_D2_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct D2ParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_d2(std::string_view source,
                                                     const D2ParseOptions& options = {},
                                                     ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kD2Directive = "d2";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_D2_HPP
