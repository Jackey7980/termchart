# 地理图族（chart/geo/，5 个）

## 已落地图表

| 图表 | 类 | 头文件 | Options |
| --- | --- | --- | --- |
| 真地图 | `GeoMap` | geo_map.hpp | `GeoMapOptions` + `GeoRasterOptions` + `GeoBitmapOptions` |
| 填色地图 | `Choropleth` | choropleth.hpp | `ChoroplethOptions` + `GeoRegion` |
| 点密度 | `DotDensity` | dot_density.hpp | `DotDensityOptions` + `DotDensityRegion` |
| 六边形聚合 | `Hexbin` | hexbin.hpp | `HexbinOptions` |
| 瓦片网格 | `TileGrid` | tile_grid.hpp | `TileGridOptions` + `GeoTile` |

## 数据模型

- `GeoRegion`：一个地理区域（多边形顶点 + 值），Choropleth / DotDensity 的输入；
- `GeoTile`：瓦片网格的一个格（行列坐标 + 值），TileGrid 的输入——
  把地理简化成等值格网（如国家级"网格地图"）；
- **GeoJSON FeatureCollection**：`GeoMap` 直接吃 `assets/maps/*.json`（`util/geo/geojson.hpp`
  解析），支持 `add_overlay_*` 叠加图层（国家轮廓当底图 + 城市点当数据层，见 `examples/69_city_plot`）。

## 呈现策略

- **Choropleth**：区域内按值着色/密度填充；区域边界的格粒度多边形走
  解析落格（不做假精度）；
- **DotDensity**：区域内按值撒点（点数 ∝ 值），点的落位是确定性的
  （同输入同输出，可测试）；
- **Hexbin**：把散点聚到六边形格——终端里六边形由字符近似表达，
  聚合统计在数据侧完成；
- **TileGrid**：最朴素的"等值瓦片"地图，每格一个字符/色。

## 坐标系

两条路并存，别混用：

- **Choropleth / DotDensity / Hexbin / TileGrid 不做投影**：区域顶点是调用方给的
  格坐标或归一化坐标，经纬度 → 平面不在这四个类的职责内；
- **GeoMap 自己做投影**：`GeoProjection{Equirectangular, WebMercator, Orthographic,
  LambertCylindrical}`，输入是经纬度，内部按投影后 bbox 等比取景（正射/兰勃特还带
  `ortho_center_*` / `lambert_ref_lat` 中心参数）。

## 呈现策略（GeoMap）

三条出口共用同一份投影几何，不重复实现投影：

1. `draw(DrawContext)`：字符格 Bresenham 描边，最低清；
2. `rasterize(cols, rows)`：子像素栅格化，`GeoRasterStyle{Braille 2×4, Quadrant 2×2,
   HalfBlock 1×2}`，逐格取"覆盖面积最大的区域"的颜色；
3. `render_bitmap(w, h)`：真彩色位图（横向精确重叠 + 纵向超采样），GUI / PNG / sixel 的数据源。

`examples/67_world_map`（四投影 + 三种 style + `--png`）、`68_cn_province`（省界分级着色）、
`69_city_plot`（国家轮廓底图 + 城市点叠加）是这三条出口的活文档。

## 配色

连续值配色用 `color/colormap.hpp`（viridis/turbo 等），离散分级用
`color/palette.hpp`；降级链与全库一致（先按 ColorDepth 量化）。
GeoMap 的 `bins` 是"分级数"，但**目前只有字符格出口真的按 bins 量化**，高清两条出口是连续映射。

## 数据来源

`assets/maps/{world,cn,cities}.json` 随仓库走（已进 git），由 `tools/fetch_map_data.py`
抓 Natural Earth 50m + DataV 省界后量化生成；`tools/topojson_to_geojson.py` 负责 TopoJSON
缝合。产物合法性由 `tools/gen_geojson_samples.py --check`（已接进 `check_generated`）守。

## 未做

- 图例 / 色标（`bins` 分级没有 legend，看不出颜色含义）；
- `HitRegion` 出口（`interaction` 的 tooltip / `legend_toggle` 接不上，城市点不能悬停出值）；
- 瓦片服务、缩放平移联动（interaction/zoom 可配合但未预组装）；
- 安装包不带地图数据（只有 `install(DIRECTORY include/ ...)`，无 `share/termchart/maps`）。

逐条待办与验收口径见 `docs/reference/backlog.md` 第六节，质量项见 `quality_todo.md` P5。
