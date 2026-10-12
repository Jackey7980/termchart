# 终端能力：来源与优先级

## 优先级规则（term/caps.hpp 顶部注释的口径）

```
显式环境变量 > TERM 名称 > 默认保守值
```

## 合成入口

```cpp
namespace termchart::term {
struct CapEnvironment { std::string_view term, colorterm, term_program;
                        bool ci; std::string_view wt_session; };
struct TerminalCaps   { bool unicode; ColorDepth color_depth;
                        ImageProtocolKind image; bool mouse;
                        bool alt_screen; bool title_setting; };

TerminalCaps detect(const CapEnvironment& env, bool tty) noexcept;
TerminalCaps minimal() noexcept;                       // 最保守：ASCII/无色/无图像
ColorDepth color_depth_for(const CapEnvironment&) noexcept;
ImageProtocolKind image_protocol_for(const CapEnvironment&) noexcept;
bool unicode_for(const CapEnvironment&) noexcept;
}
```

## 三条单维规则

- `color_depth_for`：COLORTERM=truecolor / 24bit → 真彩；
  WT_SESSION → 真彩（Windows Terminal 指纹必设、其他终端几乎不设）；
  TERM 含 256color → 256；TERM 含 16color → 16；CI/未知 → 16 保守。
- `image_protocol_for`：kitty 终端 → Kitty；iTerm.app → ITerm2；
  TERM 含 sixel/xterm-256color 系 → Sixel（按规则表）；
  都不中 → Halfblock（真彩即可用的兜底）。
- `unicode_for`：LANG/LC_ALL 含 UTF-8、或 Windows Terminal 等 → true。

（具体匹配串以头文件实现为准；本文档记录的是规则形态与优先级顺序。）

## 纯函数纪律

探测**不读写终端、不发查询序列**：输入是环境变量快照 + 是否 tty。
- CI/单测把环境变量塞进 `CapEnvironment` 即可完全复现；
- 非 tty（`tty == false`）先套 `term/daemon_safe.hpp` 的保守假设再合成；
- 规则函数单独公开是**为了可测试**——它们就是那份优先级表的可执行形式。

## 需要更强探测时

- DA1 设备属性查询：`term/capability_query.hpp`
  （`DeviceAttributes` / `Da1Features`——发 `ESC[c` 收应答；
  有交互回路时才可用）；
- 深度实测：写真彩色块 + 光标回读属侵入式探测，库默认不做，
  交由调用方自行决定。

## 消费方

- `Opts::color_depth`（api/opts.hpp）：覆盖探测结果的手动出口；
- `Renderer(ColorDepth)` / `SnapshotOptions::depth`；
- `protocol/fallback.hpp` 的 `ProtocolCaps`（图像降级链）。
