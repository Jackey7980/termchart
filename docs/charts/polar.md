# 极坐标图族（chart/polar/，8 个）

## 已落地图表

| 图表 | 类 | 头文件 |
| --- | --- | --- |
| 饼图 | `PieChart` | pie.hpp |
| 环图 | `DonutChart` | donut.hpp |
| 玫瑰图 | `RoseChart` | rose.hpp |
| 雷达 | `RadarChart` | radar.hpp |
| 极坐标柱 | `PolarBarChart` | polar_bar.hpp |
| 极坐标线 | `PolarLineChart` | polar_line.hpp |
| 极坐标热力 | `PolarHeatChart` | polar_heat.hpp |
| 仪表盘 | `GaugeChart` | gauge.hpp |

## pie.hpp（已核对的真实 API）

```cpp
struct PieOptions {
    double start_angle = 90.0;   // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;
    bool show_legend = true;
};
class PieChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const PieOptions& options);
    void draw(DrawContext ctx) override;                 // Canvas 直绘回退
    bool to_scene(scene::Scene& out, std::uint32_t width,
                  std::uint32_t height) const override;  // 首选：产出矢量 IR
};
```

字符化策略（头文件注释原文摘要）：圆盘 = 绘图区内切椭圆的**格粒度解析填充**
（逐格归一化半径 ≤ 1 才落笔）；扇区不依赖颜色——每扇区取阶梯表**专属填充字符**
（█▓▒░■●▲◆○ 循环），扇区边界即字符边界。

## gauge.hpp（已核对的真实 API）

```cpp
struct GaugeOptions {
    double value_min = 0.0, value_max = 100.0;
    double start_angle = 225.0, end_angle = -45.0;  // 经典 270° 开口朝下
    bool clockwise = true;
    int tick_count = 9;          // 主刻度数（含两端）
    bool show_value = true;
};
class GaugeChart final : public Chart {
public:
    void set_value(double value);
    void set_color(Color color);
    void set_options(const GaugeOptions& options);
};
```

字符化：弧体 `●`、主刻度 `+`（弧体与内侧 rnorm 0.88 两处）、指针 `*`
（圆心到 rnorm 0.72 的 DDA 线段）、轴帽 `●`；数值域线性映射到扫角。

## 与 indicator/gauge_kpi 的区分（规格强调）

- **polar/gauge**：弧形刻度带 + 指针，图形化读数；
- **indicator/gauge_kpi**：边框 + 标签 + 主值 + ▲/▼ 变化量的**数字卡片**，
  不做弧形。选型见 charts/indicator.md。

## 角度约定

所有极坐标图统一：角度为度、数学方向（0=东、90=北），顺/逆时针由
`clockwise` 控制。圆在格粒度下必有台阶，按角度采样落格，不做假精度。

## 示例

examples/09_radar、10_gauge。
