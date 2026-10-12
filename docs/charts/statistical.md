# 统计图族（chart/statistical/，11 个）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| ROC 曲线 | `RocChart` | roc.hpp | `RocOptions` + `RocMode` 枚举 |
| ECDF | `EcdfChart` | ecdf.hpp | `EcdfOptions` |
| 回归拟合 | `RegressionChart` | regression.hpp | `RegressionOptions` |
| 相关系数矩阵 | `CorrMatrixChart` | corr.hpp | `CorrMatrixOptions` |
| Bland-Altman | `BlandAltmanChart` | bland_altman.hpp | `BlandAltmanOptions` |
| 森林图 | `ForestChart` | forest.hpp | `ForestOptions` + `ForestStudy`（元分析研究项） |
| PCA | `PcaChart` | pca.hpp | `PcaOptions` |
| 自相关 | `AutocorrChart` | autocorr.hpp | `AutocorrOptions` |
| 季节分解 | `SeasonalChart` | seasonal.hpp | `SeasonalOptions` |
| 生存曲线 | `SurvivalChart` | survival.hpp | `SurvivalOptions` |
| 分布叠加 | `DistributionChart` | distribution.hpp | `DistributionOptions` |

## 定位

统计图族的共同点：输入不是"直接画的数据"，而是**需要统计变换的数据**——
库负责变换（分箱、分位数、拟合、相关计算），变换结果再走通用字符化。
与 chart/cartesian 的分工：直方图/箱线这类"形状固定、计算简单"的归 cartesian；
ROC/PCA/生存分析这类有统计语义的归本族。

## API 形态

与本库其他图表一致：`struct XxxOptions` + `class XxxChart final : public Chart`，
数据经 `set_*` 传入（原始样本或统计量，按图而定），`draw(DrawContext)` 绘制。
字段语义以各头文件顶部注释为准。

## 置信区间的表达

终端格粒度下置信带用密度字符表达（误差范围宽 → 字符密度渐变），
配合 `chart/cartesian/errorbar.hpp` 的误差棒思想。

## 示例数据约定

- 非有限输入（NaN/Inf）按图的语义丢弃或按 0 处理，不崩溃；
- 样本量为 0 / 方差为 0 的退化输入：画空图或退化图形，不报错。

## 未做

- 各图的统计显著性标注、p 值显示（规格未细化）；
- 多变量面板组合（facet 级别的统计排版）——`layout/facet.hpp` 存在但未与
  统计图族接线。
