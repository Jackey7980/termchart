# 盲文点阵表

## 定义

盲文区 U+2800–U+28FF：每个字符 8 个点，排成 **2×4 子像素网格**：

```
(1)(4)
(2)(5)
(3)(6)
(7)(8)
```

码点 = U+2800 + 位掩码，位值：dot1=0x01, dot2=0x02, dot3=0x04,
dot4=0x08, dot5=0x10, dot6=0x20, dot7=0x40, dot8=0x80。

## 机器表

`detail/braille_table.hpp` 是**生成物**（`tools/gen_braille_table.py` 产出，
禁手改）：按位掩码索引的字符表，供 `raster::BrailleRasterizer` O(1) 查询。

一致性校验：`python tools/gen_braille_table.py --check`
（或 `cmake --build --preset ninja --target check_generated`）。

## 用法（raster::BrailleRasterizer）

- 输入 `image::FloatField`，输出格数 = field ÷ (2,4)；
- 每个格聚合 8 个子像素的亮/灭（`RasterOptions::threshold`，默认 0.5）；
- 点亮色 `fg` / 熄灭色 `bg`（单色，无双色）；
- **空白是 U+2800（BLANK BRAILLE PATTERN）不是空格**——判空两者都要跳
  （历史真 bug：Scene 多次收口用空白铺满画布擦掉先前内容）。

## 与 Scene 收口的关系

`canvas/scene_renderer.cpp` 描边累积进 braille 2×4 场（分辨率优先），
按当前 `StrokeOp` 颜色光栅化；golden `scene_raster/basic.txt` 锁外观。

## 分辨率对照

| 光栅器 | 子像素 | 每格像素 |
| --- | --- | --- |
| braille | 2×4 | 8 |
| quadrant | 2×2 | 4 |
| halfblock | 1×2 | 2（可双色） |
| block/ascii | 1×1 | 1 |

需要分辨率且单色够用 → braille；需要颜色 → halfblock。
