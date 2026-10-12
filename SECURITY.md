# Security Policy / 安全策略

## 支持的版本

termchart 尚未发布正式版本（CMake 项目版本 `0.1.0`，阶段 0–5 已落地、阶段 6 推进中，
权威进度见 [`ROADMAP.md`](ROADMAP.md)）。当前只对 `main` 分支的最新提交提供安全修复。

## 报告漏洞

**请勿通过公开 Issue 报告安全漏洞。**

请使用 GitHub 的私有漏洞报告渠道：

1. 打开 <https://github.com/Jackey7980/teamchart/security/advisories>；
2. 点击 **Report a vulnerability**，私有描述问题（复现步骤、影响面、涉及模块）。

## 响应时限

| 阶段 | 目标 |
| --- | --- |
| 确认收到 | 3 个自然日内 |
| 初步评估（影响面 / 严重级别） | 7 个自然日内 |
| 修复或缓解（高危） | 30 个自然日内 |

## 漏洞报告范围说明

termchart 是渲染库，攻击面集中在**输入解析**路径，尤其：

- `image/png`（PNG 解码，自带 inflate）；
- `protocol/`（sixel / kitty / iTerm2 内联序列解析）；
- `interaction/`（终端输入字节流解析）;
- `dsl/`（mermaid 等 DSL 文本解析）；
- `util/utf8` / `util/grapheme`（不可信 UTF-8 文本）。

这些路径均有对应 fuzz 目标（`tests/fuzz/`，`ctest --preset fuzz`）与语料，
报告时附上能触发的最小输入文件最有帮助。库的输出侧（SGR / 终端转义序列生成）
只面向终端，通常不属于安全边界，但欢迎一并反馈。

## 处理过的已知边界（非漏洞）

见 `README.md`「已知边界」与 `docs/reference/capability_matrix.md`：
PNG 解码不支持 Adam7 隔行、16 位样本取高字节等，均为文档化行为。
