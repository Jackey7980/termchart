// 82_autoviz_csv.cpp —— AutoViz 一行出图：地狱级脏 CSV 全自动
//
// 演示 api::auto_plot 的完整路径：把一份"什么都有"的脏 CSV 落成临时文件，
// 引擎自己完成 sniff → 清洗 → 探测 → 路由 → 否决 → 打分 → 选图 → 出图。
// 本示例**不指定任何列与图种**——那正是 AutoViz 的存在意义。
//
// 脏数据清单（对应真实业务导出）：
//   * 注释行（# 前缀）与空行；
//   * 千分位（"1,234.50"）、货币前缀（¥2,000.00）、会计括号负数（(890.25)）；
//   * 全角数字与全角负号（３４５．６７、－１２）；
//   * 中文字段与中文分类值；NA/N/A 缺失记号；
//   * 缺列行（比表头短）、空分类行；
//   * 非法 UTF-8 字节（"\xFF\xFE" 开头的乱码串）；
//   * 混合时间格式（带秒/不带秒/纯日期）。
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include "termchart/api/autoviz_plot.hpp"
#include "termchart/api/print.hpp"

namespace {

// 故意不用原始字符串字面量：里面要放非法 UTF-8 字节与全角数字，
// 逐个转义写清楚"这就是脏字节"，而不是看不见的控制字符。
const char* kHellCsv =
    "# 2024 Q1 区域营收明细（业务系统导出，未清洗）\n"
    "ts,region,revenue,note\n"
    "2024-01-15 08:30:00,华东,\"1,234.50\",正常\n"
    "2024-01-16 09:00:00,华北,(890.25),会计负数\n"
    "2024-02-01 10:15:30,华南,¥2,000.00,货币前缀\n"
    "\n"  // 空行
    "2024-02-03 11:00:00,华东,３４５．６７,全角数字\n"
    "2024-03-01 12:00:00,,N/A,缺失事件\n"  // 空分类
    "2024-03-05,华北,150.00,纯日期\n"      // 短时间格式
    "2024-03-06 08:00:00,华南,－１２,全角负号\n"
    "2024-03-07 09:30:00,华东,\"\xFF\xFE"
    "坏字节\",乱码\n"
    "2024-03-08,华北,98.7,NA\n"
    "2024-03-09 10:00:00,华南,2,400,缺列行只有三列\n";

}  // namespace

int main() {
    // 落到临时文件：auto_plot 的入口是文件路径（与"喂给引擎一个文件"的
    // 真实用法一致），示例不绕过它。
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "termchart_82_autoviz_hell.csv";
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    {
        std::FILE* out = std::fopen(path.string().c_str(), "wb");
        if (out == nullptr) {
            std::fprintf(stderr, "82_autoviz: 打不开临时文件 %s\n", path.string().c_str());
            return 1;
        }
        std::fputs(kHellCsv, out);
        std::fclose(out);
    }

    std::printf("== 82_autoviz_csv: 地狱级脏 CSV → 自动选图 ==\n");
    std::printf("(临时文件: %s)\n\n", path.string().c_str());

    const termchart::Status st = termchart::api::auto_plot(path.string());
    if (st != termchart::ErrorCode::Ok) {
        std::printf("\n== auto_plot 失败: %s ==\n", termchart::describe(st));
        std::filesystem::remove(path);
        return 1;
    }

    std::filesystem::remove(path);
    return 0;
}
