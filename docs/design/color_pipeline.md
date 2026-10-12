# 颜色管线与降级

## 管线

```
数据 → Color（语义色）→ color::resolve_*（按 ColorDepth 量化）→ SGR 序列（render/sgr.hpp）→ 终端
```

- `core/color_depth.hpp` 定义 `ColorDepth`（真彩 / 256 / 16 …）；
- `render/sgr.hpp` 是**唯一 SGR 生成点**：`append_style()`（完整序列，未设的前/背景发
  39/49）与 `append_style_delta()`（只发变化部分，帧内路径用这个）；
- 任何地方都不许再拼一套 SGR（收窄边 `backend/snapshot/ansi_backend > render/sgr`
  就是为了让导出路径也复用它）。

## 降级规则

1. **量化先行**：`color::resolve_*` 会先按 `ColorDepth` 量化再取色——真彩颜色在
   16 色深度下返回 16 色表里的那一项。这是刻意的：GUI 显示的颜色必须与终端实际
   显示一致，不能让 GUI 假装能显示真彩。
2. **能力探测**：`term/caps.hpp::detect(CapEnvironment, tty)` 纯函数合成能力
   （显式环境变量 > TERM 名称 > 默认保守值；`WT_SESSION` 是 Windows Terminal
   真彩的可靠指纹）。非 tty 先套 `term/daemon_safe.hpp` 的保守假设。
3. **Unicode 降级**：`Opts::unicode(false)` / `ascii_only()` 时制表符 → '+'、
   块字符 → '#'。

## 语义色设施

- `color/palette.hpp`：**离散**调色板（Default/Ocean/Forest/Sunset/Mono/HighContrast），
  `series_color(kind, i)` 循环取模，系列多不越界；未知名字返回 Default——配色不该让
  图表画不出来。
- `color/colormap.hpp`：**连续**色表 viridis/inferno/magma/plasma/turbo；
  `sample(kind, t)` 纯函数；`stops(kind, n)` 预采样缓存（无锁，单线程主循环前提）。
  Turbo 用官方 256 级查找表（`detail/turbo_table.hpp`，生成器自 assets/colormaps），
  **不许"近似"**——没有解析式定义。
- `color/gradient.hpp` / `quantize.hpp` / `named_colors.hpp`：渐变、量化、命名色。

## 图表层面的可读性

颜色不可辨时图表仍须可读：扇区/系列取**专属填充字符**（█▓▒░■●▲◆○ 循环），
颜色只是叠加信息。
