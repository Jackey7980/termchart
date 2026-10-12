# 性能优化

## 原则先行

1. **输出量 = 变化面积**：实时路径靠差分（render/diff.hpp + `append_style_delta`），
   每帧只写变化段。全量重发是性能 bug。
2. **字节生成与写出分离**：`Renderer` 只生成字节；写出收在 `api/print.hpp`。
   测性能时把二者分开计时。
3. **查表不现算**：colormap 用 `stops()` 预采样缓存（热力图每秒查几万次，
   逐次跑多项式太贵）；宽度/分割查 `detail/*_tables.hpp` 生成表（constexpr、零初始化）。

## 各环节要点

| 环节 | 做法 |
| --- | --- |
| 帧 diff | `diff(prev, cur, merge_gap=4)`：同行相邻段合并；尺寸变了直接全量 |
| SGR | `append_style_delta()` 只发变化样式；`style_sequence()` 是完整序列（未设的发 39/49），别在帧内用 |
| buffer | `std::span<Cell>` 不跨 `resize` 保存；侧面表稀疏，别逐格摸 |
| Scene | `reserve(count)` 预留；`bounds()` 有缓存语义时别反复算 |
| 光栅化 | 输入 `FloatField` 尺寸 ÷ 子像素 = 输出格数，按需选低分辨率光栅器（Ascii 最便宜） |
| 布局 | `make_area`/轴绘制每帧重算是可接受的；布局搜索类（graph）注意复用 WireGrid 障碍登记 |
| 汇编层 | `detail/sort_network.hpp` 之类生成结构就是为省分支 |

## 基线

`bench/` 目录零外部依赖，`--quick` 进 ctest 冒烟：

- bench_sugiyama：分层布局扩展性
- bench_force：力导向收敛
- 规格中其余（bench_render/bench_raster/bench_buffer/bench_diff 等）以
  `bench/` 目录现状为准

历史基线 `bench/baseline.json`。性能目标口径见
[reference/performance_targets.md](../reference/performance_targets.md)。

## 常见反模式

- 每帧 `render_full()`（除非真清屏了——清屏后先 `reset()`）;
- 用 `style_sequence()` 拼帧内输出（字节翻倍）;
- 在循环里调 `palette_by_name`/解析类函数（字符串查表），应预先解析一次;
- 把 golden 测试当性能测试（golden 锁外观，bench 才锁吞吐）。

## 测量纪律

- 用 `util/timer.hpp` 计时；过滤日志（`util/logging.hpp`）再测；
- 并行会话会让全量构建随机变红（半截新文件），性能结论要干净树上复测
  （CODEBUDDY.md 坑 25）。
