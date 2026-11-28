# HEVC 编码器：可执行工程拆分与演进路线

> 本文是**执行依据**，不是现状陈述。所有「已完成」判定以代码与实测为准
> （`ROADMAP.md` 的「项目现状」节历史上多次落后，不作为结论来源）。
> 配套：阶段一交接细节见 `hevc_encoder_handoff.md`，进度勾选以 `ROADMAP.md` 为准。
>
> 核心原则：**先让 I_PCM 像素级严格正确，再拆文件**；拆文件按依赖倒序
> （先基建 → 再熵编码 → 再变换/预测 → 最后 P 帧与环路滤波）。
> 每拆一个文件都必须用 `diag_hevc.py` 的严格字节比较回归，任一破坏立即回退。

---

## 0. 门禁总表（任何一步都得先绿）

| 门禁 | 命令 | 判据 | 当前 |
| --- | --- | --- | --- |
| G0 I_PCM 黄金基线 | `python diag_hevc.py` | 6/6 GREEN；`tc_hevc.265` = 20433 B / sha `5d4adef5…`；`dec_U[0:16]==enc_U[0:16]` 且 `Y[0]==16` **逐字节相等**，不是"接近" | 6/6 GREEN |
| G1 分层 | `python tools/check_layers.py` | exit 0，禁止边/收窄边/层号无新增豁免 | PASS |
| G2 头自洽 | `python tools/check_header_only.py` | exit 0 | PASS |
| G3 生成物 | `cmake --build --preset ninja --target check_all` | 分层/自洽/生成物/451 头独立编译全过 | PASS |
| G4 全量测试 | `ctest --preset ninja` | 全绿，报告写具体数字 | 91/91（doctest 1421 用例 / 239157 断言） |
| G5 **工业级硬解** | `python tools/verify_hevc_hwaccel.py` | 64/64（4 尺寸 × 4 QP × 4 形态）硬解真实启用，Y 平面与软解 **maxdiff=0**；`slice_qp_delta=qp-26`；`pcm_enabled_flag=0` | 64/64 PASS，hardware=ACTIVE |
| G6 体积 | QP 扫描 | 64×64 比 I_PCM 降 ≥80%；QP 22/26/30/34 体积与 PSNR 单调 | ≥80% 达成；1432→924→703→498 B / 43.55→39.08→36.42→34.32 dB（含色度残差） |
| G7 **色度** | `build_scratch/verify_chroma.py` | 5 形态 × 4 QP：编码端 recon U/V == ffmpeg dec U/V **maxdiff=0**；彩色图 U/V PSNR ≥37 dB | 20/20 PASS |

**G5 的硬解不得静默回退**：门禁用 `-hwaccel_output_format` 强制硬件帧格式，
请求硬解却回退到软解会直接失败。CI 无 GPU 时必须显式 `--no-hw` 并报告
`hardware=SKIPPED`，不能把跳过写成通过。

---

## 1. 与原始方案的差异（必须先读，避免照抄引入回归）

原始任务清单写于 I_PCM 攻坚期，有 4 处与当前代码冲突，执行时以代码为准：

| 清单条目 | 冲突 / 修正 |
| --- | --- |
| §一.3「CABAC 引擎：bitsToFollow 三分支 renorm、FlushD 输出 bit9 + pending×(1-bit9) + bit8」 | **不成立**。`diag_hevc.py:15` 明确：当前 I_PCM 档是「按区间反解 codILow」，字面 `bitsToFollow` 不适用；真压缩档用 FFmpeg 风格 `CabacArith`（`queue_`/`outstanding_`）。历史上误重写成 bitsToFollow 版导致 decision 流错约 50%，已回滚。门禁断言的是**语义不变量**（无首比特反转、range=510、终止 bin、PCM 后重置），照抄实现会毁掉 G0。 |
| §一.4「PCM 后 low=0, range=510, bits_to_follow=0，上下文表不重置」 | 语义保留，字段名按当前实现：`CabacArith::init()` 重置 `low_/range_/queue_/outstanding_`，`CabacContext` 不重置。 |
| §三.3「`ctb_tree` 的 `split_cu_flag` ctxInc = `(x0>0)+(y0>0)`」 | **不准确**。FFmpeg `luma_intra_pred_mode` / `hls_coding_quadtree` 用的是 `ct_depth` 邻居深度比较（`depth_left > ct_depth`），当前 `hevc_encode.cpp::code_tree` 已按此实现。用简化式会让解码端算术解码器跑偏（整帧错但 ffmpeg 不报错）。 |
| §一.6「FFmpeg 3 帧 0 error、总字节数精确」 | 仍成立，但**不足以称工业级**：软件解码通过不代表硬解通过。必须叠加 G5。 |

另外，清单 §二的目标目录里 `residual_coding.*`、`cu_coding.*`、`cabac_arith_dec.*`
是攻坚期新增、但清单未列出的模块 —— 属于必要的实现细化，保留。

---

## 2. 真实状态盘点（2026-11-08，代码 + 实测）

### 2.1 阶段 A：拆文件 —— 已完成

```
image/codec/
  bitstream/   bit_writer.hpp  ep_nal.hpp/cpp
  hevc/        nal_unit / parameter_sets / slice_header
               cabac_engine / cabac_context / cabac_syntax
               cabac_arith / cabac_arith_dec        (真压缩档算术编解码)
               transform / quantize / transquant
               intra_prediction / residual_coding
               cu_coding / ctb_tree                  (16 个模块，均 .hpp + .cpp)
```
I_PCM 档保持自包含、`diag_hevc.py` 字节级锁死，未被拆分污染。

