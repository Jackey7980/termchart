# 终端显示图片

## 链路

```
image::Bitmap（RGBA）→ protocol::*（编码字节流）→ 调用方写终端
```

`protocol/` 只负责编码：不做布局、不移动光标、不写 stdout——定位由调用方决定。

## 协议与降级

| 协议 | 类 | 负载 | 适用 |
| --- | --- | --- | --- |
| sixel | `SixelProtocol` | DCS 串，RLE 压缩 | xterm 等 |
| kitty | `KittyProtocol` | ESC_G，原始 RGBA | kitty |
| iTerm2 内联 | `Iterm2Inline` | OSC 1337，**PNG + base64**（基于内置 png_encode） | iTerm2 |
| 半块字符 | `HalfBlockProtocol` | ▀ + 前景/背景色 | 任何 24-bit 终端（兜底） |

```cpp
#include "termchart/protocol/protocol.hpp"
#include "termchart/protocol/sixel.hpp"

termchart::protocol::SixelProtocol sixel;
termchart::protocol::ProtocolOptions opt;  // cell_width_px=10, cell_height_px=20, rle=true
std::string bytes = sixel.encode(bitmap, opt);
termchart::api::write_stdout(bytes);
```

## 能力探测

- `term/caps.hpp`：`TerminalCaps::image` 是 `ImageProtocolKind`（None/Halfblock/Sixel/Kitty/ITerm2），
  `detect(env, tty)` 按优先级合成（显式环境变量 > TERM > 保守默认）；
- `protocol/detect.hpp`：协议级探测；
- `protocol/fallback.hpp`：`ProtocolCaps` + 降级链——不支持图形协议时退到半块字符。

## 位图来源

- 自造：`image::Bitmap`（RGB/RGBA，`resize/at/set`，越界安全）；
- PNG 解码：`image/png.hpp`（自带完整 inflate，可读任意常规 PNG；
  边界：不支持 Adam7 隔行、16 位样本取高字节、CRC 严格）；
- PNG 编码：`image/png_encode.hpp`（iTerm2 负载依赖它）；
- 标量场 → 位图：`image/convert.hpp`（配 `color/colormap.hpp` 着色）。

## 示例

examples/38_image_sixel（终端 sixel 显示位图）、07_fractal（分形 → 位图）。

## 常见坑

- iTerm2 只认 PNG 负载，直接喂 RGBA 不被识别；
- 半块显示天然 1:2 宽高比（一个 ▀ 覆盖上下两像素行）；
- 非支持终端先探测再编码：`caps().image == ImageProtocolKind::None` 时画字符版。
