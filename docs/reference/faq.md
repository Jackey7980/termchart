# 常见问题（FAQ）

## 显示类

**Q：Windows 下乱码 + ANSI 裸露（看得见 `ESC[38;2;…`）？**
不是库的 bug——程序已输出正确 UTF-8 与真彩 SGR。两端分别处理：
- 库层：`Terminal::enter()` 会调 `platform::enable_vt()` 让控制台解释转义；
- PowerShell 侧：`[Console]::OutputEncoding=[Text.Encoding]::UTF8`
  （`chcp 65001` 只改代码页，管不到解码层）。
最省事：用 Windows Terminal / VS Code 终端。

**Q：图像错位，像"颜色溢出"、顶部被截断？**
画布尺寸大于终端可视区：超宽折行 × 超高滚动叠加。绘图前用
`terminal.size()` 自适应：宽取 `cols`、高取 `rows - 1`（留一行给提示符），
按字符格约 1:2 宽高比取景。

**Q：宽字符右半边出现空格把字形盖掉？**
生产者没填续接格：宽字符右半格要写 `Cell(kContinuationCp, style)`，
渲染器只负责跳过它（design/utf8_handling.md）。

**Q：盲文图把已有内容擦掉了？**
判空时只排了空格——盲文空白是 `U+2800` 不是空格，两者都要跳。

**Q：颜色不对/发灰？**
`color::resolve_*` 先按 `ColorDepth` 量化再取色；检查能力探测结果
（`term::detect`）与 `Opts::color_depth` 是否被意外压到 16 色。

## API 类

**Q：`style_sequence()` 输出太多字节？**
它是完整序列（未设的前/背景发 39/49）。帧内用 `append_style_delta()`。

**Q：图表颜色在单色终端分不清？**
设计如此：系列靠专属字符区分（█▓▒░■●▲◆○），颜色是叠加信息；
若仍不可读，换柱状等形状区分更强的图。

**Q：运行期注册图表/插件不生效？**
注册唯一安全时机是 App 构造前（注册表构造期冻结快照）。
见 design/registry.md。

**Q：Mermaid 序列图解析返回 Unsupported？**
刻意的：序列图是时间轴模型，不进 `AstGraph`；
用 `dsl/mermaid_sequence.hpp` 的 `parse_sequence()`。

## 构建类

**Q：MinGW 构建连编译器自检都过不了？**
路径含非 ASCII 时 GNU make 按 ANSI 破坏参数。改用 `ninja` 预设。

**Q：全量构建随机失败，报的是别人文件的错？**
并行会话的半截新文件（src/** 是 GLOB_RECURSE）。等对方报完成再复核；
构建产物异常（如 exe 拒绝访问）先强制重链一次再排查。

**Q：`check_generated` 失败？**
生成物与生成器不一致：重跑 `generate_all`。只改清单不改生成物 = 失败，
手改生成物 = 更失败。

## 测试类

**Q：`CHECK(a || b)` 编译不过/断言信息错误？**
doctest 表达式分解不接受 `||`：写 `CHECK((a || b))`。

**Q：golden 怎么更新？**
`python tools/gen_golden.py` 或构建目标 `generate_golden`；禁手改快照。
