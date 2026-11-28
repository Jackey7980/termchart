# 交接文档：termchart 收尾冲刺

> 生成日期：2026-10-19。用途：把"继续完成整个项目"的进度、决策、未提交 WIP、剩余路线图、架构红线一次性交代清楚，供下一位（人或 Agent）无缝接手。
> 权威口径仍以 `ROADMAP.md` / `docs/reference/backlog.md` / `docs/reference/capability_matrix.md` 为准；本文件是某一时刻的快照。
>
> **注意：本文已过时**（§0 提到的 BMP 解码器 WIP 早已落地）。当前活跃的专题交接见
> [`hevc_encoder_handoff.md`](hevc_encoder_handoff.md)（自写 HEVC 真帧内压缩档）。

## 0. TL;DR 给接手者

- 主干已提交、**护栏全绿**（见 §2）。
- 唯一未提交 WIP 是 **BMP 解码器**（3 个文件，代码写完但**未编译 / 未接分发器 / 未登记 CMake**）。**第一件事就是把它收尾落地**（§3 有精确清单）。
- 之后按 §4 优先级推进：image 扩容（JPG/GIF/WebP 走 vendored `stb_image`）、3D、图表收尾、文档画廊。

## 1. 本会话已落地（已提交，可 `git log` 核对）

| commit    | 内容 |
|-----------|------|
| `454e172` | 自主 TTF 解析（sfnt/cmap/glyf/loca/hmtx + 二次贝塞尔光栅化 + glyph cache） |
| `dc205d1` | widget 即时控件三件（dropdown / textbox / radio） |
| `0c2808f` | network HTTP/1.1 客户端 + SChannel TLS 后端（fail-loud） |
| `b2fa380` | GeoJSON 地图图种 + 画廊重生成 + 示例告警修正 + Win32 demo |
| `e8cef17` | 路由穿框修复 + dsl/JSON 解析 + network 模块骨架 |
| `bc8f005` | 真地图图种 + 数据聚合/重采样 + PDF 后端 |

> 用户最初列的 7 项路线图中，**TTF、widget、HTTP（含 TLS）** 三项已交付提交。更早的 gui/api/geo 等见 `git log`。

## 2. 已验证的一致性（护栏四件套，本会话实测）

- `python tools/check_layers.py` → **通过**（network 层已登记，无 KeyError）。
- `python tools/check_header_only.py` → **通过**（350 头 / 302 实现）。
- `python tools/gen_single_header.py --check` → **在移走 BMP 未提交文件后通过**（即已提交主干的单头是自洽的）。
- `python tools/check_generated`（或 `cmake --build --preset ninja --target check_all`）同构：`detail/*.hpp`、`结构.md`、module_matrix 等生成物未被手改。
- 全量测试与 ctest：per 提交历史为绿（TTF/WIDGET 提交均报告全量零回归、ctest 74/74；network 后可能 +1）。**接手者第一验证建议仍跑 `ctest --preset ninja` 确认本机环境**。

## 3. 未提交 WIP：BMP 解码器（image 模块，原生零依赖实现）

文件（均 `??` 未跟踪）：

- `include/termchart/media/image/bmp.hpp`
- `src/media/image/bmp.cpp`
- `tests/unit/image/test_bmp.cpp`

设计（详见 `bmp.hpp` 头注）：

- 支持 1/4/8（调色板）·24（BGR）·32/16（BITFIELDS 或默认掩码）位深；bottom-up 与 top-down（负高）；行 4 字节对齐；调色板缺省色表。
- **显式拒绝**：RLE4/RLE8 压缩 → `Unsupported`；OS/2 12 字节旧头 → `Unsupported`；偏移/尺寸越界或掩码不可解析 → `ParseError`。
- 输出 RGBA `Bitmap`（与 PNG/TTF 解码同产出，供 GUI / PNG 截图 / Canvas 复用）。

**收尾清单（按顺序，缺一步都不自洽）**：