### 2.2 阶段 B（真帧内压缩）—— 出口判据全部达成

| 判据（ROADMAP:484 / 清单三） | 2026-11-10 实测（含色度残差） |
| --- | --- |
| FFmpeg 0 error | rc=0 |
| PSNR > 35 dB（固定 QP=26） | **39.08 dB**（grad / 128×64 / 默认开 DBF + 35 模式决策）；固定 DC 时代 38.40 dB、关 DBF 时代 40.28 dB 是另一口径，不可混用 |
| 64×64 体积比 I_PCM 降 ≥80% | 6353 B → 187 B，**降 97.06%**（固定 DC 基线）；模式决策后体积略增但降幅仍 >90% |
| QP 22/26/30/34 体积单调 | 1432 → 924 → 703 → 498 B |
| QP 22/26/30/34 PSNR 单调 | 43.55 → 39.08 → 36.42 → 34.32 dB |
| 关 PCM 后码流无 `pcm_flag` | `trace_headers`：`pcm_enabled_flag=0` |
| 编码端重建 == ffmpeg 解码 | 128×64 / 256×128 / 640×384 三尺寸下 **35/35 模式** 与 **多 CTB 模式决策** 均 `maxdiff=0`（`NO_DBLOCK=1` + `-skip_loop_filter all` 的干净路径） |
| **色度 U/V** | 编码端 recon 与 ffmpeg 解码逐像素一致（5 形态 × 4 QP 全 `maxdiff=0`）；彩色 grad / hstep 图 U/V PSNR **37.9~50.6 dB** |
| **工业级硬解** | d3d11va 真实启用，64/64 组 Y 平面与软解逐位一致，worst Y-PSNR 32.33 dB |

**根因修复记录**（避免重复踩）：编码端重建与解码端发散的真因是
`predict_dc` 缺 HEVC 8.4.4.2.2 参考样本替代与 8.4.4.2.5 首行/首列边界滤波；
常数图下两种写法恒等（`flat128` 逐位一致掩盖了偏差）。已由
`tests/unit/image/test_intra_prediction.cpp`（81 断言）锁死，oracle 取自
ffmpeg 解码像素反推。**自往返测试不能替代外部 oracle**。

### 2.3 真实缺口（2026-11-10 代码 + 实测）

**帧内预测本身已做完**：`predict_intra_luma_4x4` 覆盖 0..34 全部 35 个模式，
`choose_intra_luma_mode_8x8` 按真实 TU 次序逐 CU 做 SSD 模式决策，三套系数扫描
随模式切换。以下仍是缺口，按优先级登记：

