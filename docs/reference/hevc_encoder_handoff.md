# HEVC 编码器交接：真帧内压缩档（2026-11-07）

> 本文只谈 **自写 HEVC 编码器的第二阶段（真压缩）**。第一阶段 I_PCM 档是已交付的黄金基线，
> 不在本文范围，也不得被改动。写完本文时 I_PCM 基线实测仍 **6/6 GREEN、20433 字节、sha 前缀
> `5d4adef591829b46e4116e5c` 逐字节不变**。

## 0. 一句话现状（2026-11-08 更新）

> **本节历史上是「尚未逐位一致」的攻坚记录，现已过时。** 阶段一出口判据全部达成，
> 后续执行依据请直接看 `docs/reference/hevc_engineering_roadmap.md`。

`VideoEncodeMode::Intra` 是**真实帧内管线**（DC 预测 → 正向变换 → 量化 →
用量化后 level 重建 → 真实残差 CABAC）。当前实测：

- 编码端重建与 ffmpeg 解码 **8 种形态全部逐位一致**（`maxdiff=0 / 0 of 8192`）；
- 解码 Y-PSNR 39.21~63.76 dB，QP=26 为 40.28 dB（判据 >35 dB）；
- 64×64 体积 6353 B → 187 B，比 I_PCM **降 97.06%**（判据 ≥80%）；
- QP 22/26/30/34 体积 971→716→504→346 B、PSNR 43.75→40.28→37.24→34.48 dB，均单调；
- `trace_headers` 确认 `pcm_enabled_flag=0`、`slice_qp_delta=qp-26`；
- **工业级硬解通过**：Windows d3d11va 真实启用（`-hwaccel_output_format d3d11`
  强制，禁静默回退），4 尺寸 × 4 QP × 4 形态共 **64/64** 组 Y 平面与软解逐位一致。
  门禁：`python tools/verify_hevc_hwaccel.py`。

发散的真因不是 rounding 或去块滤波，而是 `predict_dc` 缺 HEVC 8.4.4.2.2
参考样本替代与 8.4.4.2.5 首行/首列边界滤波（常数图下两种写法恒等，掩盖了偏差）。
已由 `tests/unit/image/test_intra_prediction.cpp` 锁死。**自往返测试不能替代外部 oracle。**

下文 §1~§10 保留为攻坚期的过程记录（含已被证伪的 §5 判断），
用于说明「为什么不能只靠自测」，不作为当前状态结论。

## 1. 两个档位必须先分清

| 档位 | 入口 | 状态 |
| --- | --- | --- |
| `VideoEncodeMode::Pcm`（**默认**） | `hevc_encode.cpp` 的 `build_pcm_*` 一路 | 已交付。`diag_hevc.py` 6/6 门禁，字节级/sha 级锁死，**禁止改动** |
| `VideoEncodeMode::Intra` | `hevc_encode.cpp` 的 `build_intra_idr_slice` | 本阶段目标。可解、重建未完全对齐 |

`EncodeOptions::video_mode` 默认仍是 `Pcm`，所以**任何**默认路径（`encode_mp4`、示例、ctest）
都不会走到新代码——这是当前刻意保留的隔离带。未实现的能力一律返回空串（fail-loud），
没有静默降级。

## 2. 本次改动清单（已在 `f7aefbb` 提交）

