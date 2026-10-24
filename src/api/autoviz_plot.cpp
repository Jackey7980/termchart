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
    // 量级离群：这里只播报**检测到的事实**（哪列、极端值、主体在哪、抽样越界数），
    // 一个字都不说"裁了/没裁"——那是路由阶段用**全量值**定的结论，与图题同源，
    // 由下面那一行统一播报。旧实现在这里按 features 的**抽样** decision 断言
    // "不裁轴"，而 router 随后裁了 2 个点，图题写"裁去 2 个极值"、告警写"不裁轴"，
    // 同一张图两句话互相打脸——根因就是结论有两个出口。
    for (const autoviz::ValueOutlier& o : picked->features.outliers) {
        if (o.decision == autoviz::ClipDecision::kNone) {
            continue;  // 没有越界值：不该出现在这条列表里
        }
        TERMCHART_LOG_WARN("autoviz: 列 '" + o.name + "' 存在极端离群值（" +
                           fmt_value(o.extreme) + (o.high ? " 远大于中位数 " : " 远小于中位数 ") +
                           fmt_value(o.median) + "，抽样 " + std::to_string(o.count) + "/" +
                           std::to_string(o.sampled) +
                           " 个值越界）——数据本身是脏的，建议回源修数据");
    }

    // 轴域结论：**唯一出口**，与图题逐字同源（ChartCandidate::final_clip_desc），
    // 数字也只取 final_clip_count()（= 全量重算的裁剪点数）。features 的抽样比例
    // 只用来决定"要不要提醒数据脏"，不再决定"裁没裁轴"。
    const autoviz::ChartCandidate* const chosen = picked->chosen();
    if (chosen != nullptr && !chosen->final_clip_desc.empty()) {
        TERMCHART_LOG_WARN("autoviz: Y 轴最终结论 —— " + chosen->final_clip_desc +
                           "（与图题同一口径；域外点钳到轴边缘，不删点）");
    }
    // 数据加工说明（量纲闸拦下几个百分比格、丢了几行等）：由路由随图带出，
    // 这里原样播报。**沉默的加工就是撒谎**，所以宁可吵一点。
    if (chosen != nullptr) {
        for (const std::string& note : chosen->notes) {
            TERMCHART_LOG_WARN("autoviz: " + note);
        }
    }

    const std::unique_ptr<chart::Chart> chart = picked->build_chosen();
    if (!chart) {
        return ErrorCode::Unsupported;  // 有候选但建不出图（路由不变量保证不可达）
    }
    return print_chart(*chart);
}

}  // namespace termchart::api
