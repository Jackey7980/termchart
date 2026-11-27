# AV1 全量落地八阶段蓝图（2026-11-21 现状）

> 这张地图定义从「极简可解码闭环」扩展到「全量 AV1 编码器」的路线。状态以源码与实测为准；早期口头描述中过时的字节/结论已在本文校正。

## 阶段一：OBU 容器与全量序列头 —— 已通关

- 规划：抛弃 NAL Unit，实现完整 OBU 头解析、Sequence Header（色彩空间/帧率等）、Frame Header 与 IVF 封装。
- 现状：已打通。OBU 头、LEB128、IVF 头/帧、sequence header、frame header（`cdef_params` / `lr_params` / trailing bits 已补齐）均已实现。
- 实测：`python tools/verify_av1_minimal.py` 经 ffmpeg/libdav1d 逐像素 **PASS**，解出 96 字节纯灰帧（Y/U/V 全 128）。
- 备注：早期 `40 01 C0` 是硬编码 tile 的历史形态；现在 tile group 由区间编码器按符号动态生成（当前平坦帧为 `22 03 40 01 b7`，字节随语法变化）。

## 阶段二：全量 Daala 熵编码与 CDF 状态机 —— 已完成

- 规划：多符号区间分割/重归一化、完整 CDF 更新、加载默认 CDF 表。
- 已完成：
  - `RangeEncoder` / `RangeDecoder` 按 libaom `entenc.c` / `entdec.c` 规范实现 `range_split` 与重归一化；
  - `cdf_manager` 提供 `write_symbol` / `read_symbol` / `update_cdf`，本阶段补齐 `write_symbol_adaptive` / `read_symbol_adaptive` 状态机封装；
  - `update_cdf` 的 rate、单调性、计数饱和（32）已用独立参考公式逐项测试；
  - `default_cdf.hpp` 的模式/划分/skip CDF 与 `coef_cdf_data.hpp` 的残差 CDF 已接入；`base_q_idx=100 → qctx=2` 的 `txb_skip / eob / eob_extra / coeff_base / coeff_br / dc_sign` 与 libaom 逐项一致；
  - 14 个 AV1 单测通过，其中自适应状态机做 512 符号编解码往返且两端 CDF 完全一致；官方一致性向量 20/20 PASS。
- 边界：69 种帧内预测器本身属于阶段四；阶段二负责的是这些模式在 CDF 状态机中的符号树与默认表，不是预测算法。

### 阶段 2.1（已闭合）
把硬编码 tile 替换为真正的区间编码符号流已完成；平坦门禁持续 PASS，不再是待办。

## 阶段三：全量 128x128 超级块与十叉树划分 —— 已完成

- 规划：10 种划分类型（`NONE/HORZ/VERT/SPLIT/T_TOP_SPLIT/T_BOTTOM_SPLIT/T_LEFT_SPLIT/T_RIGHT_SPLIT/H4/V4`）与跨边界邻居上下文。
- 已完成：
  - 新增 `src/image/codec/av1/bitstream/partition_tree.{hpp,cpp}`：128x128 超级块（16x16 mi）十叉树，支持全部 10 种划分及矩形子块几何；
  - `PartitionContext` 按 libaom `partition_context_lookup` 维护上/左 16mi 邻居上下文，`index = (left*2+above) + bsl*4`；
  - 边界块按规范用二符号合并 CDF（`horz_alike`/`vert_alike`），无行无列时强制 `SPLIT`；
  - 编解码端共用同一棵树的构造规则，新增 2 个单测：多级混合划分往返（10 种类型全覆盖）+ 边界合并 CDF 往返，邻居上下文逐字节一致。
- 边界：8x8 单帧仍由帧尺寸推断划分、不写符号；新模块为任意尺寸帧提供完整划分树能力。

## 阶段四：全量帧内预测（69 种模式）—— 已完成