| 文件 | 改动 |
| --- | --- |
| `src/image/codec/hevc/quantize.cpp` | 整表重写为 HEVC 语义：`level_scale={40,45,51,57,64,72}`、`shift = bd+log2-5 = 5`、`scale_m=16`、`add = 1<<(shift-1)` |
| `src/image/codec/hevc/transform.cpp` | 新增 `fwd_transform_4x4(residual, coeff, use_dst)`（DCT 用 x265 `g_t4`，DST 用规范表 8-3，两段 `>>1`/`>>8`）；逆变换逐常量复刻 ffmpeg `idct_4x4` / `transform_4x4_luma`（先列 `>>7`、后行 `>>12`、`clip_int16`） |
| `src/image/codec/hevc/transquant.{hpp,cpp}` | `encode_tu_4x4` / `reconstruct_tu_4x4` 增加 `bool use_dst`（亮度 4×4 走 DST-VII，色度走 DCT） |
| `src/image/codec/hevc/intra_prediction.{hpp,cpp}` | 旧接口（`IntraMode` + `fill_reference` + `predict`）整体删除，换成 `predict_intra(mode, n, top, left, has_top, has_left, dst)`，**mode 0=Planar、1=DC**（与 HEVC 模式编号一致） |
| `src/image/codec/hevc/residual_coding.cpp` | 修 `next_greater1_ctx` 的上下文演化（见 §4）；escape 分支已实现 |
| `src/image/hevc_encode.cpp` | `build_intra_idr_slice` 换成真实管线：维护 YUV420 重建帧 `rec`、逐 TU 取 top/left 参考、`predict_intra` + `encode_tu_4x4` 写回 rec、把系数从行主序重排成对角扫描序后交给 `encode_cu_intra_8x8`；另外修了 `code_tree` 的 `split_cu_flag`（见 §9） |

### 2.1 其后追加的一轮（P0-A，见 §5）

| 文件 | 改动 |
| --- | --- |
| `src/image/codec/hevc/parameter_sets.hpp` | `Pps` 增 `bool disable_deblocking = false`（POD，默认 false ⇒ 位序列与 G0 基线一致） |
| `src/image/codec/hevc/parameter_sets.cpp` | `write_pps` 按该开关写 `present=1 / override=0 / disabled=1`；否则沿用 `present=0` |
| `src/image/hevc_encode.cpp` | 真压缩档传 `Pps{.disable_deblocking = true}`；PCM 档仍传 `Pps{}` |
| `src/image/codec/hevc/slice_header.cpp` | 仅注释更新：说明 `override_enabled=0` ⇒ slice header 零新增比特 |
| `docs/reference/hevc_encoder_handoff.md` / `ROADMAP.md` | §5 改写为实测结论；G0 字节数/sha 口径校正为 20433 / `5d4adef5…` |

**后续状态（2026-11-09）**：`hevc_encode.cpp` 现传 `Pps{}`（解码端开滤波），并在
`build_intra_idr_slice` 出口调 `deblock_frame_luma`（`src/image/codec/hevc/deblocking.cpp`，
近似实现：只滤亮度、bS 恒 2、单 tc）。实测 recon vs 默认解码 Y 平面 maxdiff≈4
（QP26 / 128×64 渐变图）；`TERMCHART_HEVC_NO_DBLOCK=1` 编码 + `-skip_loop_filter all`
解码时 maxdiff=0，该组合才是"预测 / 变换 / 量化 / CABAC / 重建"的干净逐像素门禁。
§5.2 的"开关去块解码像素逐字节相同"只在 checker 图成立：grad 图 8 对齐的台阶边界
满足 `d < beta`，会被滤波改动（差 4）。

**验证**：`diag_hevc.py` 仍 6/6 GREEN、`tc_hevc.265` = 20433 B / sha `5d4adef5…`
（PCM 档位零影响）；六形态 Intra 复测结果与 §3 表格逐位相同（去块开关对像素无影响）。

## 3. 怎么验证（可复现）

```powershell
# 编译临时验证驱动（-- 需 -DTERMCHART_STATIC 与 -DTERMCHART_HEVC_RECON_DUMP）
g++ -std=c++20 -O2 -DTERMCHART_STATIC -DTERMCHART_HEVC_RECON_DUMP -Iinclude -Isrc -Isrc/image `
  -o build_scratch/intra_recon.exe _intra_recon_test.cpp `
  src/image/hevc_encode.cpp src/image/codec/bitstream/ep_nal.cpp src/image/codec/hevc/*.cpp

# 六种测试形态（第一个参数）
./build_scratch/intra_recon.exe flat     # 全平 118
./build_scratch/intra_recon.exe dc       # 全平 100
./build_scratch/intra_recon.exe hstep    # 8x8 块内平坦、块间跳变 → 每 TU 仅 DC、幅值大
./build_scratch/intra_recon.exe hgrad4   # 弱水平渐变 → 少 AC、小 level
./build_scratch/intra_recon.exe checker  # 2x2 棋盘 ±8 → 多非零系数、小 level
./build_scratch/intra_recon.exe grad     # 强渐变 + ±14 棋盘 → 多系数 + 大幅值
```

