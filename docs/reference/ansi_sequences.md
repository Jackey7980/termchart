# ANSI 序列速查

## 本库的生成点

**唯一 SGR 生成点：`render/sgr.hpp`**。任何地方不手拼 SGR。

- `append_style(...)`：完整序列（未设前/背景发 `39` / `49`）；
- `append_style_delta(...)`：只发变化部分（帧内路径用这个，省字节）。

## 常用序列

| 用途 | 序列 |
| --- | --- |
| 光标定位 | `ESC[{row};{col}H` |
| 光标隐藏/显示 | `ESC[?25l` / `ESC[?25h` |
| 交替屏进/出 | `ESC[?1049h` / `ESC[?1049l` |
| 复位 SGR | `ESC[0m` |
| 前景 16 色 | `ESC[3{n}m`（n=0..7）、亮色 `9{n}` |
| 背景 16 色 | `ESC[4{n}m`、亮 `10{n}` |
| 256 色 | `ESC[38;5;{n}m` / `ESC[48;5;{n}m` |
| 真彩 | `ESC[38;2;{r};{g};{b}m` / `ESC[48;2;…m` |
| 属性 | 1 粗体、4 下划线、7 反显、22/24/27 复原 |
| 鼠标 SGR 编码开关 | `ESC[?1000;1006h` / `…l` |
| 括号粘贴 | `ESC[?2004h` / `…l` |
| 设备报告 | DA1：`ESC[c` |
| 链接 | `OSC 8;…ST`（term/hyperlink.hpp） |
| 剪贴板 | OSC 52（term/clipboard.hpp） |

完整机器表见 `detail/ansi_tables.hpp`（生成物，勿手改）；
转义解析（输入方向）在 `interaction/input_parser.hpp` 与 `term/ansi.hpp`。

## 能力门控

能不能发由 `term::TerminalCaps` 决定（`term/caps.hpp::detect`）：
显式环境变量 > TERM 名称 > 保守默认。CI/非 tty 套 `daemon_safe` 保守假设。

## 相关参考

- 颜色量化与降级：design/color_pipeline.md
- 终端能力对照：reference/terminal_matrix.md
- 能力来源与优先级：reference/terminal_capabilities.md
