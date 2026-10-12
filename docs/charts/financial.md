# 金融图族（chart/financial/，7 个）

## 已落地图表

| 图表 | 类 | 头文件 | 说明 |
| --- | --- | --- | --- |
| K 线 | `CandlestickChart` | cartesian/candlestick.hpp* | 阴阳烛（ OHLC） |
| 成交量 | `VolumeChart` | volume.hpp | 量能柱 |
| 回撤 | `DrawdownChart` | drawdown.hpp | 净值回撤曲线 |
| 收益率曲线 | `YieldCurveChart` | yield_curve.hpp | 期限结构 |
| 砖形图 | `RenkoChart` | renko.hpp | 固定砖块价格滤波 |
| 点数图 | `PointFigureChart` | point_figure.hpp | X/O 列变化 |
| 卡吉图 | `KagiChart` | kagi.hpp | 阴阳转折线 |
| 深度图 | `DepthChart` | depth.hpp | 买卖盘深度 |

*注：candlestick 头文件位于 `chart/cartesian/`（规格归金融族，物理位置在
直角坐标目录），其余在 `chart/financial/`。

## OHLC 约定

K 线用字符方向与密度编码涨跌（非仅靠颜色）：实心/空心、竖线长度表达
开高低收。单色终端可读。

## 派生类图表的变换

- Renko / PointFigure / Kagi 都是对价格序列的**滤波变换**：先按规则生成
  砖块/列/转折，再画。变换参数（砖块大小、反转额度）在各自 Options；
- Drawdown / YieldCurve 是序列变换后画线。

## API 形态

与本库其他图表一致：`struct XxxOptions` + `class XxxChart final : public Chart`，
`set_*` 传数据、`draw(DrawContext)` 绘制。字段以头文件为准。

## 未做

- 十字光标取值联动（interaction/crosshair.hpp 存在，但未与金融图族
  预组装成"行情终端"示例）；
- 复权、停牌缺口等业务语义处理（库只管画）。
