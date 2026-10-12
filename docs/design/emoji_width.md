# emoji 宽度策略

## 决策

宽度判定收在 `util/wcwidth.hpp`，除常规 East Asian Width 规则外，对 emoji 采用
**按呈现方式给宽**的策略：

- 默认（text presentation 未指定时）按宽字符处理占两格——终端实际渲染 emoji
  几乎都是双格宽，按 1 格排会与后续字符重叠；
- 宽度以**用户感知单元（grapheme）**为单位计算：ZWJ 序列、肤色修饰符、变体选择符
  都并入前一个主码点，不单独占格（分割在 `util/grapheme.hpp`）。

## 依赖关系

- `detail/unicode_tables.hpp`（生成物）提供 East Asian Width 区间表；
- `detail/grapheme_tables.hpp`（生成物）提供 emoji/组合记号区间表；
- 两者都约定「升序且互不重叠」，被 `tests/unit/test_layering.cpp` 作为结构不变量守住，
  生成器 `build_ranges()` 也自校验（重叠直接 SystemExit）。

## 为什么不跟随终端实测

终端对 emoji 的实际占格五花八门（不少终端把 2 格宽的 emoji 画成 1 格或反之），
库无法探测。策略是选「多数情况正确」的保守值：双格。
单格显示的终端会出现一列空白——这是可接受的退化，反向（重叠）不可接受。

## 未做

- 未做「按终端指纹微调 emoji 宽度」的表驱动特判；`term/caps.hpp` 的
  `TerminalCaps::unicode` 只控制 Unicode 与否，不细化到 emoji；
- 变体选择符（VS15 text presentation）强制单格的路径未在公开 API 单列。