| 编号 | 缺口 | 证据 / 现状 |
| --- | --- | --- |
| ~~P0-5~~ **已修（2026-11-10）** | 跨 CTB 行的 **bottom-left 扩展参考** | 根因是 `end_of_tiles_y` 被误当成整帧高度：FFmpeg `hevcdec.c:2753` 它是**当前 CTB 行底部**，故每个 CTB 行最后一行的 TU 关闭 bottom-left 扩展（`mvs.c:58`）。编码端按 `pic_h` 判定会读下一 CTB 行尚未编码的 `rec` 区。改为按 `min(((y0>>6)<<6)+64, pic_h)` 判定后，35 模式在 128×64 / 256×128 / 640×384 全部 `recon == dec` maxdiff=0，默认模式决策已**放开此前排除的 mode 0/2..9**。注：此前文档写的「top-right」是误判——坏的是用 L 参考的 2..9，用 T 参考的 26..34 一直正常。 |
| ~~P1~~ **已完成（2026-11-12）** | 8×8 / 16×16 / 32×32 TU + 自动档 + 大 TU `transform_skip` | 多 CG 残差语法、n×n 帧内预测、CU/transform 树分裂已落地；`tools/verify_hevc_tu_sizes.py` 三模式 360 条码流 ffmpeg 软解 `recon == dec`（`neq=0`）全绿，自动档比最佳单一档省 20.7%（关闭 skip）/ 相对关闭 skip 的最佳固定档省 21.0%（开启 skip）。`transform_skip` 已解除 4×4 互斥：RExt profile（`general_profile_idc=4`）+ `sps_range_extension` 配套开关 + **`pps_range_extension.log2_max_transform_skip_block_size_minus2`**（该字段在 PPS 而非 SPS，旧文档把它写错了）；8/16/32 TU 与色度（4×4 及大 TU 两条路径）都按各自平面 QP 的 RD 选 skip，`--force-skip` 端到端验证 `flag=1` 语法。`trace_headers` 逐字段核对已接入 `tools/verify_hevc_tu_sizes.py`，关闭 skip 的 Main 档核对不写 extension。 |
| ~~P1~~ **已完成（2026-11-10）** | 色度预测与色度残差 | DM 模式色度预测（复用亮度 35 模式逻辑 + `c_idx` 语义）、4×4 色度 DCT-II、`qPi→qPc` 色度 QP、`cbf_cb/cr` 与 U/V 残差 CABAC（`c_idx>0` 固定 ctx 偏移）全部落地。5 形态 × 4 QP 的 `recon U/V == ffmpeg dec U/V` maxdiff=0，彩色图 U/V PSNR 37.9~50.6 dB。 |
| ~~P3-1~~ **已完成（2026-11-13）** | DBF 精确化 | 亮度强/弱滤波与 tc 双段本就逐式对齐 FFmpeg（4×4/8×8 档 `maxdiff=0`，旧文档「亮度差≈4」过时）；本轮补齐色度 8.7.2.5 弱滤波、固定档 TU 边界周期、自动档逐 8×8 网格真实边界图，`--dblock-on` 60 组 Y/U/V 全部 `max=0 neq=0`。bS 恒 2 是全帧内码流的规范结论，不是近似。**剩余**：仅码率控制（P3-4）未开工；SAO（P3-2）与 RDO（P3-3）均已于 2026-11-14 落地，默认都关闭以保字节级基线。 |
| ~~P3-2~~ **已完成（2026-11-14）** | SAO（BO / EO / merge） | `src/media/encoder/hevc/sao.{hpp,cpp}` 完整实现；SPS `sample_adaptive_offset_enabled_flag` 默认 0，`EncodeOptions::hevc_sao` 显式开启。编码端按 DBF→SAO 顺序滤波，`sao()` 语法按 CTB 与 `coding_quadtree` 交织写回。72 组全量 oracle（6 形态 × 3 QP × 4 TU，on/off 各一条）经 ffmpeg 软解 `recon == dec neq=0`；`dots` 档 Y-PSNR **+1.7~+2.3 dB**、`grad` 档 +0.2~+0.5 dB，码率仅 +0.05~0.9%；默认关闭时码流与旧 7 参调用逐字节相同。详见下方「P3-2 收口记录」。 |
| ~~P3-3~~ **已完成（2026-11-14）** | RDO（模式 + CBF） | `EncodeOptions::hevc_rdo` 默认关闭；开启后帧内模式按「约束式 RD」（纯 SSD 最优的失真余量内挑省 bin 的候选）、CBF 按 `cbf=0/1` 的 `SSD + λ·bins` 比较，模式语法 bin 由 `mode_signal_bins` 与 `put_*` 实拼对拍。CI `--rdo` 门禁 96 组全量 + quick，`neq=0`、参数集逐字段不变、`rdo=0` 与旧 7 参逐字节相同；全矩阵 RBSP −4.57%、最坏 PSNR −0.58 dB。详见下方「P3-3 收口记录」。 |
| ~~P2~~ **帧间主体已完成（2026-11-16）** | 阶段二 | 单参考→双参考前向 P 帧已落地：`VideoEncodeMode::Inter` 真实现（不再是空串）；`inter_coding.{hpp,cpp}` 做 Merge/AMVP 候选 + 整数/1-4 像素 MC（`interp.cpp` 8-tap/4-tap 对齐 FFmpeg）；`hevc_subpel_me` / `hevc_multi_ref` 两开关（默认 false 保基线）。多参考档 L0=[POC-1,POC-2]、RPS 两负参考 + `ref_idx_l0` 语法 + DPB 两帧滚动，`verify_hevc_subpel.py` 12/12 + `verify_hevc_multiref.py` 12/12 `neq=0`、ref=1 选中率 85.7%、cycle2 码率 −71.4%，均接 CI。仍未做：**双向 B 片**（需 GOP 帧重排 + POC 管理重构）、独立 `dpb`/`motion_estimate` 模块、`bitstream_control`。**刻意不新建 `rd_optimization.{hpp,cpp}`**：决策读 `tab_ipm` MPM 表、逐 TU 量化 level 与 `rec` 重建，全在 `emit_cu` 作用域，抽出只增回调复杂度零收益（见 P3-3 收口记录）。 |
| ~~P4~~ **功能已完成（2026-11-13 复核）** | MP4 路径 QP | `encode_mp4` → `encode_hevc_annexb(frames, opts)` → `std::clamp(opts.video_qp, 0, 51)` → `slice_qp_delta` 已全链路透传（旧文档「尚未透传」已更正）；`tests/unit/image/test_hevc_qp.cpp` 覆盖 slice_qp_delta，`test_mp4_encode.cpp` 新增「video_qp 经 MP4 路径透传」用例（QP10/40 码流不同且体积单调）。**注意**：`video_mode` 默认 `Pcm`（I_PCM 档刻意忽略 QP 以保基线字节不变），按 QP 产出必须显式设 `VideoEncodeMode::Intra`。 |

### 2.4 本轮修复记录（2026-11-09 ~ 11-10，均为真 bug）

1. **系数扫描随模式切换**（根因）：FFmpeg `hevcdec.c:1396-1403` 由亮度模式隐式选
   `SCAN_VERT`(6..14) / `SCAN_HORIZ`(22..30) / `SCAN_DIAG`；旧实现固定 DIAG，
   18 个模式的系数位置全错位，固定模式解码 PSNR 掉到 8~16 dB。新增 3 套扫描表、
   各自的 `sig_coeff` ctx 映射，以及 SCAN_VERT 的 `last_significant_coeff_x/y` 交换。
2. **MPM 左邻居跨 CTB 边界**：`hevcdec.c:2264-2267` 用 `ctb_left_flag || x0b`，
   `x0>0` 即左邻居可用；旧实现在 `x0%64==0` 强制回退 DC，第二个 CTB 起候选表错位。
3. **`rem_intra_luma_pred_mode` 求逆**：prev=0 分支解码端先把候选表**升序排序**再递增，
   旧实现按未排序表求逆，315 组合中错 7 个。
4. **DC 边界滤波**：FFmpeg `pred_template.c:451-457` 只有一条 `3*dc+2` 无条件公式，
   旧实现有"单侧不可用则 `(x+dc+1)>>1`"的错误分支；`predict_dc` 与
   `predict_intra_luma_4x4` 两条路径现已统一。