驱动做的事：构造 128×64 位图 → `encode_hevc_annexb(frames, {.video_mode=Intra})` →
写 `tc_intra.265` + 编码端重建 `tc_enc_recon.yuv` → 调 ffmpeg 解码到 `tc_dec_ref.yuv` →
逐像素比较，打印 `maxdiff` / 不匹配数 / 编码端与解码端各自的 Y-PSNR / 前 20 个像素。

**注意**：`tc_enc_recon.yuv` 是 `fopen("...","ab")` 追加写的，驱动里已加
`std::remove("tc_enc_recon.yuv")` 清场。**不要去掉**——否则读到上一次运行的残留，
会得出「编码端重建完全错误」的假结论（本轮踩过）。

### 本轮实测数据（`next_greater1_ctx` 修复后）

| 形态 | 编码端 Y-PSNR | 解码端 Y-PSNR | maxdiff | 不匹配 |
| --- | --- | --- | --- | --- |
| flat | 48.09 | 48.08 | 1 | 5 / 8192 |
| dc | 64.04 | 63.76 | 2 | 8 / 8192 |
| hstep | 45.12 | 36.53 | 8 | 7504 / 8192 |
| hgrad4 | 48.95 | 39.70 | 4 | 6638 / 8192 |
| checker | 39.40 | 19.53 | 70 | 8172 / 8192 |
| grad | 40.41 | 26.22 | 24 | 8176 / 8192 |

修复前 `grad` 是 **12.26 dB / maxdiff 188 / px[0] dec=128 enc=39**（典型失步）；
现在是 26.22 dB / 24 / px[0] 完全相等。**失步已经不存在，剩下的是数值级偏差。**

## 4. 已修复：`coeff_abs_level_greater1_flag` 的上下文演化

`residual_coding.cpp` 原实现：

```cpp
inline int next_greater1_ctx(int g, int flag) {
    if (flag) return 0;
    return g < 3 ? g + 1 : 3;      // ← g==0 时会给出 1
}
```

ffmpeg 的真身是（`_ff_cabac.c`，`hls_residual_coding` 内）：

```c
greater1_ctx = (greater1_ctx + (greater1_ctx - 1U < 2)) & (flag - 1);
```

逐项展开：`(g - 1U < 2)` 只在 `g==1` 或 `g==2` 时为真（`g==0` 时 `0-1U` 回绕成 `UINT_MAX`），
`flag==1` 时 `& (1-1)` 清零、`flag==0` 时 `& 0xFFFFFFFF` 保持。于是正确演化是：

- `flag==1` → 0
- `flag==0` → `g==1||g==2` 则 `g+1`，**`g==0` 保持 0**，`g==3` 保持 3

错写 `g+1` 的后果：一个 `level>1` 的系数把 `g` 归零后，紧随其后的 `level==1` 系数会用
`ctx+1` 编码，而解码端用另一个概率模型 → **算术解码器从此跑偏**。

为什么之前一直没被发现：**单非零系数的 TU 只会编码一个 gt1 bin（`g` 恒为初值 1），
根本不触及 `g==0` 分支**。所以 `flat`/`dc`/`hstep`（每 TU 只有 DC）一切正常，
只要 TU 里出现「两个以上非零系数」就整块崩坏——`checker` 与 `grad` 正好是这个形状。

