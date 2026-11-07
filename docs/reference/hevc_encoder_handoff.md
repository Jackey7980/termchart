# HEVC 编码器交接：真帧内压缩档（2026-11-07）

> 本文只谈 **自写 HEVC 编码器的第二阶段（真压缩）**。第一阶段 I_PCM 档是已交付的黄金基线，
> 不在本文范围，也不得被改动。写完本文时 I_PCM 基线实测仍 **6/6 GREEN、20433 字节、sha 前缀
> `5d4adef591829b46e4116e5c` 逐字节不变**。

## 0. 一句话现状

`VideoEncodeMode::Intra` 已从「发合法语法 + 合成系数」升级为**真实帧内管线**
（DC 预测 → 正向变换 → 量化 → 用量化后 level 重建 → 真实残差 CABAC）。
码流能被 ffmpeg 解出、不报错，**但编码端重建与解码端像素尚未逐位一致**：
纯色/近纯色图误差 0–2 灰阶，含真实残差的图仍有 TU 级累积偏差（最大 24–70 灰阶）。
本轮已定位并修掉一个**必然导致算术解码失步**的上下文 bug（见 §4），
剩余偏差已收敛到「重建链路 rounding / deblocking」这一层，不再是失步级错误。

## 1. 两个档位必须先分清

| 档位 | 入口 | 状态 |
| --- | --- | --- |
| `VideoEncodeMode::Pcm`（**默认**） | `hevc_encode.cpp` 的 `build_pcm_*` 一路 | 已交付。`diag_hevc.py` 6/6 门禁，字节级/sha 级锁死，**禁止改动** |
| `VideoEncodeMode::Intra` | `hevc_encode.cpp` 的 `build_intra_idr_slice` | 本阶段目标。可解、重建未完全对齐 |

`EncodeOptions::video_mode` 默认仍是 `Pcm`，所以**任何**默认路径（`encode_mp4`、示例、ctest）
都不会走到新代码——这是当前刻意保留的隔离带。未实现的能力一律返回空串（fail-loud），
没有静默降级。

## 2. 本次改动清单（尚未提交）

| 文件 | 改动 |
| --- | --- |
| `src/image/codec/hevc/quantize.cpp` | 整表重写为 HEVC 语义：`level_scale={40,45,51,57,64,72}`、`shift = bd+log2-5 = 5`、`scale_m=16`、`add = 1<<(shift-1)` |
| `src/image/codec/hevc/transform.cpp` | 新增 `fwd_transform_4x4(residual, coeff, use_dst)`（DCT 用 x265 `g_t4`，DST 用规范表 8-3，两段 `>>1`/`>>8`）；逆变换逐常量复刻 ffmpeg `idct_4x4` / `transform_4x4_luma`（先列 `>>7`、后行 `>>12`、`clip_int16`） |
| `src/image/codec/hevc/transquant.{hpp,cpp}` | `encode_tu_4x4` / `reconstruct_tu_4x4` 增加 `bool use_dst`（亮度 4×4 走 DST-VII，色度走 DCT） |
| `src/image/codec/hevc/intra_prediction.{hpp,cpp}` | 旧接口（`IntraMode` + `fill_reference` + `predict`）整体删除，换成 `predict_intra(mode, n, top, left, has_top, has_left, dst)`，**mode 0=Planar、1=DC**（与 HEVC 模式编号一致） |
| `src/image/codec/hevc/residual_coding.cpp` | 修 `next_greater1_ctx` 的上下文演化（见 §4）；escape 分支已实现 |
| `src/image/hevc_encode.cpp` | `build_intra_idr_slice` 换成真实管线：维护 YUV420 重建帧 `rec`、逐 TU 取 top/left 参考、`predict_intra` + `encode_tu_4x4` 写回 rec、把系数从行主序重排成对角扫描序后交给 `encode_cu_intra_8x8`；另外修了 `code_tree` 的 `split_cu_flag`（见 §9） |

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

## 5. 已定位但未修：编码端没有做 deblocking，解码端一定会做

`pps` 里 `deblocking_filter_control_present_flag = 0` → slice header 不出现
`slice_deblocking_filter_disabled_flag`，按规范**推断为 0，即去块滤波启用**。
ffmpeg 因此必然在 CU/TU 边界做滤波，而编码端的 `rec` 是「预测 + 逆变换」的裸结果。

这解释了两件事：

1. `flat`/`dc` 只有 0–2 灰阶差（全平图没有边界阶跃，滤波不改变像素）；
2. `hstep`/`grad` 在块边界处稳定出现 1–8 灰阶差。

`ffmpeg -skip_loop_filter all` 在 v9 上**无效**（我已试过，输出不变），不能靠它绕过。

两条出路，**建议先做 A**：

- **A（推荐，1 小时内）**：PPS 打开 `deblocking_filter_control_present_flag=1`，
  slice header 写 `slice_deblocking_filter_disabled_flag=1`。这样解码端不做滤波，
  「编码端重建 == 解码端输出」这个判据才成立，才能继续定位真正的残差/变换差异。
- **B（正确但更重）**：编码端实现 HEVC 去块滤波（边界强度、bS 判定、阈值表），
  与变亮/变暗的累积效应一并消失；顺带提升压缩质量。

在 A 落地之前，**任何 ≥3 灰阶的偏差都不能确定是残差链路的锅**。

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

1. 先做 §5 的 A（关 deblocking），把所有偏差降到「纯链路」层面再谈；
2. 对齐 `dequant_4x4` 的舍入：现在 `add = 1<<(shift-1) = 16`、`shift=5`。核对
   ffmpeg 的 `add` 是否与 `scale_m`（16）和负 level 的处理完全一致——负数右移的
   舍入方向最容易差 1；
3. 对齐 `inv_transform_4x4` 的 `clip_int16` 位置：ffmpeg 的两段 `SCALE` 都是
   `av_clip_int16((x + (1<<(shift-1))) >> shift)`，第一段也必须夹；
4. 若仍差 1，改用「单 TU 隔离法」：构造只有 1 个 8×8 CU、其余全黑、且让首 TU 的
   pred 恒为 128 的图，直接对比两侧该 TU 的 16 个像素，把预测因素排除掉。

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

1. **P0**：§5 的 A（关 deblocking），拿到「重建可逐位对齐」的干净判据。
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
6. **`-skip_loop_filter all` 在 ffmpeg v9 上无效**，别指望它验证无滤波路径。
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
