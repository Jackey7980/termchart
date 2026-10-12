# 文本图族（chart/text/，4 个）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| 进度条组 | `Progress` | progress.hpp | `ProgressOptions{bar_width, show_percent}` |
| 表格 | `Table` | table.hpp | —（按头文件为准） |
| 标签云 | `TagCloud` | tag_cloud.hpp | `TagCloudOptions` |
| 词云 | `Wordcloud` | wordcloud.hpp | `WordcloudOptions` |

## progress.hpp（已核对的真实 API）

```cpp
struct ProgressOptions {
    std::size_t bar_width = 0;    // 0 = 自动：内容区宽 - 标签宽 - 百分比列
    bool show_percent = true;
};
class Progress final : public Chart {
public:
    void add_bar(std::string label, double fraction, Color color = Color::ansi16(2));
    void set_options(const ProgressOptions& options);
    [[nodiscard]] std::size_t bar_count() const noexcept;
};
```

每行：标签 + 液条（█ 已完成 / ░ 未完成）+ 百分比。
fraction 收敛到 [0,1]，非有限值按 0 处理，不崩溃。

## progress vs progress_ring（规格强调的区分）

- **text/progress**：横向条形**组**，多值并列、行式读数；
- **indicator/progress_ring**：单值**环形**读数（chart/indicator/progress_ring.hpp）。
二者服务不同场景，不互相替代——仪表盘上并列多任务进度用前者，
单个完成度大字读数用后者。

## Table

语义表格的字符渲染：边框用制表符（Unicode / ASCII 退化）、列宽自适应、
对齐可控。与 `document/table_model.hpp`（structured 导出的表格模型）分工：
这里负责画，document 负责语义。

## TagCloud / Wordcloud

词频 → 字号（终端里是**文本密度/长度/色深**的映射）。
区别按头文件注释为准（tag_cloud 偏轻量行内，wordcloud 偏整块排布）。

## 示例

examples/11_wordcloud。
