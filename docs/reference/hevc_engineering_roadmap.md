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
| G4 全量测试 | `ctest --preset ninja` | 全绿，报告写具体数字 | 91/91（doctest 1418 用例 / 238946 断言） |
| G5 **工业级硬解** | `python tools/verify_hevc_hwaccel.py` | 64/64（4 尺寸 × 4 QP × 4 形态）硬解真实启用，Y 平面与软解 **maxdiff=0**；`slice_qp_delta=qp-26`；`pcm_enabled_flag=0` | 64/64 PASS，hardware=ACTIVE |
| G6 体积 | QP 扫描 | 64×64 比 I_PCM 降 ≥80%；QP 22/26/30/34 体积与 PSNR 单调 | ≥80% 达成；1111→748→564→421 B / 43.54→39.14→36.47→34.32 dB |

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

| 判据（ROADMAP:484 / 清单三） | 2026-11-09 实测 |
| --- | --- |
| FFmpeg 0 error | rc=0 |
| PSNR > 35 dB（固定 QP=26） | **39.14 dB**（grad / 128×64 / 默认开 DBF + 模式决策）；固定 DC 时代 38.40 dB，关 DBF 时代 40.28 dB 是另一口径，不可混用 |
| 64×64 体积比 I_PCM 降 ≥80% | 6353 B → 187 B，**降 97.06%**（固定 DC 基线）；模式决策后体积略增但降幅仍 >90% |
| QP 22/26/30/34 体积单调 | 1111 → 748 → 564 → 421 B |
| QP 22/26/30/34 PSNR 单调 | 43.54 → 39.14 → 36.47 → 34.32 dB |
| 关 PCM 后码流无 `pcm_flag` | `trace_headers`：`pcm_enabled_flag=0` |
| 编码端重建 == ffmpeg 解码 | **35/35 模式** 与 **256×128 多 CTB 模式决策** 均 `maxdiff=0`（`NO_DBLOCK=1` + `-skip_loop_filter all` 的干净路径） |
| **工业级硬解** | d3d11va 真实启用，64/64 组 Y 平面与软解逐位一致，worst Y-PSNR 32.33 dB |

**根因修复记录**（避免重复踩）：编码端重建与解码端发散的真因是
`predict_dc` 缺 HEVC 8.4.4.2.2 参考样本替代与 8.4.4.2.5 首行/首列边界滤波；
常数图下两种写法恒等（`flat128` 逐位一致掩盖了偏差）。已由
`tests/unit/image/test_intra_prediction.cpp`（81 断言）锁死，oracle 取自
ffmpeg 解码像素反推。**自往返测试不能替代外部 oracle**。

### 2.3 真实缺口（2026-11-09 代码 + 实测）

**帧内预测本身已做完**：`predict_intra_luma_4x4` 覆盖 0..34 全部 35 个模式，
`choose_intra_luma_mode_8x8` 按真实 TU 次序逐 CU 做 SSD 模式决策，三套系数扫描
随模式切换。以下仍是缺口，按优先级登记：

| 编号 | 缺口 | 证据 / 现状 |
| --- | --- | --- |
| P0-5 | 跨 CTB 行的 **top-right 扩展参考** | 256×128 下固定模式实测 `mode 0 / 2..9` 的 `recon != ffmpeg dec`（FFmpeg 侧读 CTB 边缘/emulated edge 填充，编码端 `rec` 无同语义）。默认模式决策已排除这 10 个模式，保证任意尺寸逐像素一致；`TERMCHART_HEVC_FIXED_MODE` 逐模式 oracle 仍可复现。修好后应放开候选集并重跑 35 模式 × 多尺寸门禁。 |
| P1 | 8×8 / 16×16 / 32×32 TU、`transform_skip` | SPS `log2_diff_max_min_transform_block_size = 0`（MaxTb=4px）；`transform.cpp` 只有 4×4 DCT-II / DST-VII。 |
| P1 | 色度预测与色度残差 | `cbf_cb/cr` 恒 0、`rec.u/v` 恒 128、语法写 `intra_chroma_pred_mode=4`(DM)。彩色内容会丢色度。 |
| P3 | DBF 精确化 | `deblocking.cpp` 是 bS 恒 2、仅亮度、单 tc 的近似，与 FFmpeg 差 maxdiff≈4（只影响编码端 recon，不进码流）；SAO 在 SPS 中关闭。 |
| P2 | 阶段二 | `inter_prediction` / `motion_estimate` / `dpb` / `sao` / `rd_optimization` / `bitstream_control` **全仓库 0 实现文件**；`VideoEncodeMode::Inter` 显式返回空串（fail-loud 契约，不是半成品）。"P 帧功能清理"的结论是：没有可清理的代码，需要的是把该占位语义写进公开头并锁定（已完成）。 |
| P4 | MP4 路径 QP | `EncodeOptions::video_qp` 已可配置，但 `encode_mp4` 尚未透传。 |

### 2.4 本轮修复记录（2026-11-09，均为真 bug）

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

