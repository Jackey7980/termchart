# 主题与配色

## 颜色基础类型

`core/style.hpp` 的 `Style`（前景/背景/属性位）与 `core/rgb.hpp` 的 `Rgb`；
`core/color_depth.hpp` 的 `ColorDepth` 决定量级（真彩/256/16）。

## 离散调色板：color/palette.hpp

```cpp
namespace termchart::color {
enum class PaletteKind { Default, Ocean, Forest, Sunset, Mono, HighContrast };

std::span<const Rgb> palette(PaletteKind);          // 整块色板
Rgb series_color(PaletteKind, std::size_t index);   // 第 i 个系列，循环取模
std::size_t palette_size(PaletteKind);
std::span<const Rgb> palette_by_name(std::string_view); // 未知名返回 Default
}
```

- 系列取色用 `series_color`，系列数超过色数自动循环，不越界；
- 单色/打印/降级场景用 `Mono`；
- 未知名字返回 Default 而不是失败——配色不该让图表画不出来。

## 连续色表：color/colormap.hpp

```cpp
enum class ColormapKind { Viridis, Inferno, Magma, Plasma, Turbo };
Rgb sample(ColormapKind, double t);                 // t 钳到 [0,1]
std::span<const Rgb> stops(ColormapKind, std::size_t count = 256); // 预采样缓存
```

- 热力图/色标每秒查几万次：用 `stops()` 预采样（首次构建、之后只读、无锁——
  单线程主循环前提）；
- **Turbo 不许近似**：用官方 256 级查找表（`detail/turbo_table.hpp` 生成物）。

## 降级链

`color::resolve_*` 先按 `ColorDepth` 量化再取色：真彩颜色在 16 色深度下返回
16 色表里最接近的那项。能力探测来自 `term::detect(CapEnvironment, tty)`
（显式环境变量 > TERM 名 > 保守默认；`WT_SESSION` 识别 Windows Terminal 真彩）。

## 图表层的样式钩子

- `chart::DrawContext` 的 `grid_style / axis_style`：空 Style 用库默认
  （网格暗灰、轴默认前景）；
- `Chart::set_margins(top, right, bottom, left)`（默认 1/1/3/8）；
- 每个图表类的 `set_color()` / Options 里的色字段控制系列主色。

## 字符即主题的一部分

单色终端可读性不靠颜色：扇区/系列取专属填充字符（█▓▒░■●▲◆○ 循环）。
设计深色主题时同样要保证字符策略仍然成立（不要把"靠颜色区分"当成唯一通道）。

## 命名色

`color/named_colors.hpp` 提供 HTML 式命名色（header-only 纯头）。

## 未做

规格中的全局主题对象（一次设置、全图表生效）未落地；现状是逐图表传
Style/Options + 全局调色板组合。