5. **I_PCM 黄金基线复原**：真压缩档把 SPS 的 MaxTb 改成 4px、`CabacEncoder` 改用
   `init_hevc()`，误伤了 PCM 档（4/6 GREEN、20427 B、色度全 128）。改为按档位分离
   `Sps::log2_diff_max_min_transform_block_size`（PCM=3 / Intra=0）并让
   `CabacEncoder::init_contexts` 回到 `init()`，G0 恢复 **6/6 GREEN / 20433 B**。
6. **模式决策接入 + 候选裁剪**：逐 CU 模拟「预测→量化→逆量化重建」选 SSD 最小；
   为避免 P0-5 破坏多 CTB 一致性，默认排除 mode 0/2..9，仍有 25 个可用模式，
   QP26 PSNR 38.40→39.14 dB。

7. **P0-5 根因修复（2026-11-10）**：`hevcdec.c:2753` 的 `end_of_tiles_y` 是**当前 CTB 行底部**
   （`FFMIN(y_ctb + ctb_size, height)`），`mvs.c:58` 据此在每个 CTB 行最后一行关闭
   bottom-left 扩展；编码端旧代码按整帧 `pic_h` 判定，读了下一 CTB 行尚未编码的 `rec`。
   改为按 CTB 行底判定后，35 模式在 128×64 / 256×128 / 640×384 全部逐像素一致，
   默认模式决策恢复全 35 模式（不再排除 mode 0/2..9）。此前文档把问题写成 top-right
   是误判，已一并更正。
8. **4K 模式决策性能（2026-11-10）**：`choose_intra_luma_mode_8x8` 原本每个 CU 都
   「分配整帧 + 整帧 memcpy」（4K 约 12 MB × 32640 CU ≈ 390 GB），是 564 秒/帧的主因。
   改为复用持久缓冲、每次只同步预测真正会读的邻域窗口（8x8 分裂档 16×16、单 TU 档 `2*cu`；早期写成固定 9×9 覆盖不到第 4 个 4x4 TU，会读到 trial 残留导致非确定性，已修），
   语义逐位不变；实测 **39.1 秒/帧**，码流 62 KB，d3d11va 与软解 Y 平面仍 maxdiff=0。
9. **色度预测与色度残差（2026-11-10）**：此前 `cbf_cb/cr` 恒 0、`rec.u/v` 恒 128，
   彩色内容全变灰度。按 FFmpeg `hevcdec.c:1484-1506` 在第 4 个亮度 TU（blk_idx==3）
   之后写 U/V 残差，`cabac.c` 的 `c_idx>0` 上下文偏移（last_sig +15、sig_coeff +27、
   greater1 +16、greater2 +4），色度用 DCT-II 与 `qPi→qPc` QP。预测侧踩到两处
   c_idx 专属语义：`pred_template.c:123-126` 的 `size_in_tbs` 色度为 2（亮度 1），
   以及 `:515/:542` 的 mode 10/26 后处理仅亮度生效；修正后彩色图 U/V 从 21 dB
   升到 38~50 dB，全 QP `recon U/V == dec U/V` maxdiff=0。

**新增回归**：`tests/unit/image/test_hevc_scan.cpp`（扫描表双射、模式→扫描映射边界、
3 套扫描的残差语法往返、模式决策确定性与越界回退、CTB 行底 bottom-left 关闭语义、
色度残差 ctx 绝对偏移与往返、色度跳过亮度专属 DC 后处理）。

---

## 3. 可执行拆分（按依赖顺序，每项 = 文件 + 实现 + 门禁 + 验收）

> 纪律：每项开工前跑 G0~G5 建立基线；完工后全跑一遍。任何一项让
> `diag_hevc.py` 不再是 6/6 或让 I_PCM 像素不再逐字节相等，立即回退该项。

### P0 — 帧内预测模式扩展（阶段一收尾）

> **P0-1 ~ P0-4 已于 2026-11-09 全部落地**（见 §2.4）：35 模式全覆盖、三套系数
> 扫描、MPM 与 rem 求逆修正、逐 CU SSD 模式决策。下表保留为原始验收口径，
> 当前唯一未完成项是新增的 P0-5。

| 步骤 | 文件 | 内容 | 验收 |
| --- | --- | --- | --- |
| P0-1 | `intra_prediction.{hpp,cpp}` | 补 Planar 已实现的接入；加角度 10(H)/26(V) 与参考样本平滑（8.4.4.2.2 的 1:1:1 滤波与 `strong_intra_smoothing`）；参考不可用时按规范替代 | 单测：固定 top/left 逐像素对拍 FFmpeg 解码结果 |
| P0-2 | `cabac_syntax.cpp` | MPM 推导按 8.4.4.2.3 实现（左/上 PU 模式、`candModeList`），`prev_intra_luma_pred_flag` / `mpm_idx` / `rem_intra_luma_pred_mode` 真正按选中模式写 | 单测：3 种 MPM 分支的 bin 序列 |
| P0-3 | `ctb_tree.cpp` / `hevc_encode.cpp` | 维护 `tab_ipm`（每 PU 的模式），CTB 边界按 FFmpeg `_hevcdec.c:2276` 把 `cand_up` 强制 DC | G5 全矩阵 + 8 形态逐位一致 |
| P0-4 | `intra_prediction.cpp` / `hevc_encode.cpp` | 逐 CU SSD 模式决策（模拟完整重建链），替换 `kDcMode` 硬编码 | **已达成**：QP26 PSNR 38.40→39.14 dB，QP 扫描体积/PSNR 仍单调 |
| P0-5 **已完成** | `intra_prediction.cpp` | 按 `end_of_tiles_y`（当前 CTB 行底部）判 bottom-left 可用性，放开 mode 0/2..9 | **已达成**：三尺寸 35/35 + 模式决策 maxdiff=0；G5 64/64 保持 |