- 规划：8 基础方向 + 45 细角度 + 3 Smooth + Paeth + CFL + filter_intra + intra_edge。
- 已完成：
  - `src/image/codec/av1/predict/intra_predict.{hpp,cpp}` 覆盖 61 个亮度模式（8 方向基模式 × 7 档 angle_delta + DC/3 Smooth/Paeth），含上/左边界填充、滤波强度、角点滤波与细角度半像素上采样；
  - `predict_filter_intra` 落地规范 7.11.2.3 的 5 组 8 抽头滤波器，块语法按 `skip → y → angle_delta → uv_mode → cfl_alpha → filter_intra` 编码；
  - `predict_cfl_chroma` 落地 4:2:0 CFL（4x4 亮度子块 q3 平均、去均值、alpha 联合符号/幅值语法），色度基础预测沿用 DC；
  - 新增 `tools/av1_intra_probe.cpp` 与门禁扩展：`python tools/verify_av1_minimal.py` 现同时跑 61 组方向/非方向模式、5 档 filter intra 与 32 组 CFL 组合，全部经 ffmpeg/libdav1d 逐像素对拍 PASS。
- 历史问题（已修复）：早期 `Y[0]=177` 源于 filter_intra 语法开关与预测器不对齐；CFL 早期偏差来自 4x4 子块取样步长与 `ROUND_POWER_OF_TWO_SIGNED(...,6)` 缩放档位，均已按规范校正。

## 阶段五：全量帧间预测 —— 主链与复合预测均已逐像素通关

- 规划：7 参考槽、8 插值滤波器、Warped Motion、OBMC、Wedge/Masked 复合预测。
- 已完成：
  - `predict/inter_predict.{hpp,cpp}` 落地三档 8 抽头插值 + bilinear、整/亚像素块拷贝、4:2:0 色度半 MV 预测、复合平均/距离加权、OBMC 叠加与仿射 warp 双线性取样；
  - 亚像素取整改为 **16 倍中间精度**（水平 `(Σ+4)>>3`、纵向 `(Σ+1024)>>11`），与 libaom `av1_convolve_2d_sr_c` / dav1d `put_8tap_c` 完全同构；此前按 `(Σ+64)>>7` 两级同精度处理，双向亚像素时会与解码器差 ±1；
  - 色度相位改为规范形式：整数偏移取亮度 MV `>>4`、1/16 相位取低 4 位（原来简单 `mv/2` 会丢掉相位最低位），且色度沿用亮度同一个 `interp_filter`（不再是 bilinear）；
  - `headers.{hpp,cpp}` 增加非 `reduced_still_picture_header` 的序列头与 KEY/INTER 帧头（错误 resilient、7 参考槽、IDENTITY 全局运动、亚像素 MV 开关），并新增帧级 `interp_filter` 可配项（规律/平滑/锐利/双线性）；
  - `tile_group.{hpp,cpp}` 增加 INTER 块语法（skip/intra/ref/newmv/bitstream）与参考帧重建；`tools/av1_inter_probe.cpp` 生成 KEY+INTER 两帧 IVF，支持 NEWMV 探针与帧级滤波器选择；
  - `tools/verify_av1_minimal.py` 增加两帧门禁与阶段五亚像素门禁：NEWMV 扫全 1/8 相位 × 四种帧级滤波器共 69 组，ffmpeg/libdav1d 解出的 192 字节（亮度 + 4:2:0 色度）与编码器预期逐像素一致。
  - 阶段五后半段已闭合：NEWMV + 非零残差、帧头 TRANSLATION 全局运动（`global_mv_x/y` 真正写进 GMV 矩阵）、双参考复合 AVG / WEDGE / DIFFWTD（WEDGE 扫 16 个 idx × 2 sign，DIFFWTD 扫 8 组 MV × mask_type）与单参考 inter-intra（4 种帧内模式 × BLEND/WEDGE × 16 idx 全量扫表）均由 ffmpeg/libdav1d 逐像素对拍通过；
  - 复合混合在 4 位小数中间精度上完成（`avg_c`/`mask_c` 同构），楔形掩码用 8x8 的 16×2 权重表、4:2:0 色度按 2×2 取均值；DIFFWTD 按 `w_mask_c` 从两路差值推权重并下采样色度掩码；inter-intra 边界默认值（上 127 / 左 129）与 `II_MASK` 权重已对齐。
  - 帧级 warp 已闭合：ROTZOOM/AFFINE 的 alpha/beta/gamma/delta 与平移用 12 位 subexp 写入，重建按 8 抽头 warp 权重表做水平/垂直两遍滤波；8x8 块的 4x4 色度因 `imin(cbw4,cbh4)=1` 不走 warp，改用 GMV 在块中心导出的 MV 做普通 4:2:0 MC，5 组参数逐像素对拍通过。