同时提醒：`_residual_test.cpp`（残差语法自往返测试）**修前修后都 PASS**，
因为它用的解码器出自同一份误解。**自往返测试不能替代外部 oracle**，这类 bug 必须靠
ffmpeg 逐像素比对才能抓到。

## 5. A 已落地并实测：去块滤波**不是**本批码流的偏差来源（原判断被证伪）

原判断是「`pps` 里 `deblocking_filter_control_present_flag=0` → 解码端必然滤波，
而编码端的 `rec` 是裸重建，所以 1–8 灰阶的边界差是滤波造成的」。**实测不成立。**

### 5.1 已落地的实现（只改 PPS，slice 零新增比特）

`Pps` 增 `bool disable_deblocking = false`。置 true 时 `write_pps` 写
`deblocking_filter_control_present_flag=1` / `deblocking_filter_override_enabled_flag=0` /
`pps_deblocking_filter_disabled_flag=1`，并**不写** `pps_beta/tc_offset_div2`
（disabled=1 时按 7.3.2.3 不出现，推断为 0）。

`override_enabled=0` 是关键：按 7.3.6.1，`deblocking_filter_override_flag` 随之被推断为 0，
于是 `slice_deblocking_filter_disabled_flag` **直接继承 PPS 的 1**——所以
`write_slice_header()` **一个比特都不用改**（`cbs_h265_syntax_template.c:1761-1779`
与 `_hevcdec.c:1099-1129` 两处实现互证）。真压缩档传
`Pps{.disable_deblocking = true}`，PCM 档仍传默认 `Pps{}`，位序列逐字节不变。

### 5.2 实测：开/关去块，解码像素逐字节相同

同一份 `checker` 码流，只翻转这个 PPS 开关：

| | 码流 | PPS 实参 | 解码 Y 平面 |
| --- | --- | --- | --- |
| 去块开（原状） | 294 B | `present_flag=0` | `6B482727A4CDB002…` 12288 B |
| 去块关（P0-A） | 295 B | `present=1 / override=0 / disabled=1` | `6B482727A4CDB002…` 12288 B |

两边 sha 完全相同。PPS 解析用 `ffmpeg -bsf:v trace_headers` 逐字段核对过，
确认 `deblocking_filter_control_present_flag` 与 `pps_deblocking_filter_disabled_flag`
的取值与预期一致，不是「没写进去」。六个形态（flat/dc/hstep/hgrad4/checker/grad）
重跑后 PSNR、maxdiff、mismatched 与 §3 表格**逐位相同**。

### 5.3 结论与影响

1. **§6 的 DC 偏差不能归给滤波**。原先「`hstep`/`grad` 在块边界稳定 1–8 灰阶差 = 滤波」
   的推断作废——关掉滤波后差值一点没变，说明这些偏差来自残差/重建链路本身。
2. 判据现在已经干净：编码端 `rec` 与解码端输出的差异可以直接当作链路误差来查，
   不必再为「是不是滤波」留余量。这一条仍然值得保留（B 方案一旦落地就需要它）。
3. **遗留疑问（值得单开一条）**：为什么去块对这批码流零影响？两种可能——
   ① 我们的语法子集让 `l->bs` 全零（边界强度根本没被点亮），这本身是个应该查的缺陷；
   ② 阈值（QP 26 的 beta/tc）把边界全判成「不滤波」。
   无论哪种，都说明「解码端在滤波」这个前提在本流上从未真正生效。
4. `ffmpeg -skip_loop_filter all` 确认无效（同一码流加不加该开关，解码输出 sha 相同）。
   **B 方案（编码端实现 DBF）优先级相应下调**：它提升的是压缩质量，不再承担
   「让判据成立」的职责。

### 5.4 复现