1. 接入分发器：在 `src/media/image/decoder.cpp` 顶部加 `#include "termchart/media/image/bmp.hpp"`，并在 `decode_image` 的 switch（当前 61–71 行）加 `case ImageFormat::Bmp: return decode_bmp(data, size);`。`sniff_image_format` 已能嗅探 `"BM"`（33 行），无需改。
2. 登记测试：在 `tests/CMakeLists.txt` 第 77 行 `unit/image/test_ttf_font.cpp` 之后加一行 `unit/image/test_bmp.cpp`。
3. 构建：`cmake --build --preset ninja --target termchart termchart_tests`。
4. 跑测试：`./build/ninja/tests/termchart_tests.exe --test-case="image/bmp*"`（含错误路径），再全量。
5. 护栏：`python tools/check_layers.py` / `check_header_only.py` / `gen_single_header.py --check`（此时 BMP 已提交路径，需先重生成单头见第 7 步）。
6. 全量：`ctest --preset ninja`。
7. **重新生成单头**：`python tools/gen_single_header.py`（BMP 头会被折进 `single_include/termchart.hpp`），再 `gen_single_header.py --check` 必须过。
8. 提交：`bmp.hpp` `bmp.cpp` `test_bmp.cpp` `decoder.cpp` `tests/CMakeLists.txt` `single_include/termchart.hpp`。**务必带上 CMakeLists 与单头**，否则工程不自洽（见仓库教训：新测试登记进 CMake 后提交必须带 CMake 改动）。

> 注：单头 `--check` 当前"失败"纯因这 3 个未提交文件被生成器 glob 进单头；落地并 regen 后即恢复一致。

## 4. 剩余路线图与建议优先级

来源：用户最初 7 项 + 仓库 `backlog.md` / `ROADMAP.md`（权威口径）。

- **[P0] BMP 收尾**（§3）。最小可交付增量。
- **[P1] image 解码扩容 JPG / GIF / WebP**：
  - 决策建议：**vendored `stb_image`**（可选依赖，与现有 libpng/zlib 同模式，默认 OFF、缺失即 fail-loud `Unsupported`）。**不要**自主写 JPEG 解码器（3000+ 行、Huffman/采样因子/渐进式正确性风险高；"零强制依赖"不冲突——libpng 已是先例）。
  - `decoder.hpp` 已预留 `ImageFormat::Jpeg/Gif/WebP` 枚举与 `Unsupported` 分支；落地时只需在 `decoder.cpp` switch 加 case 调 stb，并补 vendored 头与 CMake option。
  - GIF：按需只解首帧（静态图库语义），避免引入动画时序复杂度。
- **[P1] 3D 全量**：`util/math3d`（vec3/mat4/quat）+ camera（视锥体裁剪）+ zbuffer + 3D 散点/曲面图。约 3000 行，**新模块需登记层号**（在 `cmake/TermchartModules.cmake` 加条目，未登记会让 `check_layers` 报错）。优先先评估放哪层（建议紧邻 `scene`/`render` 之下，L4–L5 区间）。
- **[P2] 80 种图收尾**：先核 `docs/reference/capability_matrix.md` 第五节各图交互完成度，避免重复造已存在的图。
- **[P2] 文档与画廊**：`docs` 同步（api.md 等）、`python tools/gen_gallery.py` 重生成（依赖已构建示例二进制，刻意不进 `check_generated`）。

## 5. 架构红线（违反即 CI 红 / 工作树损坏）

