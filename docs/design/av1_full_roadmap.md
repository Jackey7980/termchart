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

## 阶段五：全量帧间预测 —— 主链通关，高级语法待接线

- 规划：7 参考槽、8 插值滤波器、Warped Motion、OBMC、Wedge/Masked 复合预测。
- 已完成：
  - `predict/inter_predict.{hpp,cpp}` 落地三档 8 抽头插值 + bilinear、整/亚像素块拷贝、4:2:0 色度半 MV 预测、复合平均/距离加权、OBMC 叠加与仿射 warp 双线性取样；
  - `headers.{hpp,cpp}` 增加非 `reduced_still_picture_header` 的序列头与 KEY/INTER 帧头（错误 resilient、7 参考槽、IDENTITY 全局运动、亚像素 MV 开关）；
  - `tile_group.{hpp,cpp}` 增加 INTER 块语法（skip/intra/ref/newmv/bitstream）与参考帧重建；`tools/av1_inter_probe.cpp` 生成 KEY+INTER 两帧 IVF；
  - `tools/verify_av1_minimal.py` 增加两帧门禁：ffmpeg/libdav1d 解出的 192 字节与编码器预期逐像素一致。
- 剩余边界：逐块 NEWMV 残差、帧级 warp 参数（subexp）、逐块 Wedge/Masked 复合与 inter-intra 的语法接线尚未闭合；当前稳定码流使用 IDENTITY 全局运动 + GLOBALMV 零 MV，高级工具以预测器原语与单测形态就位。

## 阶段六：全量变换与量化 —— 最小闭环已通，跨解码器残差待闭合

- 规划：DCT/ADST/Identity（至 64x64）与量化矩阵。
- 现状：DCT8 反变换 + 反量化已形成最小闭环；`eob=0`（全零残差）与非零残差的自研编解码往返均通过（`tests/unit/image/test_av1_obu.cpp`，AV1 单测 12/12）。
- 已定位并修复：插桩 dav1d 打印确认，原帧头让 libdav1d 把 `base_q_idx` 读成 200（dequant 389/639），与编码端 qctx=2 表不匹配；现以预移位写入补偿该一位偏移，实测 qidx=100、dequant=93/112。同步修正 dequant 值与精确 `av1_idct8` 定点阶段。
- 剩余阻塞：非平坦残差逐像素仍不一致。插桩显示 Y 模式/tx_type/eob 的解码符号与编码端意图仍有差异。另确认本机 `dav1d-src` 与 ffmpeg 内置 libdav1d 对单 tile 组的头字段解释不同：前者不读 `tile_start_and_end_present_flag`，后者需要 `0x40 + tile_size` 前缀；去掉前缀会让目标解码器直接失败，因此保留目标版本可解码的布局。

## 阶段七：全量环路滤波 —— 头已通，算法为零

- 规划：Deblocking、CDEF、Loop Restoration（Wiener / Sgrproj）。
- 现状：序列头/帧头中的 `cdef_params` / `lr_params` 已补齐，滤波算法本身未实现。

## 阶段八：全量 RDO 与决策树 —— 未开始

- 规划：在划分/模式/参考帧间做全局率失真搜索，配 Early Termination 控制耗时。
- 现状：仅写死规则，无 RDO。

## 当前站位与推进顺序

塔尖与入口已建好：阶段一完全通关，阶段二/四/六打出「一滴血」闭环。接下来按以下顺序把骨架填满血肉：

1. ~~**闭合阶段六**：非平坦残差跨解码器像素对拍~~（已由 8x16 61/61 逐像素门禁闭合）。
2. **阶段 2.1 之后**：把 69 种帧内模式的符号树铺入 CDF 状态机，建立动态 CDF 更新（61 模式默认 CDF 已接通，自适应 CDF 状态机已在阶段二落地）。
3. **阶段三**：从 `PARTITION_NONE` 扩到十叉树与跨边界上下文。
4. ~~**阶段四**：方向预测器、Paeth、CFL、filter_intra、intra_edge 逐个落地。~~（已通关）
5. **阶段五**：帧间预测 —— 参考帧主链已通，剩余 NEWMV/warp/复合模式的语法接线。
6. **阶段七**：环路滤波算法。
7. **阶段八**：RDO 与 Early Termination。

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

## 验证基线

- `ctest --preset ninja`：95/95 通过。
- `./build/ninja/tests/termchart_tests.exe --test-case=*av1*`：26/26 通过。
- `python tools/verify_av1_minimal.py`：`AV1 MINIMAL ORACLE: PASS`（8x8 平坦 1 组 + 8x16 模式/角度 61 组 + filter intra/CFL 37 组 + KEY/INTER 两帧 1 组，全部逐像素一致）。
