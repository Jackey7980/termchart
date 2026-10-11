// capability_query.hpp —— DA1 / DA2 主动探测
//
// 环境变量（见 term/caps.hpp）只能"猜"，DA 是终端**自报**的能力，二者互补：
//   DA1  ESC [ c     → 终端回 ESC [ ? Ps ; … c
//   DA2  ESC [ > c   → 终端回 ESC [ > Ps ; … c
//
// 本模块只做两件事：生成查询序列、解析回复。真正的读写终端由 term/terminal 或
// app/ 负责——保持这里无 I/O，单测才能确定性地验证解析（含畸形输入）。
#ifndef TERMCHART_TERM_CAPABILITY_QUERY_HPP
#define TERMCHART_TERM_CAPABILITY_QUERY_HPP

#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::term {

struct DeviceAttributes {
    bool valid = false;
    bool secondary = false;   // 回复来自 DA2（'>' 前缀）
    int device_class = 0;     // 首个参数
    std::vector<int> params;  // 其余参数（不含 device_class）
};

// 需要写入终端的查询序列。
[[nodiscard]] TERMCHART_API std::string query_da1();
[[nodiscard]] TERMCHART_API std::string query_da2();

// 解析终端回复。容忍前置 ESC、大小写无关的终止符 'c'/'C'；
// 结构不合法（缺 '['、无参数、无终止符、出现非数字）时返回 false 且不改动 out。
[[nodiscard]] TERMCHART_API bool parse_device_attributes(std::string_view reply,
                                                         DeviceAttributes& out) noexcept;

// DA1 里对渲染有意义的几位（编号取自 xterm 的 ctlseqs）。
struct Da1Features {
    bool sixel = false;            // 4
    bool window_ops = false;       // 18
    bool ansi_color = false;       // 22
    bool rectangular_ops = false;  // 28
};

[[nodiscard]] TERMCHART_API Da1Features da1_features(
    const DeviceAttributes& attributes) noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_CAPABILITY_QUERY_HPP