- OBMC（2026-11-24 闭合）：8x16 双块流已由 libdav1d 真解；根因是默认 CDF 表按 dav1d 块尺寸顺序（首项 BS_8x8）存放，`obmc[3]` 取到 BS_16x8 概率，编码端与解码端区间错位使后续 chroma skip 解成 0/msac 越读。改用 `obmc[0]` 后 55 组参数全部解出两帧；h<=4 的亮度邻居 mc 改用 dav1d 窄块（3+FILTER）系数表（修正 phase10 系数）后 11 组 oracle 全部逐像素一致。
- 阶段五完成口径：① 上述全部模式经 ffmpeg/libdav1d 逐像素 PASS；② 覆盖逐块 warp、DISTWTD、多块分区与非 8x8 尺寸；③ ME/RDO 决策属阶段八，不作为阶段五完成条件。DISTWTD 已闭合：序列头开 `order_hint` + `jnt_comp`，comp_group_idx=0 的 joint 组写 jnt_comp 位（注意 dav1d 的 bit=0 才是 WEIGHTED_AVG、bit=1 是 AVG），d0==d1 ⇒ jnt_ctx=3、权重 7/9，在 4 位小数中间量上 `(a*7+b*9+128)>>8`，7 组 MV 逐像素 PASS。
- 逐块 warp 与多块分区（2026-11-25 闭合）：16x16 帧 `PARTITION_SPLIT` 的四个 8x8 NEWMV 子块，右下块 `motion_mode=MM_WARP`，PARAMETERS 由上/左/左上角三个邻居 MV 拟合出非单位 affine。关键语法结论：`is_motion_mode_switchable=1` 时**每个有可匹配邻居的帧间块**都要写 motion_mode（3 符号 CDF2，不是 4 符号），顶行/左列子块的 newmv 上下文按 `nearest_match` 分别是 2/2，右下是 4；DRL 位只在候选 MV 不同时出现（MV 相同的候选会合并），取 0 选中上邻居。推导出的矩阵经 `get_shear_params` 校验，越界则退化为普通 MC（8x16 单采样点即此路径）。`tools/av1_partition_probe.cpp` mode 3 + 门禁 6 组（含零 MV 与三组不同邻居 MV）逐像素 PASS，全量门禁合计 16 组约 267 例。

## 阶段六：全量变换与量化 —— 已闭合（8-bit 4:2:0 纵切口径）