**新增回归**：`tests/unit/image/test_hevc_scan.cpp`（扫描表双射、模式→扫描映射边界、
3 套扫描的残差语法往返、模式决策确定性与越界回退）。

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
| P0-5 | `intra_prediction.cpp` / `residual_coding.cpp` | 对齐跨 CTB 行 top-right 扩展参考（FFmpeg 帧缓冲 / emulated edge 语义），修好后放开默认决策里被排除的 mode 0/2..9 | 256×128 + 640×384 下 35/35 固定模式与模式决策均 `recon == dec` maxdiff=0；G5 64/64 保持 |

**风险**：MPM 是"共同误解"高发区 —— 编码端与 ffmpeg 用同一份误解时自往返全过、
外部解码全错。P0-2 必须用 `trace_headers` 逐字段核对模式 bin，不能只跑自测。

### P1 — 8×8 TU 与 transform_skip

| 步骤 | 文件 | 内容 | 验收 |
| --- | --- | --- | --- |
| P1-1 | `transform.cpp` | 8×8 整数 DCT 蝶形（FFmpeg `dsp_template.c` 的 `TR_8`），逐常量对齐 | 往返单测 |
| P1-2 | `quantize.cpp` | 8×8 的 `shift = bd + log2 - 5` 与 `level_scale` 复用 | 逐常量对拍 `_ff_cabac.c:1173-1176` |
| P1-3 | `cabac_syntax.cpp` / `residual_coding.cpp` | 多 CG 的 `significant_coeff_group_flag`、`ctx_idx_map` 按 `prev_sig` 分支、`scan_2x2` CG 顺序 | 外部解码逐位一致 |
| P1-4 | `parameter_sets.cpp` | `transform_skip_enabled_flag` 与 `log2_max_transform_skip_block_size` 开关 | G5 + trace_headers |

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
| P3-1 | `deblocking.{hpp,cpp}` | bS 计算、β/tc 阈值表、亮度/色度滤波；**编码端 `rec` 必须同步滤波**，否则重建漂移 | 清单五：PSNR +≥0.5 dB 或同 PSNR 体积 −≥5% |
| P3-2 | `sao.{hpp,cpp}` | EO/BO，先全 off 保证码流合法，再逐模式打开 | 同上 |
| P3-3 | `rd_optimization.{hpp,cpp}` | SATD/SSD + λ·bits 决定 CU 切分 / 预测模式 / CBF | 关闭 RD 仍合法；打开后体积 −≥10% |
| P3-4 | `bitstream_control.*`（可选） | 固定 QP → 简单 CBR/VBR | 帧间码率波动收敛 |

**注意**：当前真压缩档靠 PPS `pps_deblocking_filter_disabled_flag=1` 让解码端
不做滤波，从而保证逐像素判据。打开 DBF/SAO 后，`enc vs dec` 判据要改成
「编码端同步滤波后的重建 == 解码输出」，G5 的 Y 平面逐位比对也必须保留。

### P4 — 工程收口

| 步骤 | 内容 | 验收 |
| --- | --- | --- |
| P4-1 | `EncodeOptions::video_qp` 透传到 `encode_mp4` / `api::save_image` | MP4 路径可按 QP 产出，单测覆盖 |
| P4-2 | 新增文件按四件套：CMake 引用、`src/` 同名 cpp、单测、`check_header_only` 登记 | G1~G4 |
| P4-3 | `docs/reference/hevc_encoder_handoff.md` §0 现状更新为「逐位一致 + 硬解通过」 | 文档与代码一致 |
| P4-4 | `ROADMAP.md` 顶部「修订记录」按阶段勾选，附实测数字 | CONTRIBUTING 提交规范 |

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
   ↓
P0-5（跨 CTB top-right 扩展参考）→ 放开 mode 0/2..9，35 模式 × 多尺寸全绿
   ↓
P1（色度预测/残差 → 8×8 TU → transform_skip）
   ↓
P2（DPB → ME → inter_prediction → I+P+P；G5 扩成 3 帧序列硬解）
   ↓
P3（DBF 精确 bS + 色度通路 → SAO → RDO / 码率控制）
   ↓
P4（MP4 QP 透传 / 文档 / ROADMAP 勾选）
```

> 纪律补充：P 帧（阶段二）**未开工且无半成品**。`VideoEncodeMode::Inter`
> 是显式 fail-loud 占位，不要在 P0-5 / P1 未收口前把帧间分支塞进编码树；
> 每加一个模式或尺寸，先跑 `verify_modes_recon.py` 的 `recon == dec` 逐像素门禁，
> 再跑 G5 硬解矩阵。
>
> **当前封存于阶段一（2026-11-09）**：阶段二/三与 AV1 的启动顺序由用户决定，
> **AV1 由用户亲自开启**。本路线保留为 HEVC 后续参考，不阻塞 AV1；在用户下指令前
> 不新增任何 AV1 实现文件，`VideoCodec::AV1` 保持 fail-loud 空实现契约。

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
