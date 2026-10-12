# 指标图族（chart/indicator/，5 个）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| KPI 卡片 | `GaugeKpi` | gauge_kpi.hpp | `GaugeKpiOptions{show_delta}` |
| 进度环 | `ProgressRing` | progress_ring.hpp | `ProgressRingOptions` |
| 星级 | `StarRating` | star_rating.hpp | `StarRatingOptions` |
| 状态灯 | `StatusLight` | status_light.hpp | `StatusLightOptions` + `LightState` 枚举 |
| 温度计 | `Thermometer` | thermometer.hpp | `ThermometerOptions` |

## gauge_kpi.hpp（已核对的真实 API）

```cpp
struct GaugeKpiOptions { bool show_delta = true; };
class GaugeKpi final : public Chart {
public:
    void set_label(std::string text);
    void set_value(std::string text);        // 主值是预格式化文本
    void set_delta(std::string text, bool up);  // 变化量 ▲/▼
    void set_options(const GaugeKpiOptions& options);
};
```

设计要点（头文件注释）：卡片 = 边框 + 标签（上）+ 主值（中）+ 变化量（下）。
**主值是预格式化文本**——千分位、单位缩写等格式化策略留给调用方，
库不替用户做决定。

## gauge_kpi vs polar/gauge（规格强调的区分）

- **indicator/gauge_kpi**：数字卡片，"读数"强调，不做弧形；
- **polar/gauge**：弧形刻度带 + 指针，图形化读数（见 charts/polar.md）。

## progress_ring vs text/progress

- **indicator/progress_ring**：单值环形读数（弧形）；
- **text/progress**：横向条形组，多值并列。

## 其他成员

- `StatusLight`：`LightState` 枚举（状态色/字符）+ 一格指示灯；
- `StarRating`：★☆ 计数；
- `Thermometer`：竖向量程读数。

## 使用场景

这些图都是**小面积、单值、高密度信息**的呈现，适合仪表盘（examples/06_dashboard）
与状态栏组合。它们不画坐标轴（`DrawContext` 的轴/网格开关对它们无意义）。

## 仪表盘组装提示

- 主循环骨架见 guide/app_and_widget.md；
- 悬停取值：给卡片登记 `HitRegion::rect`，payload 存卡片 id；
- 刷新：只更新对应格区域，让差分渲染自然只输出变化部分。