```powershell
# 去块开：临时把 Pps{.disable_deblocking=true} 改成 Pps{}，编译到另一个 exe
./build_scratch/intra_recon.exe checker                 # 关
./build_scratch/intra_recon_on.exe checker              # 开（A/B 用）
# 各自产出 tc_dec_ref.yuv，比 sha256 即知是否受影响

# 核对 PPS 实参（不信编码器自述）
ffmpeg -i tc_intra.265 -c copy -bsf:v trace_headers -f null - 2>&1 |
  Select-String 'deblocking_filter_control_present_flag|pps_deblocking_filter_disabled_flag'
```

## 6. 未定位：多系数 TU 的 DC 重建系统性偏 1–2

`checker` 的逐像素偏差形状非常有信息量（`dx` 是 `dec - enc`）：

```
x= 0.. 3  d = 0   0   0   0      ← TU1 全对
x= 4.. 7  d = +2  +2  +2  +2     ← TU2 整体抬升
x= 8..11  d = +6  +5  +5  +5     ← TU3
x=12..15  d = +7  +7  +7  +7     ← TU4
x=16..19  d = +9  +9  +9  +9
```

**同一 TU 内偏差恒定、跨 TU 阶跃累积**——这是「该 TU 的重建 DC 与编码端不同」的指纹，
不是滤波（滤波只改边界行列），也不是失步（失步会差上百且无这种规律）。

`hstep` 同样如此（TU2 +2、TU3 +1、TU4 +1），而它每 TU 只有一个 DC 系数。
所以问题既不在「多系数」也不在「大幅值」，而在**level 很小时的最小可分辨重建**：
level 差 1，DC 差约 3 灰阶，然后被下一 TU 的预测继承、逐块放大。

排查建议顺序（每步都用 §3 的命令复测六种形态）：

1. ~~先做 §5 的 A（关 deblocking）~~ **已完成**：PPS 已关滤波，且实测对像素零影响
   （§5.2），因此 §6 的偏差**与滤波无关**，直接从第 2 步开始；
2. 对齐 `dequant_4x4` 的舍入：现在 `add = 1<<(shift-1) = 16`、`shift=5`。核对
   ffmpeg 的 `add` 是否与 `scale_m`（16）和负 level 的处理完全一致——负数右移的
   舍入方向最容易差 1；
3. 对齐 `inv_transform_4x4` 的 `clip_int16` 位置：ffmpeg 的两段 `SCALE` 都是
   `av_clip_int16((x + (1<<(shift-1))) >> shift)`，第一段也必须夹；
4. 若仍差 1，改用「单 TU 隔离法」：构造只有 1 个 8×8 CU、其余全黑、且让首 TU 的
   pred 恒为 128 的图，直接对比两侧该 TU 的 16 个像素，把预测因素排除掉。

> 补充线索（§5.3 遗留疑问）：若发现 `l->bs` 全零导致去块从未真正生效，说明我们
> 发出的 CU/TU 边界信息与解码端理解不一致——那本身就是失配的另一个证据，值得
> 用同一套逐像素判据一并查。

## 7. 当前支持的语法子集（超出即不支持）

- 分辨率必须 `W % 64 == 0 && H % 64 == 0`，否则返回空串（CTB 对齐 padding 还没做）；
- 4:2:0、8bit、Main profile、`pcm_enabled = 0`（真压缩档内）；
- CU 固定 8×8（`PART_2Nx2N`），`max_transform_hierarchy_depth_intra = 1` → 每个 8×8 CU 拆 4 个 4×4 TU，z 序；
- 帧内预测**只用 DC**（`mpm_idx = 1`；因为全部邻居也是 DC，`candModeList` 恒为
  `{Planar, DC, 26}`，DC 的索引恒为 1）。35 种角度模式、Planar 都还没进决策；
- 色度：`intra_chroma_pred_mode = 4`（DM），但 `cbf_cb = cbf_cr = 0`，**色度没有任何残差修正**；
- 无 SAO、无去块滤波、无 32×32/16×16/8×8 TU、无 RDO（不做模式/深度搜索）、无码率控制；
- 仅 `SCAN_DIAG`、单 CG、`sign_data_hiding_flag = 0`、无 scaling list、无 transform skip。

