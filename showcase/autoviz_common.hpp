// showcase/autoviz_common.hpp —— autoviz "一张图" 示例的共享逻辑（header-only）
//
// 复用 Panorama 难数据（showcase::build，固定种子 20261022 完全可复现）：把 6 个相关
// 品种的价格路径 + 等权指数拼成一张带时间列的宽表，喂给 autoviz，让它"自己选对图"。
// TUI 与 GUI 两个可执行共用本文件的 run_autoviz()，分别落 ANSI 与真窗口。
#pragma once

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "termchart/termchart_full.hpp"  // 全库伞形头（含 chart / api / gui 像素渲染）
#include "termchart/autoviz/engine.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/data/dataframe.hpp"

#include "panorama_data.hpp"

namespace showcase {

// 从步数生成 ISO 日期（用于 autoviz 时间管道；2024 非闰年，2 月按 28 天）。
inline std::string iso_date(std::size_t t) {
    int y = 2024, m = 1, d = 1;
    const int dim[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (std::size_t i = 0; i < t; ++i) {
        ++d;
        if (d > dim[m]) {
            d = 1;
            if (++m > 12) {
                m = 1;
                ++y;
            }
        }
    }
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%u-%02u-%02u", static_cast<unsigned>(y),
                  static_cast<unsigned>(m), static_cast<unsigned>(d));
    return std::string(buf);
}

// 难数据 → 宽表 CSV（时间列 + 6 品种价格 + 指数），autoviz 的容错入口吃这个。
inline std::string autoviz_csv(const Panorama& p) {
    std::string s = "日期";
    for (const Instrument& in : p.inst) {
        s += ',';
        s += in.tag;
    }
    s += ",指数\n";
    char buf[64];
    for (std::size_t i = 0; i < p.M; ++i) {
        s += iso_date(i);
        for (const Instrument& in : p.inst) {
            s += ',';
            std::snprintf(buf, sizeof(buf), "%.6g", in.price[i]);
            s += buf;
        }
        s += ',';
        std::snprintf(buf, sizeof(buf), "%.6g", p.market[i]);
        s += buf;
        s += '\n';
    }
    return s;
}

struct AvResult {
    std::unique_ptr<tc::chart::Chart> chart;  // 选中并建好的图表（空 = 无可用候选）
    std::string name;        // chart_name::kXxx
    std::string title;       // 图表标题
    std::string pipeline;    // 命中管道名
    std::string csv;         // 喂给 autoviz 的数据（可 --csv 落盘核对）
    std::size_t viable = 0;  // 通过否决的候选数
    std::string error;       // 非空 = 失败原因
};

// 端到端：难数据 → CSV → parse_dataframe → auto_visualize → 选中图。
inline AvResult run_autoviz() {
    AvResult r;
    const Panorama p = build(20261022u);
    r.csv = autoviz_csv(p);

    tc::data::DataFrame df = tc::data::parse_dataframe(r.csv);
    if (df.columns.empty()) {
        r.error = "parse_dataframe 返回空表";
        return r;
    }

    tc::autoviz::AutoVizOptions opts;
    opts.time_column = "日期";  // 强制时间管道：多品种价格 → 多序列折线
    opts.canvas_width = 120;
    opts.canvas_height = 40;

    auto res = tc::autoviz::auto_visualize(df, opts);
    if (!res) {
        r.error = "auto_visualize 失败 code=" + std::to_string(static_cast<int>(res.error()));
        return r;
    }

    for (const auto& cand : res->candidates) {
        if (cand.viable()) ++r.viable;
    }
    if (const tc::autoviz::ChartCandidate* c = res->chosen()) {
        r.name = c->name;
        r.title = c->title;
        r.pipeline = std::string(tc::autoviz::pipeline_name(c->pipeline));
    }
    r.chart = res->build_chosen();
    if (!r.chart) {
        r.error = "build_chosen 返回空（无可用候选）";
    }
    return r;
}

}  // namespace showcase