- 规划：DCT/ADST/Identity（至 64x64）与量化矩阵。
- 已完成：DCT8 反变换 + 反量化最小闭环；`eob=0` 与非零残差往返通过；8x16 双块非平坦残差 61 组（模式 × 角度）经 ffmpeg/libdav1d 逐像素对拍全 PASS（2026-11-22 起门禁），阶段六的「跨解码器残差不一致」阻塞已消除。
- 历史修复：帧头 `base_q_idx` 曾让 dav1d 读成 200（dequant 389/639），预移位补偿后 qidx=100、dequant=93/112；`av1_idct8` 定点阶段与 dequant 同步校正；tile group 保留目标解码器需要的 `0x40 + tile_size` 前缀。
- 全量变换核（2026-11-25）：`src/image/codec/av1/transform/transform_2d.{hpp,cpp}` 用规范正交基实现 DCT（2/4/8/16/32/64）、ADST/FlipADST（4/8/16）、IDENTITY（4..64）的正/逆二维变换与任意矩形组合；三种系数扫描序（ZIGZAG/HORZ/VERT）覆盖全部 `TX_SIZES_ALL`。单测 `tests/unit/image/test_av1_transform.cpp`：DCT8 基与既有 bit-exact 残差链同构、全尺寸 IDENTITY 往返、DCT/ADST 混合往返、扫描序全排列。
- 量化矩阵（2026-11-25）：15 组规范的 4x4/8x8 亮度/色度权重表落到 `transform/qmatrix_data.hpp`；帧头 `using_qmatrix` + `qm_y/qm_u` 两个 4-bit 选择子已接线，反量化改为 `(dq * qm[rc] + 16) >> 5`。`av1_inter_probe` argv[24] 可选预设，门禁扫 0/3/7/11/14 五组 KEY+INTER 非零残差帧，ffmpeg/libdav1d 逐像素 PASS。
- 边界：ADST/IDENTITY 的**码流 tx_type 选择子**（`tx_mode=SELECTABLE` 的变换树语法）尚未接入编码器，当前码流仍走 `TX_MODE_LARGEST` 的 DCT_DCT；变换核本身已可用并有往返单测。量化矩阵只覆盖 8x8 亮度残差路径（其余块尺寸的权重表已在库中，接线方式相同）。ME/RDO 属阶段八。

## 阶段七：全量环路滤波 —— 算法已落地，逐像素门禁覆盖 Deblocking

- 规划：Deblocking、CDEF、Loop Restoration（Wiener / Sgrproj）。
- Deblocking（2026-11-25）：`postproc/deblock.{hpp,cpp}` 按规范实现 E/I/H 阈值、HEV 判定与 4/6/8/16 宽边缘的平坦区滤波；帧头补上 `level[0]/level[1]、level_u/level_v、sharpness` 与 delta 位。新增 `tools/av1_deblock_probe.cpp`，8x16 KEY 帧 8 组强度（1/4/15/16/24/32/48/63）经 ffmpeg/libdav1d 逐像素 PASS。
- CDEF：`postproc/cdef.{hpp,cpp}` 实现 8 方向搜索（partial_sum 与 div_table）、方向表、一/二级约束滤波与 8x8 块遍历。Loop Restoration：`postproc/loop_restoration.{hpp,cpp}` 实现 Wiener 7x5（H/V 抽头）与 Sgrproj 3x3/5x5 引导滤波。两者有单测（方向搜索、零权重 no-op、脉冲平滑、值域约束），**尚未接入码流 cdef_params/lr_params 做逐像素对拍**。
- 边界：本机 ffmpeg/libdav1d 在 sharpness>0 且 HEV 判定临界时与规范 E/I 表有边界差异，门禁只锁 sharpness=0 的 8 组；库实现保留规范公式。

## 阶段八：全量 RDO 与决策树 —— 决策核心已落地，编码器接线待做

- 规划：在划分/模式/参考帧间做全局率失真搜索，配 Early Termination 控制耗时。
- 已落地（2026-11-25）：`rdo/rdo.{hpp,cpp}` 用 16-bit CDF 自信息估码率、残差幅值估残差码率、SSD 计失真；`av1_rdo_pick_mode` 做候选模式遍历 + 码率下界/失真上界提前终止，`av1_rdo_should_split` 做划分 RD 比较。单测覆盖码率单调性、SSD、模式选择、提前终止与穷举一致性、划分决策。
- 边界：探针编码器当前仍写死模式/划分（写码逻辑未调用 RDO 选择器），参考帧/复合模式的全量决策树属后续接线工作。
- 真实帧入口（2026-11-27）：新增 `encoder/intra_encoder.{hpp,cpp}` 与 `image::encode_av1_ivf`，吃 RGB `Bitmap`，走 4:2:0 → 8x8 模式 RDO（区间编码器实测码率 + 重建 SSD）→ 16x16 NONE/SPLIT 决策 → 64x64 超级块铺帧 → IVF。同步修正 `write_partition_tree` 的 CDF 层级索引、子块 4x4 偏移、隐式 SPLIT 只递归单子块、NONE 节点更新划分上下文，以及 `PartitionContext` 未初始化邻居缓存的越界。公开整帧入口默认 `prediction_only=true`：只做预测选择、不写残差，已登记 5 组真实纹理帧逐像素门禁；两维都大于 64 的帧尚未与 libdav1d 对齐，入口显式拒绝。`prediction_only=false` 的残差路径仍属实验，不进一致性门禁。