**风险**：MPM 是"共同误解"高发区 —— 编码端与 ffmpeg 用同一份误解时自往返全过、
外部解码全错。P0-2 必须用 `trace_headers` 逐字段核对模式 bin，不能只跑自测。

### P1 — 8×8 TU 与 transform_skip

| 步骤 | 文件 | 内容 | 验收 |
| --- | --- | --- | --- |
| P1-1 | `transform.cpp` | 8×8 整数 DCT 蝶形（FFmpeg `dsp_template.c` 的 `TR_8`），逐常量对齐 | 往返单测 |
| P1-2 | `quantize.cpp` | 8×8 的 `shift = bd + log2 - 5` 与 `level_scale` 复用 | 逐常量对拍 `_ff_cabac.c:1173-1176` |
| P1-3 | `cabac_syntax.cpp` / `residual_coding.cpp` | 多 CG 的 `significant_coeff_group_flag`、`ctx_idx_map` 按 `prev_sig` 分支、`scan_2x2` CG 顺序 | 外部解码逐位一致 |
| P1-4 **已完成** | `parameter_sets.cpp` | `Pps::transform_skip_enabled_flag` + `Pps::log2_max_transform_skip_block_size`（写在 **PPS** range extension）；`Sps/Vps::general_profile_idc=4`(RExt) 与 `Sps::range_extension` 配套 | 360 条码流软解 `recon == dec neq=0`；`trace_headers` 逐字段核对 profile / extension / minus2；单测锁住 max=2 不写 extension、3/4/5 写出不同 `ue(v)`、Main 位宽不变 |

**风险**：多 CG 的 `ctx_idx_map` 与 `prev_sig` 是失步高发区；先做"仅 DC、多 CG"
的最小用例跑通，再放开任意系数。

### P2 — 阶段二：P 帧与参考帧管理（约 8 文件）

依赖顺序严格按 `dpb → motion_estimate → inter_prediction → slice/cabac/ctb 扩展`：

| 步骤 | 文件 | 内容 | 验收 |
| --- | --- | --- | --- |
| P2-1 | `dpb.{hpp,cpp}` | DPB、参考列表 L0/L1、POC 管理、短期参考集 | 单测：I+P+P 的 POC 单调、参考索引正确 |
| P2-2 | `motion_estimate.{hpp,cpp}` | 整像素全搜索 + 小菱形细化，SATD 代价 | 单测：已知位移序列能找回 MV |
| P2-3 | `inter_prediction.{hpp,cpp}` | Merge / AMVP、1⁄4 像素亮度插值（8-tap）、色度 4-tap、单向/双向加权 | 与 FFmpeg MC 输出逐像素对拍 |
| P2-4 | `slice_header.cpp` | `slice_type=P`、`num_ref_idx`、POC LSB、`ref_pic_list_modification`、`short_term_ref_pic_set` | trace_headers 逐字段核对 |
| P2-5 | `cabac_syntax.cpp` / `ctb_tree.cpp` | `merge_flag/idx`、`mvp_flag`、`mvd`、`ref_idx`、`cu_skip_flag`、预测单元划分 | I+P+P FFmpeg 0 error |
| P2-6 | `parameter_sets.cpp` | `sps_temporal_mvp_enabled_flag`、VPS/SPS 的 DPB 层数与 `max_dec_pic_buffering` | G5 扩展为 3 帧硬解矩阵 |
| P2-7 | `hevc_encode.cpp` | 档位 `Inter` 真正接入（当前显式返回空串，fail-loud） | 清单四：3 帧 0 error；P 帧显著小于 I 帧 |

**硬性要求**：P 帧一旦引入，G5 必须从单帧扩成 **I+P+P 三帧序列 × 多 QP × 多尺寸**
的硬解矩阵；DPB 相关参数集字段是硬解拒绝的高频原因，必须 `trace_headers` 核对。

### P3 — 阶段三：质量与性能（约 4 文件）

| 步骤 | 文件 | 内容 | 验收 |
| --- | --- | --- | --- |
| ~~P3-1~~ 已完成 | `deblocking.{hpp,cpp}` | bS 判定（全帧内恒 2）、β/tc 表、亮度强/弱滤波、**色度弱滤波**、固定档 TU 边界周期、自动档逐 CU 边界图；编码端 `rec` 同步滤波 | `--dblock-on` 60 组开 DBF 逐像素 `neq=0`；15 组 DBF 开关 Annex-B 字节恒等；单测 10 用例 |
| ~~P3-2~~ 已完成（2026-11-14） | `sao.{hpp,cpp}` | BO（32 band 起点）/ EO（4 方向 × 5 类）/ merge 左·上，RD = 真实滤波 SSE + λ·bins；`put/get_sao_params` 对偶、`exact_sse` 局部原点试算、DBF→SAO 顺序 | `--sao` 72 组全量 + quick 8 组：`neq=0`、`trace_headers` 核对 SPS/slice 开关、sao=0 与旧 7 参逐字节相同、PSNR 不劣化且矩阵内确有增益用例；单测 9 用例 / 847 断言 |
| ~~P3-3~~ 已完成（2026-11-14） | `intra_prediction.{hpp,cpp}` + `hevc_encode.cpp`（不新建 `rd_optimization.*`，理由见 §2.3） | 帧内模式 RD（SSD + λ·[模式语法 bin + 残差 bin]，实算 bin 数）、CBF RD（cbf=0 vs cbf=1）、统一 `hevc_rdo` 开关；CU 切分沿用自动档既有的 `SSD + λ·bins` | `--rdo` 全量 96 组 + quick 8 组：`neq=0`、`trace_headers` 参数集逐字段不变、rdo=0 与旧 7 参逐字节相同、总 RBSP 下降且矩阵内有「码率降+画质不降」用例；单测 6 用例 / 246 断言。**验收口径已修正**：原写「体积 −≥10%」在固定 QP 下必然伴随画质退化（实测 −10.85% 对应最坏 −9.52 dB），改为「同画质约束下码率下降」 |
| P3-4 | `bitstream_control.*`（可选） | 固定 QP → 简单 CBR/VBR | 帧间码率波动收敛；单帧 IDR 序列下无意义，留待 P2 P 帧之后 |

