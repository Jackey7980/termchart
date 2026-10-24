// api/autoviz_plot.hpp —— AutoViz 一行出图薄壳（L21）
//
// 定位（对应"新手默认路径零配置"）：给一个文件路径，引擎自动清洗、探测、
// 选图、写到 stdout。本函数是**薄壳**——不猜列、不猜图种、不写业务判断；
// 选图是 autoviz 引擎（L21）的事，出图是 print_chart 的事。
//
// 边界（与 autoviz 引擎同口径，显式列出避免"以为支持"）：
//   * 仅表头（0 数据行）→ InvalidArgument：没有数据不是可视化问题；
//   * **不自动识别 Unix 时间戳数字串**：1700000000 与 18 亿访问量/ID 无法
//     区分，强行识别会把数值列画成时序线。要时序图显式传 opts.time_column；
//   * 格式嗅探只分两家的：首个非空白字符是 '{' 走 JSONL，其余走 CSV；
//   * 画布用 print_chart 默认 80×24（不探测终端——CI/无 tty 也要可复现）；
//   * AutoViz 是**探索性分析**工具：生产环境请显式指定图表类型与列映射
//     （见 docs/guide/autoviz.md）。
#ifndef TERMCHART_API_AUTOVIZ_PLOT_HPP
#define TERMCHART_API_AUTOVIZ_PLOT_HPP

#include <string_view>

#include "termchart/autoviz/options.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::api {

// 读文件 → autoviz 选图 → print_chart 出图（TUI 路径）。
// 返回 Status（core/expected.hpp 约定：只回报成败不用 Result<void>——
// Expected<void,E> 实例化 std::variant<void>，踩 void 形参构造的方言容忍度，
// 不跨编译器可靠；见 network/tls/backend.hpp 同款注释）。
[[nodiscard]] TERMCHART_API Status auto_plot(std::string_view filepath,
                                             const autoviz::AutoVizOptions& opts = {});

}  // namespace termchart::api

#endif  // TERMCHART_API_AUTOVIZ_PLOT_HPP