## 当前站位与推进顺序

阶段一/二/三/四/五/六已闭合；阶段七、八的**算法核**已落地但端到端接线未做。下一轮按以下顺序推进（每项都以「解码像素逐像素一致」为出口条件）：

1. **阶段七接线**：CDEF → LR 的码流参数与编码端应用（见「尚未完成」A1/A2/A4）。
2. **阶段八接线**：先补 ME，再把 RDO 接到模式/划分/参考选择（见 B1–B5）。
3. **真编码主线**：真实分辨率、多超级块、多 tile、多帧参考链（见 C1–C3）。
4. **量化与码控**：rate control / AQ / delta_q（见 B7）。
5. **合规验证**：conformance 进 CI + 锁定解码器版本（见 D1）。

## 尚未完成清单（2026-11-25，逐条回源码核验）

> 下面每条都跑过递归搜索 + 读实现，标注证据位置；不写"应该是""大概"。上一版本节的「CDF 表只覆盖子集」经实测为错，已在 C6 更正。

### A. 阶段七（算法在库里，编码链上没人调用）

- **A1 CDEF 未接线**：`av1_cdef_filter` 全仓调用者只有 `cdef.cpp` 自身与单测 `test_av1_stage78.cpp`，编码/重建路径 **0 处调用**；`cdef_params` 六个字段在序列头全写 0（`headers.cpp:122-127`），即"声明 enable_cdef=1 但强度恒零"。待做：非零强度 + `cdef_bits>0` 写码 → 重建后应用 → 逐像素对拍。
- **A2 Loop Restoration 未接线**：`lr_type[3]` 全写 0（`headers.cpp:129-131`），Wiener 抽头与 Sgrproj `s0/s1/w0/w1` **没有任何写入函数**（全仓 `av1_wiener`/`av1_sgrproj` 调用 0 处），重建路径也不调用。
- **A3 三级流水不存在**：deblock 唯一生产调用是 `tools/av1_deblock_probe.cpp:61`，`av1_deblock_*` 不进任何编码器/重建函数；CDEF、LR 无调用 ⇒ deblock → CDEF → LR 顺序链未接。
- **A4 `sharpness>0` 未锁**：门禁 `deblock_cases = [(lv, 0) for lv in ...]`（`tools/verify_av1_minimal.py:591`）只跑 sharpness=0，脚本注释自陈是解码器在 HEV 临界点的差异（:589-590）；根因未定位。
- **A5 deblock 覆盖窄（实测）**：探针是单个 8x16 KEY 帧且 `loop_filter_level_uv = 0`（`tools/av1_deblock_probe.cpp:32-34`），**只滤亮度**；单测仅 3 个用例（level 0 no-op、4-tap HEV 分支、sharpness clamp，`tests/unit/image/test_av1_deblock.cpp:11/19/35`），**6/8/16 宽边缘与色度滤波无任何测试**。

### B. 阶段八（决策核有，但既没接线、模型也偏粗）