1. **分层强制**：模块 = `include/termchart/` 一级目录，层号在 `cmake/TermchartModules.cmake`（`TERMCHART_MODULE_LAYERS` / `NARROW_EDGES` / `FORBIDDEN_EDGES`）。改/加一级目录必须登记。`tools/check_layers.py` 执行，判定顺序：禁止边 → 收窄边 → 层号。禁止边含 `backend>chart`、`chart>render`、`render>interaction`。
2. **每头独立编译**：`tools/check_header_only.py`。新增公开头必须带 `src/` 同名 `.cpp`（基类也要虚拟表锚点）或登记进 `cmake/header_only.txt`；GUI 平行栈会把头单独 include。
3. **生成物禁手改**：`include/termchart/detail/*.hpp`、`结构.md` 附一/附四、`docs/reference/module_matrix.md`、`single_include/termchart.hpp`、`tests/golden/*`。只能跑生成器（`tools/gen_*.py`）；`--check` 非零即不一致。改动 `#include` 必跑分层校验。
4. **零强制依赖**：可选依赖走 CMake option，默认 OFF，缺失只降级不失败（libpng/zlib/utf8proc 现状）。vendored 第三方（如 stb_image）放 `third_party/`，保持可关闭。
5. **fail-loud（用户明确取向）**：可选扩展缺失显式报错 `Unsupported` / `ParseError`，**不静默降级**（仅针对可选扩展；核心库零依赖永不失败）。
6. **中文路径陷阱**：工作区路径含中文时，MinGW 下别把构建目录外的绝对路径源文件交给 Ninja（会生成带中文的嵌套 obj 名，GCC 写不出 `.obj`）。一律用 ninja 预设 + 仓库内相对路径登记。
7. **宽字符续接格**：生产者写 buffer 必须自己填 `kContinuationCp`；渲染器跳过它。漏了宽字形右半边会被空格覆盖。
8. **SGR 唯一出口**：`render/sgr.hpp` 是唯一 SGR 序列生成点，snapshot 后端也复用它；别处不许拼 SGR。
9. **golden 不比字节**：`tests/golden/*` 把渲染字节流解析回属性（码点/前景/背景/属性位）再比对，避免把"先发 fg 还是 bg"固化成契约。
10. **单头生成顺序**：先 `check_layers` + `check_header_only`，再 `gen_single_header`；新增头会进单头，必须 regen 并 `--check`。

## 6. 已知坑（逐项对应，跳坑即返工）

- BMP 调色板是**交错 RGB 三元组**（R0G0B0…），不是三段平面——早期 PNG 夹具踩过。
- BMP 负 height = top-down（数据首行是图像 y=0），正 height = bottom-up（数据首行是图像末行）。两者行序相反，别搞反。
- 32bpp 无 BITFIELDS 时默认 BGRX（alpha 字节通常无效），要按"有 alpha 掩码才读 A"。
- TTF 纯直线轮廓（方形 / `.notdef`）**连续 on 点也要成段**——漏了整轮廓被丢（本会话 TTF 修过此 bug）。
- TTF 浮点抖动：`(1-t)+t ≠ 1` 会让"直线段"产生 1e-14 倾斜，整覆盖区出现 63/255 缝——`ttf_font.cpp` 的 `add_edge` 已做近轴吸附。
- 单头 `--check` 报"已被引用但文件不存在" = 某头被 `#include` 但文件缺失或生成物陈旧——先 regen，别手改单头。

## 7. 验证命令速查

```bash
# 配置 + 构建（首选 ninja 预设）
cmake --preset ninja && cmake --build --preset ninja
# 测试
ctest --preset ninja
# 单用例最快：直接跑 doctest 二进制
./build/ninja/tests/termchart_tests.exe --test-case=*image/bmp*
# 护栏四件套
python tools/check_layers.py
python tools/check_header_only.py
python tools/gen_single_header.py --check
python tools/check_generated   # 或 cmake --build --preset ninja --target check_all
# 重生成产物（改动生成器或 #include 后）
python tools/gen_single_header.py
```

## 8. 接手第一步（最小动作）

收尾 BMP（§3 清单 1–8）。落地后主干即恢复全绿、单头自洽，再按 §4 推进 image 扩容 / 3D。

---

### 附：本会话**未做**的事（避免误以为已完成）

- 未编译 / 未提交 BMP（见 §3）。
- 未开始 JPG / GIF / WebP（stb_image 方案待拍板落地）。
- 未开始 3D。
- 未核对 80 种图的实际缺口（capability_matrix 待刷新）。
- 未跑本会话内的全量 ctest（依赖上次提交的历史绿；建议接手者跑一遍）。
