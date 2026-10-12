# 性能目标

## 原则

性能预算花在**每帧输出字节**与**每帧 CPU 时间**两处；
基线用 bench 目标钉住，回归靠 ctest 冒烟。

## 现有基线（bench/，零外部依赖，--quick 进 ctest）

| 目标 | 度量 |
| --- | --- |
| bench_sugiyama | 分层布局扩展性（节点/边规模 → 耗时） |
| bench_force | 力导向收敛（迭代数与耗时） |
| baseline.json | 历史基线数值 |

规格中的其余 bench（render/raster/buffer/grapheme/diff/utf8/fill/scene/
svg_backend/mermaid_parse）以 `bench/` 目录现状为准。

## 量级目标（设计预算，非实测承诺）

| 项 | 预算 |
| --- | --- |
| 差分重绘 | 输出字节 ∝ 变化格数（非全屏） |
| SGR 帧内 | delta 序列（append_style_delta），不发完整样式 |
| diff 本身 | 单帧 O(格数)，merge_gap 合并减少段数 |
| colormap 采样 | 命中 stops() 缓存 O(1) |
| 生成表查询 | constexpr 表 + 二分，O(log n)、零初始化 |
| 光栅化 | field 尺寸 ÷ 子像素数，线性于输出格数 |

## 达成手段（对应 design 文档）

- 差分：design/diff_rendering.md
- 查表：design/memory_model.md（生成物表）
- 预采样缓存：design/color_pipeline.md（colormap stops）
- 布局扩展性：graph/layouts.md（bench_sugiyama / bench_force）

## 测量纪律

- 干净树上测（并行会话的半截文件会让构建随机变红）；
- 生成与写出分开计时（Renderer 只产字节，写 stdout 在 api/print）；
- golden 锁外观、bench 锁吞吐，二者不互相替代。

## 未做

- 规格中逐项量化目标（如"1 万节点布局 < X ms"）尚未在文档层固化数值；
- bench 目标与 CI 的阈值告警未接线（当前只冒烟）。