## 8. 建议的下一步顺序

1. ~~**P0**：§5 的 A（关 deblocking）~~ **已完成**（只改 PPS、slice 零新增比特）。
   实测去块对像素零影响，判据已干净；下一步直接进第 2 条。
2. **P0**：按 §6 的 2/3/4 步把 DC 偏 1 的问题压到 0；`checker` 跑到 `maxdiff=0` 才算过关。
3. **P1**：把这套验证固化成**正式测试**（现在 `tests/` 里对 HEVC 内部模块零覆盖，
   全靠根目录一堆 `_*.cpp` 临时文件）。建议 `tests/unit/image/test_hevc_intra.cpp`：
   用内置的迷你 CABAC 解码 + 逐像素判据，不依赖本机 ffmpeg。
4. **P1**：色度残差（`cbf_cb/cbf_cr` + 4×4 色度 TU），否则彩色图质量上不去。
5. **P2**：TU 尺寸扩展（8×8/16×16/32×32）→ 角度模式 → RDO → 码率控制。
6. **P2**：分辨率不整除 64 时的 CTB 边界处理（而不是直接返回空串）。

## 9. 本轮踩过的坑（别再踩）

1. **`split_cu_flag` 在叶子层不写**：`log2_cb_size == log2_min_cb_size`（8×8）时，
   ffmpeg 不解码这个 flag（`split_coding_unit_flag_decode` 只在 `log2_cb_size > 3` 时调用）。
   写了就整帧偏一个 bin。
2. **`split_cu_flag` 的上下文不是固定的**：`ctx = kCtxSplitCuFlag + (depth_left > ct_depth) +
   (depth_top > ct_depth)`，邻居编码深度要从 **min-CB 粒度**的 `ct_depth` 表取。用错上下文
   同样会静默失步（ffmpeg 不报错，只是像素全错）。
3. **变换输出是行主序，残差语法要对角扫描序**：必须
   `scan[n] = raster[kDiagScan4x4Y[n]*4 + kDiagScan4x4X[n]]`，漏了就整块乱。
4. **`luma4x4[i]` 传 `nullptr` 表示该 TU 无残差**，不要为了「凑数」塞小系数——
   会强行点亮 `cbf_luma`，把后续 bin 全带偏。
5. **重建必须用量化后的 level**：用原始变换系数重建会让编码端参考与解码端漂移，
   误差沿扫描顺序累积（这正是「为什么必须维护 `rec` 而不是直接拿源像素当参考」）。
6. **`-skip_loop_filter all` 无效**（v9 与我实测的 8.1.2 都是：加不加该开关，解码
   输出 sha 相同），别指望它验证无滤波路径。要关滤波只能改 PPS（见 §5.1）。
7. **自往返测试抓不到「共同误解」**：`_residual_test.cpp` 全 PASS 的同时 ffmpeg 解码是错的。
   外部 oracle（ffmpeg）是唯一真判据。

## 10. 工作树卫生

- 需要保留的验证入口：`_intra_recon_test.cpp`（六形态逐像素比对）、
  `hevc_driver.cpp`（生成 I_PCM 黄金码流）、`diag_hevc.py`（6/6 门禁）。
- 交付前应删除：`_b2_test.cpp`（引用了已删除的 `IntraMode` / `predict`，
  **现在编译不过**）、`tc_enc_recon.yuv` / `tc_intra.265` / `tc_dec_ref.yuv` /
  `_verify_pcm.yuv` 等运行产物。
- 根目录还堆着大量历史调试脚本（`scan_*.py` / `probe_*.py` / `check_*.py` / `solve_*.py` 等
  几十个，多为 I_PCM 阶段反解 codILow 时留下），**都不在 CMake/ctest 里**，
  不影响构建，但会干扰 `git status`。清理时注意别删 `diag_hevc.py` 与 `hevc_driver.cpp`。
- 本仓库工作树常有多 Agent 未提交改动：提交时必须精准暂存，**不要 `git checkout .`**。
