# 地理图族（chart/geo/，4 个）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| 填色地图 | `Choropleth` | choropleth.hpp | `ChoroplethOptions` + `GeoRegion` |
| 点密度 | `DotDensity` | dot_density.hpp | `DotDensityOptions` + `DotDensityRegion` |
| 六边形聚合 | `Hexbin` | hexbin.hpp | `HexbinOptions` |
| 瓦片网格 | `TileGrid` | tile_grid.hpp | `TileGridOptions` + `GeoTile` |

## 数据模型

- `GeoRegion`：一个地理区域（多边形顶点 + 值），Choropleth / DotDensity 的输入；
- `GeoTile`：瓦片网格的一个格（行列坐标 + 值），TileGrid 的输入——
  把地理简化成等值格网（如国家级"网格地图"）。

## 呈现策略

- **Choropleth**：区域内按值着色/密度填充；区域边界的格粒度多边形走
  解析落格（不做假精度）；
- **DotDensity**：区域内按值撒点（点数 ∝ 值），点的落位是确定性的
  （同输入同输出，可测试）；
- **Hexbin**：把散点聚到六边形格——终端里六边形由字符近似表达，
  聚合统计在数据侧完成；
- **TileGrid**：最朴素的"等值瓦片"地图，每格一个字符/色。

## 坐标系

库不做地图投影：区域顶点是**调用方投影后的格坐标或归一化坐标**。
经纬度 → 平面的投影（墨卡托等）不在库的职责内。

## 配色

连续值配色用 `color/colormap.hpp`（viridis 等），离散分级用
`color/palette.hpp`；降级链与全库一致（先按 ColorDepth 量化）。

## 未做

- 内置世界/国家边界数据（库不带数据包）；
- 投影变换、瓦片服务、缩放平移联动（interaction/zoom 可配合但未预组装）。
