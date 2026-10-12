# UTF-8、宽度与组合序列

## 数据模型

- `core/cell.hpp`：每格只存**单个主码点** `char32_t`；组合序列的结构定义在
  `core/grapheme.hpp`（只定义、不分割）。
- **真正的 grapheme 分割在 `util/grapheme.hpp`**：把用户文本切成"用户感知的一个字"，
  每个字占一格，组合记号进侧面表。
- 宽度判定在 `util/wcwidth.hpp`（含 emoji 宽度策略，见 emoji_width.md）。

## 宽字符：两格 + 续接格

宽字符（CJK、全角）占两格。第二格是**续接格**：`Cell::cp == kContinuationCp`（`U'\0'`）
表示该格是宽字符的右半边。

- 生产者（写 buffer 的一方，如 `api/quick.cpp`）负责填续接格：
  `Cell(kContinuationCp, style)`；
- 渲染器（`render/renderer.cpp` 的 `emit_cell_text`）负责**跳过它、不写任何字节**。

漏掉约定的后果：渲染器会在宽字形右半边补一个空格把字形覆盖掉。
回归测试：`tests/golden/ansi_props/wide_chars.txt`（4 格 / 8 列）。

## 组合记号：侧面表

`CellBuffer` 用**稀疏侧面表**（`unordered_map<index, array<char32_t, N>>`）存组合序列：
绝大多数单元没有组合记号，逐单元存数组会让 buffer 体积翻倍。
`detail/grapheme_tables.hpp` / `detail/unicode_tables.hpp` 是生成的区间表
（升序且互不重叠，`tests/unit/test_layering.cpp` 与生成器双重自校验）。

## 输出编码

- 渲染输出 UTF-8 字节流；编解码工具在 `util/utf8.hpp`；
- Windows 中文环境"乱码 + ANSI 裸露"通常不是库的 bug：库已输出正确 UTF-8 与真彩 SGR，
  需要终端开 VT 解释 + PowerShell 设 `[Console]::OutputEncoding`（见 reference/faq.md）；
- `Opts::unicode(false)` / `ascii_only()`：一切绘制退化到 ASCII
  （制表符 → '+'、块字符 → '#'），用于保守终端。