**注意（2026-11-13 更正）**：真压缩档传默认 `Pps{}`，
`deblocking_filter_control_present_flag=0` ⇒ 解码端
`pps_deblocking_filter_disabled_flag` 推断为 0，**DBF 生效**（旧文档写「靠禁滤波保证判据」已过时）。
编码端重建出口有 `deblock_frame_luma` 近似实现（bS 恒 2、仅亮度、单 tc），与 FFmpeg 差
`maxdiff≈4`；逐像素门禁 `tools/verify_hevc_tu_sizes.py` 用 `TERMCHART_HEVC_NO_DBLOCK=1`
+ `ffmpeg -skip_loop_filter all` 让两端都关 DBF，故 `recon == dec neq=0` 成立。
**P3-1 已于 2026-11-13 完成**：编码端 DBF 精确化后，**开启 DBF 时**同样逐位一致
（`--dblock-on` 60 组 Y/U/V `max=0 neq=0`）；G5 的 Y 平面逐位比对保留。
两端都关 DBF 的旧门禁路径也保留，用于隔离「滤波」与「码流语法」两类变量。

#### P3-3 收口记录（2026-11-14）

**落地面**：`EncodeOptions::hevc_rdo`（默认 false）、`intra_prediction.hpp/cpp` 的
`mode_signal_bins` / `rdo_distortion_slack` / `choose_intra_luma_mode` 的 RD 参数、
`hevc_encode.cpp` 的模式 RD + CBF RD、`tools/hevc_tu_probe.cpp` 第 9 参、
`tools/verify_hevc_tu_sizes.py --rdo`、`tests/unit/image/test_hevc_rdo.cpp`、
CI `encoder-cert` 的 quick 门禁、`single_include` 重生成。

**代价口径**：`J = SSD + λ·bins`，bin 数全部由 `encode_residual` / `encode_residual_4x4`
实算，模式语法 bin 由 `mode_signal_bins` 按 `put_prev_intra_luma_pred_flag` /
`put_mpm_idx` / `put_rem_intra_luma_pred_mode` 的真实长度给出（单测把两者实拼对拍
35 模式 × 5 组 MPM 候选）。λ 分两条：模式 RD 用 `mode_rdo_lambda = auto_lambda·0.15`
（模式 bin 叠在残差 bin 之上，且 bin≈bits 会系统性高估码率），CBF RD 用未折减的
`auto_lambda`（与 TU 层 / transform_skip 同口径）。二者共用一个失真余量
`rdo_distortion_slack()`。

**为什么是「约束式」而不是裸 RD**：裸 `SSD + λ·bins` 会拿画质硬换码率。
实测 6 形态 × 4 QP × 4 档全矩阵，λ 不折减时总码率 −10.85%，但 `one8/QP40`
掉 **9.52 dB**；逐点扫描 λ 系数与余量后确认，固定 QP 下「−≥10% 且画质不降」
是自相矛盾的验收。故改为「先定纯 SSD 最优，只在 `SSD ≤ 最优×(1+slack)` 的候选里
挑 bins 最省的」，默认 slack=0.05；门禁全矩阵实测 **RBSP −4.57%（118126→112728 B）、
最坏 −0.58 dB、平均 +0.00 dB、55 组「码率降且画质不降」**、`neq=0`。
（实验脚本按原始 Annex-B 字节口径为 −4.47%；两者差在防竞争字节。）
路线图原验收口径据此更正，不为凑数字牺牲画质。

**修掉的真 bug**：CBF 清零最初放在 `emit_cu` 末尾统一处理，`slack=0.25` 时
`grad/dots QP10 TU2` 出现 `neqY=9/18`。根因是 4×4 亮度 CU 内后 3 个 TU 的帧内
预测会读前序 TU 的重建，统一回滚会让块内参考分叉。改为**逐 TU 就地决策**
（写完本 TU、下一个 TU 预测之前清零并把 `rec` 回滚成 pred），大 TU/色度单块
路径仍可在末尾处理；复测 `slack=0.25` 全矩阵 `neq=0`。

**明确不做（附理由）**：
1. **SATD 预筛**：本编码器 35 模式循环本来就要跑量化重建拿精确 SSD，SATD 只
   能减少候选数、反而可能漏掉最优模式，零收益。
2. **真实 λ·bits / CABAC 状态模拟**：决策时上下文状态尚未确定，且 HM 系实现
   也普遍用 bin 数/估计 bits；沿用库内 `bins.size()` 口径避免两套 λ 混用。
