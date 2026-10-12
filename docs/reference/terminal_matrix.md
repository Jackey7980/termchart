# 终端能力对照

## 能力维度

`term/caps.hpp::TerminalCaps`（已核对）：

```cpp
struct TerminalCaps {
    bool unicode;                        // Unicode 可用（否则 ASCII 退化）
    ColorDepth color_depth;              // 真彩 / 256 / 16
    ImageProtocolKind image;             // None/Halfblock/Sixel/Kitty/ITerm2
    bool mouse;                          // 鼠标事件
    bool alt_screen;                     // 交替屏
    bool title_setting;                  // 设标题
};
```

## 常见终端对照（ detect 的典型结果）

| 终端 | Unicode | 颜色 | 图像 | 鼠标 |
| --- | --- | --- | --- | --- |
| Windows Terminal | ✓ | 真彩（WT_SESSION 指纹） | 半块 | ✓ |
| VS Code 终端 | ✓ | 真彩 | 半块 | ✓ |
| kitty | ✓ | 真彩 | kitty 协议 | ✓ |
| iTerm2 | ✓ | 真彩 | iTerm2 内联（PNG） | ✓ |
| xterm+sixel | ✓ | 真彩/256 | sixel | ✓ |
| 老版 cmd.exe | 部分 | 16 | 无 | 部分 |
| CI / 非 tty | ✗ | 无（daemon_safe） | 无 | ✗ |

（此表为 detect 规则的预期效果概述；逐条优先级规则以
`term/caps.hpp` 与 terminal_capabilities.md 为准。）

## 探测输入

`CapEnvironment{term, colorterm, term_program, ci, wt_session}`：
- `WT_SESSION`：Windows Terminal 指纹，真彩识别的可靠信号
  （原生 Windows 会话 TERM 常为空）；
- `ci`：CI / GITHUB_ACTIONS → 保守假设。

纯函数探测：不读写终端、不发查询序列——CI 与单测可完全复现。

## 图像协议对照

| 协议 | 编码器（protocol/） | 负载 |
| --- | --- | --- |
| Sixel | SixelProtocol | DCS + RLE |
| Kitty | KittyProtocol | 原始 RGBA |
| iTerm2 | Iterm2Inline | PNG + base64 |
| 半块 | HalfBlockProtocol | ▀ + 双色（任何真彩终端） |
| 无 | — | 画字符版 |

## 关联文件

- 优先级规则：reference/terminal_capabilities.md
- 深度查询 / DA1：term/capability_query.hpp（DeviceAttributes / Da1Features）
- 非 tty 保守化：term/daemon_safe.hpp
