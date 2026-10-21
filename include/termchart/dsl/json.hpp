// dsl/json.hpp —— JSON 解析入口（compat shim，L17）
//
// 实现已于 2026-10-21 下沉收编至 io/json.hpp（L2，"数据入口归 io"）。本头保留
// 原 dsl 命名空间的入口语义：dsl::JsonValue / dsl::JsonOptions / dsl::parse_json
// 均为 io 命名空间对应符号的别名，既有调用点与测试零改动。
// 新代码请直接 include "termchart/io/json.hpp"。
#ifndef TERMCHART_DSL_JSON_HPP
#define TERMCHART_DSL_JSON_HPP

#include "termchart/io/json.hpp"

namespace termchart::dsl {

using termchart::io::JsonValue;
using termchart::io::JsonOptions;
using termchart::io::ParseDiagnostic;
using termchart::io::parse_json;

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_JSON_HPP