3. **独立 `rd_optimization.{hpp,cpp}`**：决策依赖 `tab_ipm`、`rec`、逐 TU level
   与 `scan_idx`，全在 `emit_cu` 作用域内；抽出只会引入上下文参数包与回调。
4. **P3-4 码率控制**：单帧 IDR 序列没有帧间码率波动可收敛，等 P2 P 帧落地再说。
5. **非 64 倍数尺寸**：`encode_hevc_intra_annexb` 已有 CTB padding + conformance
   window，但探针硬约束 64 倍数、RDO 与 SAO 在裁剪边界的行为未做外部解码对拍，
   另立缺口，不并入本轮。

**验证**：`--rdo` 全量 96 组 + quick 8 组 `PASS problems=0`（三条流都用 ffmpeg
真解码对拍 Y/U/V）；`trace_headers` 逐字段证明参数集不变；单测 6 用例 / 246
断言；λ/slack 双参数扫描 12 组全部 `neq=0`。

#### P3-2 收口记录（2026-11-14）

**落地面**：`sao.{hpp,cpp}`（语法 bin 化 / 对偶回读 / BO·EO 滤波 / 整帧 RD 决策）、
`parameter_sets` 的 SPS 开关、`slice_header` 的两个 `slice_sao_*_flag`、
`hevc_encode.cpp` 的 CTB bin 交织与 DBF→SAO 应用、`EncodeOptions::hevc_sao`、
`tools/hevc_tu_probe.cpp` 第 8 参、`tools/verify_hevc_tu_sizes.py --sao` 门禁、
`tests/unit/image/test_hevc_sao.cpp`、CI `encoder-cert` 的 quick 门禁。
`hevc_sao` 默认 `false`，所以既有码流字节级基线不动。

**关键设计**：7.3.9.2 的 `sao()` 在每个 CTB 的 `coding_quadtree` **之前**，但 SAO
参数要等整帧重建 + DBF 完成才能决策 ⇒ 把每 CTB 的编码 bin（含 terminate）缓冲，
决策完成后按「sao bins → 该 CTB bins → terminate」交织写回。`exact_sse` 用 CTB
大小的线程局部缓冲 + `dst_x0/dst_y0` 局部原点，避免整帧拷贝且跨 CTB 邻居仍从
SAO 前的重建读取。

**本轮抓到并修掉的 4 个真 bug**（都由 `recon == dec` 逐像素门禁暴露，而不是靠
「能解码」这种弱判据）：
1. RD 候选在 `store_cand` 后没有展开 `offset_val`，`exact_sse` 恒等于「不滤波」，
   SAO 静默空转（只多写语法 bin）。
2. BO 的近似代价只累计被 4 个 band 覆盖的失真，空 band 起点白拿 `sse=0 + 最少 bit`，
   永远压过真正有增益的起点。改为「整帧 SSD − 这 4 个 band 的改善量」后，
   `dots` 档 Y-PSNR 增益从 +0.4~0.7 dB 提升到 **+1.7~+2.3 dB**。
3. EO 的 `eo_class` 2/3 两个对角方向对调；符号约定误判为 `[+,-,0,-,-]`。
   以 ffmpeg 为真值订正为 **2 = 135°（左上↔右下）、3 = 45°，符号 `[+,+,0,-,-]`**。
4. EDGE 滤波在局部 dst 上误按绝对坐标 `drow[x]` 写（应为 `drow[x-dst_x0]`），
   第二个 CTB 起踩堆（Windows `0xC0000374`）。

**验证**：`--sao` 全量 72 + 多 CTB 行 54 组、quick 8 + 2 组 `PASS problems=0`
（每条 on/off 都用 ffmpeg 真解码对拍 Y/U/V）；`trace_headers` 核对 SPS 开关 0/1
与 slice 两个 flag；单测 9 用例 / 847 断言；CI 六档 quick 门禁全绿；全量构建、
`ctest` 92/92、doctest 1467 用例 / 262860 断言、`check_all`（453 头）全绿。

### P4 — 工程收口

| 步骤 | 内容 | 验收 |
| --- | --- | --- |
| ~~P4-1~~ 已完成 | `EncodeOptions::video_qp` 透传到 `encode_mp4` / `api::save_image` | 全链路已通；`test_mp4_encode.cpp` 已补 QP10/40 透传单测（需 `video_mode=Intra`） |
| P4-2 | 新增文件按四件套：CMake 引用、`src/` 同名 cpp、单测、`check_header_only` 登记 | G1~G4（长期纪律，非一次性待办） |
| ~~P4-3~~ 已完成 | `docs/reference/hevc_encoder_handoff.md` 现状更新 | §0 已写「逐位一致 + 硬解通过」，2026-11-13 补 §0a 后续扩展现状指针 |
| ~~P4-4~~ 已完成 | `ROADMAP.md` 顶部「修订记录」按阶段附实测数字 | 2026-11-09~11-13 多行修订记录 |

---

## 4. 22 层 DAG 归属（清单六，按当前 `TermchartModules.cmake` 校准）