- **B1 RDO 无生产调用者**：`av1_rdo_pick_mode` / `av1_rdo_should_split` 全仓只在 `rdo.cpp` 定义与 `test_av1_stage78.cpp` 使用；`tile_group.cpp` 不 include rdo，模式/划分全硬编码。
- **B2 RDO 估计模型本身失真**（不是接线问题，是精度问题）：`av1_rdo_pick_mode` 把块长写死 64 样本（`rdo.cpp:45,51`），只支持 8x8；`av1_rdo_residual_bits` 用幅值二进制位数估残差码率，**不经 `cdf_manager` 的真实残差符号**（`rdo.cpp:17-27`）；`av1_rdo_should_split` 把划分额外代价写死 4.0 bit（`rdo.cpp:73`）。⇒ 块内排序可信，跨模式/跨划分比较不可信。
- **B3 ME 不存在**：全仓无 `motion_search` / `refine_mv` / 整像素搜索类符号；各探针 MV 全部来自命令行 argv。缺整像素搜索、亚像素 refine、DRL 候选生成闭环。
- **B4 划分树已接线但决策仍粗**：`write_partition_tree` 现由 `encoder/intra_encoder.cpp` 的生产路径调用（16x16 NONE/SPLIT 决策）；16x16 探针仍手工写符号，且划分决策只用 8x8 子块代价之和与 16x16 纯预测代价比较，未接全局 RD 搜索。
- **B5 tx_type 符号未写**：`tile_group.cpp` 里 tx_type 只出现在注释（:169），无写入调用；帧头 `tx_mode_select=0` 恒 `TX_MODE_LARGEST`（`headers.cpp:133/258`）。变换核与 `txtp_intra1/2`、`txtp_inter1/2/3` CDF 都已就位，缺的只是语法接线。
- **B6 warp 参数靠外部给**：帧级 ROTZOOM/AFFINE 矩阵与逐块 warp 参数由探针入参，无 GMV 估计器。
- **B7 无码控**：`base_q_idx` 是常量（`src/image/av1_encode.cpp:16` 与各探针），无 rate control / AQ；`segmentation_enabled=0`、`delta_q_present=0`（`headers.cpp:111/113`）。
- **B8 lambda 未标定**：lambda 是调用方裸值，没有 qindex/量化步长 → lambda 的换算函数。

### C. 支撑能力（挡住"真分辨率编码"）

- **C1 帧尺寸/超块/图像输入**：内部入口 `encode_av1_minimal_ivf` **只接受 8x8 且忽略像素内容**（仅校验尺寸，输出固定 SMOOTH 灰渐变，`src/image/av1_encode.cpp:10-38`）；`use_128x128_superblock=0`（`headers.cpp:68/156`）；探针帧只有 8x8 / 8x16 / 16x16。
- **C2 单 tile**：`uniform_tile_spacing_flag=1` 单 tile（`headers.cpp:100/231`），无多 tile 分支。
- **C3 帧类型/多帧链**：探针只用 `frame_type=0/1`（KEY/INTER，见各 `av1_*_probe.cpp`）；`show_existing_frame=0`（`headers.cpp:187`）；无 ALTREF/INTRA_ONLY/SWITCH、无多帧参考重用。
- **C4 位深/采样**：`high_bitdepth=0`、profile0 4:2:0（`headers.cpp:76-80`）；10-bit、4:4:4、4:2:2、monochrome 均不支持。
- **C5 其余工具语法硬关**：`enable_superres=0`、`film_grain_params_present=0`、screen content tools off（intrabc 不可达）、`frame_size_override=0`；palette/CCLM 预测器无实现。
- **C6 更正：CDF 表接近全量，缺的是语法**：`default_cdf.hpp` 实测含 `uv_mode` / `partition`（N_BL_LEVELS×4）/ `cfl_alpha` / `txtp_*` / `txsz` / `delta_q` / `delta_lf` / `restore_*` / `wedge_idx` / `comp*` / `drl_bit` / `pal_y` 等 40+ 张表，**不是"只覆盖探针子集"**；真正缺的是这些表对应的语法路径（palette、CCLM、intrabc、tx_type、segmentation 等，见 C5/B5）。

### D. 验证与工程

- **D1 conformance 未进 CI**：`.github/workflows/*.yml` 中 `conformance` / `verify_av1` **0 命中**；脚本本机 20/20 属人工跑，解码器版本未锁定。
- **D2 fuzz 无 AV1**：`tests/fuzz/` 现有 10 个目标（colormap/dsl/grapheme/input/mermaid/png/sixel/utf8），无 OBU/帧头/tile 解析。
- **D3 bench 无 AV1**：`bench/` 下 `av1` 0 命中，编码/滤波/RDO 无耗时基线。
- **D4 门禁仍是合成小帧**：最大 16x16（partition/OBMC 探针），且 `av1_encode` 忽略像素输入，没有"真实图像 → 编码 → 解码 → 源比对"的回归。

