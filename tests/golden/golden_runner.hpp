// golden_runner.hpp —— ansi_props 快照框架
//
// 为什么"解析成属性再比对"而不是比字节：
// 字节级 golden 会把**任何**无关紧要的实现选择（例如先发 fg 还是先发 bg、
// 用 CSI H 还是回车换行）都固化成契约，导致每次优化都要重刷快照——
// 那样的快照不提供保护，只提供噪声。
// 因此：把渲染输出解析回"第 r 行第 c 列是什么字符、什么前景/背景/属性"，
// 比对这些**属性**，字节顺序自由。
#ifndef TERMCHART_TESTS_GOLDEN_RUNNER_HPP
#define TERMCHART_TESTS_GOLDEN_RUNNER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace golden {

struct CellProps {
    int row = 0;
    int col = 0;
    std::uint32_t codepoint = 0x20;
    std::string fg;     // "none" / "ansi16:3" / "indexed:200" / "#rrggbb"
    std::string bg;
    std::string attrs;  // "bold|underline" 或 ""（按位序输出，稳定）

    bool operator==(const CellProps&) const = default;
};

struct Props {
    int width = 0;
    int height = 0;
    std::vector<CellProps> cells;

    bool operator==(const Props&) const = default;
};

// 解析渲染字节流 → 属性集合。只处理本库会发出的那几种序列：
// CSI…m（样式）、CSI r;cH（绝对定位）、CR、LF，以及 UTF-8 文本。
Props parse_ansi(std::string_view bytes);

// 稳定文本形式（与具体终端、平台、locale 无关）。
std::string dump(const Props& props);

// 与 tests/golden/<subdir>/<name>.txt 比对；
// 环境变量 TERMCHART_UPDATE_GOLDEN=1 时改为写入（由 tools/gen_golden.py 触发）。
void check_or_update(const std::string& subdir, const std::string& name,
                     const std::string& actual);

}  // namespace golden

#endif  // TERMCHART_TESTS_GOLDEN_RUNNER_HPP