| 模块 | 归属 | 理由 |
| --- | --- | --- |
| `bit_writer` / `ep_nal` | `image/codec/bitstream`（私有实现单元） | 无 HEVC 语义；`check_header_only.py` 的 `SRC_INTERNAL_CXX`/同名私有头自动豁免 |
| `cabac_engine` / `cabac_context` / `cabac_arith` | `image/codec/hevc` | HEVC 专属表与状态机 |
| `transform` / `quantize` | `image/codec/hevc` | 纯整数运算，但 QP/变换尺寸语义属 HEVC，不宜下沉到 L0 math |
| `ctb_tree` / `intra_prediction` / `cu_coding` | `image/codec/hevc` | 编码树与预测语义 |
| `inter_prediction` / `motion_estimate` / `dpb` | `image/codec/hevc` | 参考帧与运动补偿 |
| `deblocking` / `sao` / `rd_optimization` | `image/codec/hevc` | 环路滤波与决策 |
| `mp4_muxer` | `image/codec/container` | 容器封装 |

**与清单的差异**：`transform`/`quantize` 建议**不**下沉到 L0 math 或 L5 image ——
它们的 `shift = bit_depth + log2_trafo_size - 5`、`level_scale[qp%6]` 都是 HEVC 规范量，
下沉会让 L0 依赖 codec 语义。当前放在 `image/codec/hevc` 更合适，且已通过 G1。

**依赖方向（禁止反向）**：
```
cabac_syntax → cabac_engine/arith → cabac_context
ctb_tree → cu_coding → intra/inter → transquant → transform/quantize
slice_header → parameter_sets → nal_unit → ep_nal → bit_writer
dpb → inter_prediction → motion_estimate
deblocking/sao → 重建帧（不改变 CABAC 语法层）
```

---

## 5. 执行顺序与回退纪律

```
[已完成] P0-1~P0-4：35 模式 + 三套扫描 + MPM 修正 + SSD 模式决策
[已完成] P0-5：CTB 行底 bottom-left 可用性对齐，放开 mode 0/2..9，三尺寸 35/35 全绿
[已完成] 模式决策性能：4K 单帧 564 s → 41.8 s（13.5×，含色度残差），语义不变、硬解仍 maxdiff=0
[已完成] 色度预测与色度残差：DM + DCT-II + c_idx CABAC，U/V 逐像素一致
   ↓
~~P1（8×8 / 16×16 / 32×32 TU + 自动档 + 大 TU transform_skip）~~ 已完成 2026-11-12
   ↓
~~P2（帧间主体：单参考→双参考前向 P 帧 + 1/4 像素插值）~~ 主体已完成 2026-11-16
   （`verify_hevc_inter.py` / `verify_hevc_subpel.py` / `verify_hevc_multiref.py` 三门禁全绿；
    剩余双向 B 片需 GOP 帧重排，独立切片）
   ↓
~~P3-1（DBF 色度通路 + TU 边界图）~~ 已完成 2026-11-13 → ~~P3-2（SAO BO/EO/merge + 外部 oracle）~~ 已完成 2026-11-14 → ~~P3-3（模式 + CBF 约束式 RDO）~~ 已完成 2026-11-14 → P3-4（码率控制，单帧 IDR 下无意义，等 B 片帧间波动）
   ↓
~~P4（MP4 QP 透传 / 文档 / ROADMAP 勾选）~~ 已完成 2026-11-13（含 QP 透传单测）
```

> 纪律补充：P 帧（阶段二）主体**已完成**——`VideoEncodeMode::Inter` 是真实现，
> 单参考整数 MC → 1/4 像素插值 → 双参考 P 帧分步落地，每步都由
> `verify_hevc_inter.py` / `verify_hevc_subpel.py` / `verify_hevc_multiref.py` 的
> `recon == ffmpeg 解码` 逐字节门禁护航，默认开关关闭时字节级基线不变。
> B 片 GOP=3 重排已于 2026-11-17 落地，merge-only 路径经 `verify_hevc_bslice.py`
> 全形态 recon==dec；混合 B AMVP 已冻结为 Backlog（跨 POC 候选推导未收敛），
> 默认 `hevc_b_slice` 不启用该路径。
>
> **AV1 已由用户于 2026-11-17 亲自开启**：从最小纵切开始；在 AV1 最小
> 可解码闭环通过前，`VideoCodec::AV1` 仍保持 fail-loud 空实现契约，
> 不提前切换分发。

每一步的开工/完工检查固定为：

```powershell
python diag_hevc.py                              # G0，必须 6/6
python tools/check_layers.py                     # G1
python tools/check_header_only.py                # G2
cmake --build --preset ninja --target check_all  # G3
ctest --preset ninja                             # G4，写具体数字
python tools/verify_hevc_hwaccel.py              # G5，hardware=ACTIVE
```

**回退触发条件**：G0 不再是 6/6 / I_PCM 字节或像素不再逐字节相等 /
G5 出现硬解回退或 Y 平面 maxdiff>0 / 生成物需手改才能过门禁。
任一触发，回退该步骤而不是继续叠加修复 —— CABAC 已经翻过最险的山，
后面是数学翻译与工程拆分，不允许带着坏基线前进。

---

## 6. 提交规范（CONTRIBUTING）

- 一行主题 + 空行 + 必要说明；主题用祈使句。
- **生成物独立提交**：`single_include/termchart.hpp`、`结构.md` 附一/附四、
  `docs/reference/module_matrix.md` 由生成器产出，须与源码改动分开提交，
  并在提交信息里注明生成器与命令（如 `tools/gen_single_header.py`）。
- 牵涉进度计划的改动：`ROADMAP.md` 顶部「修订记录」加一行。
- PR 前逐项确认 `ctest` 具体数字、`check_all`、全量重编 0 warning、
  新增头四件套、`check_layers` 无新豁免、golden 由 `gen_golden.py` 重生成。
- 工作树常有多 Agent 未提交改动：**精准暂存**，不要 `git checkout .`。
