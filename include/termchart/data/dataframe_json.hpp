// data/dataframe_json.hpp —— JSON/JSONL → DataFrame（保 Key 入口，L4）
//
// 对应 AutoViz 规格书阶段一"双轨制解析"的 JSON 轨：
//   * **绝对保留原始 Key**：列序 = 首个对象的键插入序（io/json 的对象保序），
//     其后对象的新键按出现顺序追加（并集），绝不重命名、绝不按字母序重排；
//   * 嵌套对象/数组展平为**点路径**（loc.lat、tags.0）——键名不丢失，只是
//     换了一种可列名化的写法；
//   * 缺失键 → Empty 单元格（不编造、不错位、不用上一行的值填充）；
//   * 单行解析失败默认跳过并计数（脏数据流不该因一行畸形全表报废），
//     显式要 fail-loud 的调用方把 skip_invalid_lines 置 false。
//   * 本阶段**不计算任何统计量**——输出的 DataFrame 只是轻底座，画像/路由
//     是 column_profile 与 autoviz 的事。
//
// 为什么住在 data（L4）而不是 io（L2）：产物是 DataFrame（L4 类型），io 不
// 许依赖上层；io/json 只提供 DOM 与 keys()，装配在这里做。
#ifndef TERMCHART_DATA_DATAFRAME_JSON_HPP
#define TERMCHART_DATA_DATAFRAME_JSON_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/data/dataframe.hpp"

namespace termchart::data {

struct JsonFrameOptions {
    bool skip_invalid_lines = true;  // JSONL 单行失败/非记录行：跳过并计数，而非整体失败
    std::size_t max_lines = 0;       // 0 = 不限
};

struct JsonFrameResult {
    DataFrame frame;
    std::size_t lines_total = 0;    // 非空行总数（空白行不计）
    std::size_t lines_used = 0;     // 产出行的行数
    std::size_t lines_skipped = 0;  // 跳过的行数（skip_invalid_lines=true 时才有意义）
    // 其中有多少行是**位置式数组**按列序 zip 进来的（见 add_positional）。
    // 单列出来是因为这类行的语义最弱：它靠"宽度恰好相同"才成立，值本身没有键。
    std::size_t lines_zipped = 0;
};

// JSONL（newline-delimited JSON，每行一个对象）→ DataFrame。
// 顶层是对象的行按记录处理；顶层是**元素全为对象的数组**的行，其元素按多条
// 记录处理；
// 顶层是**标量数组**的行，若其长度恰好等于当前列数，按列序 zip 成一行
// （lines_zipped 计数）——救回"有值没键"的位置式记录；长度不同则一样跳过：
// 宽度对不上的数组强行对齐会把 "X004" 塞进 ts 列，那是编数据，比丢弃更坏。
// 其余（标量、空数组、其余混合数组）算非记录行，跳过并计数。
[[nodiscard]] Result<JsonFrameResult> dataframe_from_jsonl(
    std::string_view source, const JsonFrameOptions& options = {});

// 单个 JSON 值 → DataFrame：
//   * 数组且元素全为对象 → 多行（等价于把数组当 JSONL 展开）；
//   * 对象 → 单行；
//   * 其余 → InvalidArgument（调用方给的不是记录型 JSON）。
[[nodiscard]] Result<JsonFrameResult> dataframe_from_json(
    std::string_view source, const JsonFrameOptions& options = {});

}  // namespace termchart::data

#endif  // TERMCHART_DATA_DATAFRAME_JSON_HPP
