# 光栅化原理

## 子像素模型

终端字符格是低分辨率网格，但一个格能塞下多个「子像素」：

| 光栅器 | 子像素网格 | 特点 |
| --- | --- | --- |
| `raster::BrailleRasterizer` | 2×4（盲文点阵 U+2800 起） | 单色，分辨率最高 |
| `raster::HalfBlockRasterizer` | 1×2（▀▄） | 上下各一色，可表达彩色 |
| `raster::QuadrantRasterizer` | 2×2（四分块字符） | 单色 |
| `raster::AsciiRasterizer` | 1×1（密度字符） | 最朴素，全终端可用 |
| `raster::BlockRasterizer` | 1×1（八级竖条 ▁▂▃…） | 单色柱状感 |

契约（`raster/rasterizer.hpp` 顶部不变量）：

- `rasterize` 无副作用、不抛异常、不写 stdout；
- 输出 `CellBuffer` 尺寸 = `field / (subpixel_w, subpixel_h)`；
- 单色光栅器点亮处用 `opts.fg`、熄灭处用 `opts.bg`，`invert` 交换并反相。

输入是标量场 `image::FloatField`；统一入口
`raster::rasterize(field, kind, opts)` 与自动选择 `raster/auto_select.hpp`
（`RasterCaps` + `RasterRegistry`）。

## Scene → Canvas 的收口

`canvas/scene_renderer.cpp` 把 `Scene` 指令画进 `Canvas` 时：

- 描边累积进 **braille 2×4** 子像素场（分辨率优先）；
- 填充累积进 **halfblock 1×2** 子像素场（颜色优先，上下双色）；
- 按当前 `StrokeOp`/`FillOp` 颜色光栅化后叠加，只覆盖非空格。

两个已知坑（历史真 bug）：

1. 盲文空白是 `U+2800` 而非空格，判空两者都要跳，否则收口会擦掉先前内容；
2. 多次收口互不擦除是设计行为——叠加而非覆盖。

## 字符化哲学

格粒度下曲线必有台阶，不做假精度：饼图的圆盘是绘图区内切椭圆的**格粒度解析填充**
（逐格算归一化半径 ≤ 1 才落笔），仪表盘的弧按角度采样落格（步长约每周
`4·(rx+ry)` 格保证连续）。凡涉及形状落格的实现都遵循同一原则。

## 图表与 Scene 的关系

`chart` 现已统一经 `to_scene` 产出矢量 `scene::Scene`（与 `canvas/scene_renderer` 收口成盲文/半块
字符图、与 `backend/vector` 收口成 SVG/PDF/TikZ 同源）；`draw(DrawContext)` 仍作为 Canvas 直绘回退保留。
`graph/diagrams` 的端到端图（`render_xxx(Graph&, Canvas&, Options) → Rect`）仍直接吃 `canvas::Canvas`——
图的字符化本质就是落格，不经 Scene。