### E. 已拍板「不做 / 暂不做」

- 10-bit 与 4:4:4 profile：一期只交付 8-bit 4:2:0 纵切（见 `project_av1_phase1` 记忆）。
- 与 libaom 逐字节对齐：不做；判据是锁定解码器解出的像素一致，不是码流字节相同。
- dav1d/libaom 作为生产依赖：不做，只在测试/门禁期使用。

## 一致性基准（北极星）

摆脱解码器版本分歧的办法不是对齐某个 libaom/dav1d 版本，而是把**规范与官方一致性向量**作为最终裁判：

- **文本基准**：始终以 `AV1 Bitstream Specification v1.0.0 with Errata 1` 为准。
- **执行基准（考题）**：AOMedia 官方 / 受认可的一致性测试向量：
  - AOM CTC 镜像 `https://media.xiph.org/video/aomctc/`，其中 `av1_verification_test/` 用于解码器一致性验证；
  - libaom 自带测试流（`kAV1TestVectors`，`make testdata` 下载，如 `av1-1-b8-00-quantizer-00.ivf`）；
  - Argon Streams AV1（Graphcore 开源、AOMedia 推广），提供逐流 YUV MD5，是像素级正确性的强校验。
- **工具定位**：libaom 参考解码器与 dav1d 只是“解题工具”，不是考题；CI 应锁定一个**已知能通过上述向量集**的解码器版本作日常快速验算，最终交卷标准仍是向量。
- **对编码器的适配**：这些向量直接考的是解码器；本项目当前是编码器。因此向量用作“解码器端北极星”，编码器的等价最终判据是：生成的码流经该锁定解码器解出的像素与预期一致，而不是与某版 libaom 输出逐字节相同。

已落地的第一步：`tools/verify_av1_conformance.py` 用 AOM 官方 `aom-test-data` 的 20 个 quantizer 向量（8-bit `av1-1-b8-*` 与 10-bit `av1-1-b10-*`）校验解码后逐帧 i420 MD5，脚本按位深自动选择 `yuv420p` / `yuv420p10le` 与帧大小；缓存与解码临时文件固定在 `D:/tools/av1_conformance`。本机实测 `AV1 CONFORMANCE: PASS (20/20)`，说明当前 ffmpeg/libdav1d 组合可作为已通过官方向量的快速验算工具。

后续动作：把该脚本接入 CI 独立检查项，扩充向量集并锁定解码器版本；在此之前，`verify_av1_minimal.py` 只作为快速门禁，不作为合规证明。

## 验证基线（2026-11-25 实测）

- `./build/ninja/tests/termchart_tests.exe`：**1553/1553** 用例通过（434994 断言）。
- `--test-case=*av1*`：**47/47** 通过（含 `test_av1_deblock` 3 例、`test_av1_stage78` 5 例、`test_av1_encoder` 5 例）。此前记录的 26/26 已过时。
- `ctest --preset ninja`：95/95 通过（74s）。
- `python tools/verify_av1_minimal.py`：20 组 295 例逐像素全 PASS，含新增 `AV1 FRAME ORACLE (prediction-only)` 5/5。
- `python tools/verify_av1_minimal.py`：`AV1 MINIMAL ORACLE: PASS`，**19 组 292 例**逐像素一致：8x8 平坦 1、8x16 帧内模式/角度 61、filter-intra/CFL 37、KEY+INTER 两帧 1、NEWMV 亚像素 69、NEWMV 残差 5、qmatrix 5、帧级 GMV 12、复合 AVG 7、WEDGE 8、帧级 warp 5、DIFFWTD 8、DISTWTD 7、inter-intra 8、OBMC 11、逐块 warp 5、16x16 划分 28 + 6、deblock 8（sharpness 固定 0）。
- `python tools/verify_av1_conformance.py`：`AV1 CONFORMANCE: PASS (20/20)`（人工跑，未进 CI）。
