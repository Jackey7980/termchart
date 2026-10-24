// autoviz_plot.cpp —— AutoViz 薄壳实现：读文件 → 选图 → 出图
#include "termchart/api/autoviz_plot.hpp"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>

#include "termchart/api/chart.hpp"
#include "termchart/api/print.hpp"
#include "termchart/autoviz/autoviz.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/util/logging.hpp"

namespace termchart::api {
namespace {

// 读整个文件。二进制模式：字节原样交给解析器（UTF-8/脏字节/CRLF 都不在
// 这里猜，解析器的容错是它自己的职责）。
Result<std::string> read_file(std::string_view path) {
    std::ifstream in(std::string(path), std::ios::binary);
    if (!in) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    return Result<std::string>(std::move(content));
}

bool is_blank(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

// 日志里的数值：1e21 用 std::to_string 会打成 21 位数字（读不出来也占满行），
// 3 位有效数字够定位"哪个值离谱"。
std::string fmt_value(double v) {
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "%.3g", v);
    return std::string(buf);
}

}  // namespace

Status auto_plot(std::string_view filepath, const autoviz::AutoVizOptions& opts) {
    if (filepath.empty()) {
        return ErrorCode::InvalidArgument;
    }
    const Result<std::string> source = read_file(filepath);
    if (!source.has_value()) {
        return source.error();
    }

    // 格式嗅探（全函数唯一的"判断"，且只分家的格式）：首个非空白字符是
    // '{' → JSONL（保 Key/展平/脏行跳过）；其余 → CSV（容错解析）。
    // CSV 首格以 '{' 开头的概率远低于误把 JSONL 当 CSV 的代价，故从简。
    std::size_t first = 0;
    while (first < source->size() && is_blank((*source)[first])) {
        ++first;
    }
    const bool jsonl = first < source->size() && (*source)[first] == '{';

    Result<autoviz::AutoVizResult> picked =
        jsonl ? autoviz::auto_visualize_jsonl(*source, opts)
              : autoviz::auto_visualize(data::parse_dataframe(*source), opts);
    if (!picked.has_value()) {
        return picked.error();
    }
    // 表头口径体检的提示出口：走一行出图的用户不会去看 DatasetFeatures，
    // 所以把 suspects 以 WARN 打到日志（默认 stderr；测试可用 capture_log 接管）。
    // **只提示，不参与选图**——修不修数据是人的决定。
    for (const autoviz::ColumnSuspect& s : picked->features.suspects) {
        TERMCHART_LOG_WARN("autoviz: 列名口径存疑 " + s.name + "（名字像 " + s.expected +
                           "，实际 " + s.actual + "）——表头与数据列序可能错位");
    }
    // 量级离群：轴域是否被裁剪要看 decision（检测到离群 ≠ 真裁了轴），
    // 否则小样本保护闸降级时这行会撒谎。**数据本身始终是脏的**，提示的作用
    // 是让人回去改源数据（哨兵值 999999/1e21、单位混用、手滑多打 0）。
    for (const autoviz::ValueOutlier& o : picked->features.outliers) {
        std::string advice;
        switch (o.decision) {
            case autoviz::ClipDecision::kApplied:
                advice = "Y 轴已按 IQR 篱裁剪，请确认这不是脏数据";
                break;
            case autoviz::ClipDecision::kSmallSample:
                // 3~9 个点时裁掉 1 个就占 11%~33%，剩下的点被拉满整根轴——
                // 读者会以为波动剧烈。宁可略微压平，也不制造这种错觉。
                advice = "有效值仅 " + std::to_string(o.sampled) + " 个（少于 " +
                         std::to_string(autoviz::kOutlierMinSamples) +
                         " 个），样本太小不裁轴，Y 轴保持原始范围，请改数据";
                break;
            case autoviz::ClipDecision::kTooMany:
                // 越界占比过高说明"整列本来就离散"，不是"少数脏值"；裁掉两成
                // 以上等于人为制造趋势。
                advice = "越界占 " +
                         fmt_value(100.0 * static_cast<double>(o.count) /
                                   static_cast<double>(o.sampled)) +
                         "%（超过 " + fmt_value(autoviz::kOutlierMaxClipRatio * 100.0) +
                         "%），是整列离散而非少数脏值，不裁轴，Y 轴保持原始范围，请核对口径";
                break;
            case autoviz::ClipDecision::kNone:
                continue;  // 没有越界值：不该出现在这条列表里
        }
        TERMCHART_LOG_WARN("autoviz: 列 '" + o.name + "' 存在极端离群值（" +
                           fmt_value(o.extreme) + (o.high ? " 远大于中位数 " : " 远小于中位数 ") +
                           fmt_value(o.median) + "，抽样 " + std::to_string(o.count) + "/" +
                           std::to_string(o.sampled) + " 个值越界）；" + advice);
    }

    const std::unique_ptr<chart::Chart> chart = picked->build_chosen();
    if (!chart) {
        return ErrorCode::Unsupported;  // 有候选但建不出图（路由不变量保证不可达）
    }
    return print_chart(*chart);
}

}  // namespace termchart::api
