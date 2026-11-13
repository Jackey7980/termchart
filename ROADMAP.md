# ROADMAP.md

> **本文件是 termchart 项目推进的唯一进度口径。**
> 所有"做到哪了"的判断，只以本文件的勾选清单为准；不以口头描述、不以某次构建结果为准。
> 每个阶段结束时必须满足该阶段的「出口条件」，才允许进入下一阶段。

---

## 〇、如何使用本文件

1. **勾选即完成**：一个条目从 `[ ]` 变为 `[x]`，必须同时满足：代码落地 + 单元测试通过 + `check_headers.sh` 通过 + 相关校验脚本通过。缺一不可。
2. **阶段不可跳跃**：阶段 0 未完成，不得开始阶段 1。地基类工作后置的代价是指数级的。
3. **规格以 `结构.md` 为准**：本文件只回答"什么时候做、按什么顺序做"，不重复回答"做成什么样"。文件清单、层号、依赖、职责一律查 `结构.md`。
4. **生成物禁手改**：`结构.md` 附一/附四、`docs/reference/module_matrix.md`、`single_include/`、`tests/golden/` 全部由脚本生成，改了生成器必须重跑。
5. **修订流程**：调整计划走 PR，在本文件顶部 `CHANGELOG` 记录一行，不直接改勾选状态以外的东西。
6. **勾选清单是阶段规划快照**：历史阶段 0–8 的多数 `[ ]` 已由后续修订记录覆盖（见 CHANGELOG 与 `docs/reference/backlog.md`）。2026-11-10 按文件实证补勾 10 项。2026-11-13 按代码实证复核剩余 **29 条 `[ ]`**：其中阶段 9 的 C1/C2/C 出口与 D1/D2/D3/D 出口共 **7 条是真实缺口**（P 帧、精确 DBF / SAO / RDO，见 `docs/reference/hevc_engineering_roadmap.md` §2.3 与 P2/P3），其余 22 条为历史规划快照（阶段 0–8 已由后续修订记录覆盖，A1–A4 / B1–B3 共 8 条早已 `[x]`）。判断「做没做」以代码 + 修订记录最新行 + 子文档为准，不要按未勾选数量推断进度。

### 修订记录

| 日期 | 变更 | 影响阶段 |
| --- | --- | --- |
| 2026-11-13 | **首次推送触发全量 CI，修 5 个此前从未被 CI 覆盖的真问题**。① macos：`cocoa_demo.mm` 的 `Rect` 与 macOS SDK 全局 QuickDraw `Rect` 歧义（连带 double 窄化成 short），显式限定 `termchart::Rect`；② wasm：`quantize.cpp` 用了非标准的 `std::llabs`，Emscripten libc++ 无此成员，改手写绝对值；③ shared DLL job：`TermchartEmbed.java` 中文注释在 Windows runner 的 windows-1252 下 javac 报 unmappable character，给 javac 加 `-encoding UTF-8`；④ tsan（及所有 sanitizer）：JVM 无法加载插桩 native 库（static TLS block），`jni_smoke` 在 sanitizer 构建跳过、正常构建仍跑；⑤ encoder-cert：物理上界断言按 6 形态全量矩阵标定，在 quick 的 2 形态矩阵误报，改为仅全量启用。前四项都是 11-11 之后首次进 CI 的 GUI/WASM 插件代码，本地 Windows 覆盖不到——这正是把 CI 真跑起来的价值。**验证**：本地四档 oracle `PASS bad=0`、`ctest` 92/92、`check_all` 452 头全绿。 | 1、9 |
| 2026-11-13 | **HEVC 阶段三 P3-1 收口：DBF 精确化，开 DBF 时 `recon == dec`**。① 逐像素诊断（编码端开 DBF + ffmpeg 不 `-skip_loop_filter all`）把差距定位清楚：亮度 4×4/8×8 TU 档**早就 maxdiff=0**（旧文档「亮度 maxdiff≈4」过时），16/32 档差 2~4 是 TU 内部 8 对齐边被过度滤波，U/V 各档差 4~10 是缺色度通路；bS 恒 2 对全帧内码流是规范结论而非近似。② 补 8.7.2.5 色度 bS=2 弱滤波（只改 p0/q0；色度按 **8×8 色度块 = 亮度 16×16** 边界，周期下限 8 而非 4）；固定档按 `edge_period = max(8, 1<<log2_tu)` 跳过非 TU 边界；自动档在逐 CU 发射时记录 8×8 网格真实 TU 边界图 `vert_edge`/`horz_edge`。③ `tools/verify_hevc_tu_sizes.py` 增 `--dblock-on`：两端都开 DBF 的逐像素一致性门禁 + 15 组「DBF 开关前后 Annex-B 逐字节相同」断言；CI `encoder-cert` 加 quick 门禁。④ 单测补 3 组（色度公式手工展开、边界周期、边界图）。**验证**：60 组开 DBF 黑盒对拍 Y/U/V 全部 `max=0 neq=0`；四档全量 oracle `PASS bad=0`（base / transform-skip / force-skip / dblock-on 各 6 形态 × 4 QP × 5 档），`--dblock-on` 全量码率与 base 逐项相同印证 DBF 不进码流；`ctest` 92/92、doctest **1449 用例 / 261747 断言**、`check_all` 452 头全绿。 | 9 |
| 2026-11-13 | **底座清扫：shared 构建真 bug 修复 + 文档真值 + 工程尾巴**。① **首次真跑共享库构建**（`TERMCHART_BUILD_SHARED=ON`），暴露两类静态构建永远看不到的问题：15 处头文件内联定义误标 `TERMCHART_API`（MinGW 报 "definition is marked dllimport"，纪律写进 `export.hpp`）；Windows DLL 只导出显式 dllexport 符号导致上百个 `undefined reference`。**不采用**「给上百个声明逐个补标记」（无法靠类型系统验证、还会误扩 ABI 面），改由 CMake 在 shared + WIN32 下加 `-Wl,--export-all-symbols`，`TERMCHART_API` 只保留「显式 ABI 承诺」语义。② **修真 bug**：`proxy_sys_posix.cpp` 缺 `#if !defined(_WIN32)` 保护，`read_system` 与 win32 版重复定义（shared 直接链接失败；静态构建更危险——可能静默取到 POSIX「恒返回 None」实现，令 Windows 注册表代理探测失效）。③ CI 新增 `shared build (windows mingw DLL)` job；macos job 加 Cocoa 静态库产物 + `cocoa_offscreen` 注册与冒烟的显式断言。④ **API 清扫 P1 结案**：`Graph::add_node`(`Node&`) / `add_edge`(`Edge*`) 判为**刻意不对称**（失败通道不可丢、统一 Id 要改 195 处调用点且零收益），不做破坏性改名，改把「返回值仅供即时使用、长期持有走 `find`/`find_edge`」写进 `graph.hpp` + 回归用例；`quality_todo.md` 两项由 `[ ]`/`[~]` 结案。⑤ 补 `test_mp4_encode.cpp` 的 `video_qp` 透传单测（注明需 `video_mode=Intra`，默认 `Pcm` 刻意忽略 QP）。⑥ 文档真值：`CODEBUDDY.md` 的 Qt/Tk 与 `backend_tk` 说法、`hevc_engineering_roadmap.md` 的 P4 QP / 9×9 窗口 / DBF 旧段落、`backlog.md` 的 WASM「不进 CI」与 Cocoa 状态、`capability_matrix.md` 数字（416 头 / 366 cpp / 87 示例 / 193 测试源 / ctest 92 / doctest 1446）、`TermchartOptions.cmake` 的 WASM 注释，以及 29 条 `[ ]` 分类（7 条真缺口 + 22 条历史快照）。**验证**：静态 `ctest` 92/92、`check_all` 452 头全绿；shared 构建链接通过并跑 **doctest 1446 用例 / 261734 断言 / 0 失败**；`single_include` 已重生成。 | 1、9 |
| 2026-11-13 | **HEVC transform_skip 收口：色度通路、决策口径、`trace_headers` 逐字段门禁**。① **修真 bug**：色度 transform_skip 的 RD 比较把 `(px,py)` 写成 `(0,0)`，SSD 读的是整个色度平面左上角；同时误用亮度 λ（色度走 qPi→qPc 后 QP 更高）。这两处使上轮 QP40 的 U/V PSNR 掉到 23.7 dB，当时被误判为「色度不该 skip」而整体回退。现改为本块坐标 + `auto_lambda(cqp)`，色度在 4×4 与大 TU 两条路径都按 RD 参与，`--force-skip` 端到端验证 `flag=1` 的色度残差语法。② **决策口径对齐**：`eval_tu_cost` / `eval_split4_cost` 原来恒用 `kScanDiag` 估 bin 数，而发射层 log2<4 会按帧内模式切 VERT/HORIZ；改为同样调用 `scan_idx_for_intra_mode`，并把 4×4 评估抽成共用函数。③ **两层 RDO 实测并回退**：分裂侧改用子节点最优代价，虽让 QP0/10/40 反超固定档，却把 6 形态总码率抬高 2.1%（15717→16049 B）且 QP26 无改善，判为负收益不采用。④ **参数集逐字段门禁**：`verify_hevc_tu_sizes.py` 用 `ffmpeg -bsf:v trace_headers` 核对 `general_profile_idc=4`、`sps/pps_range_extension_flag=1`、`transform_skip_context_enabled_flag=0`、`log2_max_transform_skip_block_size_minus2=档位−2`，Main 档则核对不写 extension；CI 三条 quick 门禁自动覆盖。⑤ **物理上界断言**：自动档 SPS 每个 8×8 CU 多写 1 个 `split_transform_flag`（一帧 128 bin ≈ 16 B），脚本据此给出 `auto ≤ best_fixed + 上界` 的可验证判据，替代上轮模糊的「启发式未标定」表述。⑥ `hevc_encoder_handoff.md` 第 0 节补现状指针、`encode.hpp` 注释与 `single_include` 同步。**验证**：6 形态 × 4 QP × 5 档 × 3 模式 = 360 条码流 + 96 条码率对照 + 11 组 trace_headers，`PASS bad=0`；自适应 skip 相对关闭 skip 的最佳固定档总省 21.0%；`ctest`、`doctest`、`check_all` 全绿。 | 9 |
| 2026-11-12 | **HEVC `transform_skip` 解除 4×4 互斥（RExt 参数集 + 大 TU 残差语法）**。① `Vps/Sps` 增 `general_profile_idc`（Main 1 / RExt 4，`profile_tier_level` 位宽不变，兼容标志 bit1/bit4），`Sps::range_extension` 写 `sps_range_extension()` 的 9 个开关（全 0）；`Pps::log2_max_transform_skip_block_size` 在 >2 时写 `pps_range_extension()` 的 `_minus2`，=2 时**不写 extension**（沿用 FFmpeg 默认值，既有 PPS 字节数不变）。② `cu_coding.cpp` 的 8/16/32 TU 分支在 cbf=1 时按 `transform_skip_enabled` 写亮度 + U/V 的 flag；残差 bin 序列逐位不变（`transform_skip_context` / rotation / RDPCM 全关，ctx 与 Rice 同常规块）。③ `hevc_encode.cpp` 按 `max_skip_log2 = transform_skip ? (auto?5:log2_tu) : 0` 声明范围，每 TU 在「常规变换 / 跳过变换」间按 **RD（SSD + λ·bins）** 选优（最初按纯 SSE 选，16×16 QP10 码率翻倍 2635→6284 B，已修）；色度同样按 RD 参与（2026-11-13 修正：早期把色度 SSD 的坐标写成 (0,0)、读了整个色度平面左上角，且误用亮度 λ，才导致 QP40 的 U/V PSNR 掉到 23.7 dB；改用本块坐标 + 色度 QP 的 λ 后由数据决定，多数块仍选 DCT）。关闭 / 未声明的路径逐字保留，默认码流不变。④ `verify_hevc_tu_sizes.py` 增 `--transform-skip` / `--force-skip`、新增 `dots` 稀疏亮点形态，并把门禁升级为「编码端重建 == ffmpeg 解码输出逐字节相等」（`-DTERMCHART_HEVC_RECON_DUMP` + `TERMCHART_HEVC_RECON_PATH`），QP40 强制 skip 的低 PSNR 被证明纯属量化噪声（`neq=0`）。**验证**：6 形态 × 4 QP × 5 档 × 3 模式 = 360 条码流 + 96 条码率对照，`PASS bad=0`；自适应 skip 相对关闭 skip 的最佳固定档省 43.4% / 45.0% / 21.8% / 7.7%（QP0/10/26/40），总 15717 B vs 19902 B（−21.0%）；CI `encoder-cert` 加两条 quick 门禁；新增 2 组单测（RExt 位序列 + 大 TU flag 语法）。**已知边界（2026-11-13 已定量）**：自动档 SPS 的 MaxTb=32 + depth=1 让每个 8x8 CU 都要写 `split_transform_flag`，固定 4x4 档隐式分裂不写，一帧 128 bin ≈ 16 B 是参数集固有开销；QP26 skip 档 auto 比同模式固定 4x4 多 9 B 正落在其中，不是决策退化。两层 RDO 已实测并回退（总码率 +2.1%），调折扣亦会把其它 QP 抬高 14–19%，故脚本改用「收益断言 + 物理上界断言」。 | 9 |
| 2026-11-12 | **HEVC 阶段一收口：4/8/16/32 TU 端到端 + 逐区域自动 TU 档 + 模式决策修复 + 外部 oracle 入库 CI**。① 残差语法泛化到 log2 2..5（多 CG：`significant_coeff_group_flag` / last_sig prefix+suffix / `ctx_idx_map` 按 prev_sig / greater1·2 / Rice），帧内预测泛化到 n×n（含 1-2-1 参考平滑与边缘扩展），CU 树按档位停止分裂并写 `split_transform_flag`；修掉 FFmpeg 语法顺序（两个 prefix 先于两个 suffix）与 SCAN_VERT 系数转置两处错位。② 修复模式决策错配：旧 `choose_intra_luma_mode_8x8` 无论 TU 多大都用 4 个 4×4 TU 算 SSD，现按真实 TU 尺寸模拟预测+变换量化。③ 新增 `hevc_log2_tu_size=0` 自动档：逐 CU 节点比较「整块单 TU」与「4 个子块」的 `J = SSD + λ·bins`（bin 数由 `encode_residual` 实算，模式用 35 模式真实决策），自底向上折扣近似子树最优，8×8 CU 可再降到 4×4。④ `tools/verify_hevc_tu_sizes.py` + `tools/hevc_tu_probe.cpp` 入库（自编译、不依赖构建产物），CI `encoder-cert` 加 `--quick` 门禁。**验证**：5 形态 × 4 QP × 5 档 = 100 条码流，全部经 ffmpeg 软解逐像素对拍 `PASS bad=0`；自动档总码率 **10885 B**，优于任一单一固定档（最佳 13729 B，**−20.7%**），四个 QP 分别 −19.7% / −26.8% / −13.1% / −7.8%；默认 4×4 档码流逐字节不变；doctest 全量、ctest、`check_all` 全绿。`transform_skip` 当时仍限 4×4（同日后续已解除，见上一条修订记录）。 | 9 || 2026-11-10 | **HEVC 阶段一色度链路补齐（彩色内容不再丢色）**。① 此前 `cbf_cb/cr` 恒 0、`rec.u/v` 恒 128，彩色图全变灰度；现按 FFmpeg `hevcdec.c:1484-1506` 在第 4 个亮度 TU 之后写 U/V 残差，`cabac.c` 的 `c_idx>0` 上下文偏移（last_sig +15 / sig_coeff +27 / greater1 +16 / greater2 +4）成套切换，色度变换用 DCT-II、QP 走 `qPi→qPc` 映射。② 色度预测复用亮度 35 模式逻辑，新增 `ChromaPredPlane` 处理三处 c_idx 语义：坐标 /2 采样色度重建平面、`size_in_tbs` 色度为 2（`pred_template.c:123-126`）、跳过亮度专属的 DC 首行/首列滤波 与 mode 10/26 后处理（`:515/:542`）。③ `intra_chroma_pred_mode=4`(DM) 保持不变，色度预测模式跟随亮度模式、扫描表与亮度共用。**验证**：5 形态 × 4 QP 的 `recon U/V == ffmpeg dec U/V` 全部 `maxdiff=0`，彩色 grad / hstep 图 U/V PSNR **37.9~50.6 dB**；G0 6/6、三尺寸 35/35 模式与多 CTB 模式决策 maxdiff=0、G5 硬解 64/64 `hardware=ACTIVE`、QP 扫描体积 1432→924→703→498 B 与 PSNR 43.55→39.08→36.42→34.32 dB 双单调、E2E PASS、4K `hd.4k.mp4` 72146 B 且 d3d11va 与软解 Y 平面 maxdiff=0、doctest **1421 用例 / 239157 断言**、ctest **91/91**、`check_all` 全绿。④ 另把 CI 的 `macos-14` 改为 `macos-latest`（该 job 多次 0 步骤即 cancelled）。 | 9 |
| 2026-11-10 | **修掉 P0-5 真 bug + 4K 模式决策提速 + 历史勾选真值补勾**。① **P0-5 根因**：FFmpeg `hevcdec.c:2753` 的 `end_of_tiles_y` 是**当前 CTB 行底部**，`mvs.c:58` 据此在每个 CTB 行最后一行关闭 bottom-left 扩展；编码端旧代码按整帧高度判定，读了下一 CTB 行尚未编码的重建区，导致 `mode 0/2..9` 在多 CTB 行尺寸下 `recon != dec`。改为按 CTB 行底判定后，128×64 / 256×128 / 640×384 三尺寸 **35/35 模式与模式决策均 maxdiff=0**，默认模式决策**放开此前排除的 10 个模式**；此前文档误写成「top-right」已更正为 bottom-left。② **4K 性能**：`choose_intra_luma_mode_8x8` 原本每 CU 整帧分配 + 整帧 memcpy（12 MB × 32640 CU ≈ 390 GB），改为复用持久缓冲、只同步预测会读的 9×9 邻域窗口（窗口外 `POS()` 从不访问，语义逐位不变），**564 s → 39.1 s（14.4×）**，码流 62049 B，d3d11va 与软解 Y 平面仍 maxdiff=0。③ 按文件实证补勾 10 项历史规划条目（dsl/interaction/widget/app/plugin/chart 子目录/示例/单头生成器/Doxyfile/gui 头），并在「如何使用」新增第 6 条说明勾选清单是阶段规划快照；阶段 9 的 C/D 仍如实未勾。**验证**：G0 6/6、G5 硬解 64/64、QP 扫描体积 1114→744→579→419 B 与 PSNR 43.55→39.08→36.42→34.32 dB 双单调、doctest 全绿、ctest 91/91、check_all 全绿。 | 9 |
| 2026-11-09 | **阶段 9 真值同步 + CI 联网复核 + AV1 交接边界**。① 把阶段 9 的 A1–A4（拆基建/参数集/CABAC/ctb_tree）与 B1–B3 及出口按代码实证勾选：35 角度模式、三套系数扫描、逐 CU SSD 模式决策、标准 4K d3d11va 硬解均已落地；C（P 帧）/D（DBF·SAO·RDO）保持未勾选，真实缺口见 `hevc_engineering_roadmap.md` §2.3。② 联网复核最新 CI：**ci 工作流 8 job 中 7 success + `macos (clang)` cancelled**（整条 run 因此显示 cancelled，不是编译失败；`guardrails (py3.12)` 已转绿）；docs 工作流 build success / deploy failure，日志明确为 **GitHub Pages 未启用（404 Not Found）**，纯仓库设置，与代码无关。③ **AV1 由用户亲自开启**：本轮不写任何 AV1 实现，`VideoCodec::AV1` 继续 `is_video_codec_supported=false` + `encode_mp4` 返回空串的 fail-loud 契约（单测锁住），只备好交接材料（AV1 规范 OBU 头 / `leb128()` 章节、本机 ffmpeg 具备 libaom-av1 等真实编码器可作 oracle）。④ 历史阶段 0–8 的勾选清单仍滞后于代码，本次只同步阶段 9 已实证项，不越界改其它条目。 | 9 |
| 2026-11-09 | **HEVC 阶段一收口：35 种帧内模式全通、模式决策落地、I_PCM 黄金基线复原**。① 补齐帧内模式→系数扫描的隐式映射（FFmpeg `hevcdec.c:1396-1403`：模式 6..14 用 SCAN_VERT、22..30 用 SCAN_HORIZ，其余 DIAG），三套扫描表 + `significant_coeff_flag` ctx 表 + `last_significant_coeff_x/y` 交换成套切换；② 修 MPM 左邻居跨 CTB 边界误判（`ctb_left_flag || x0b`，`x0>0` 即可用）；③ 修 `rem_intra_luma_pred_mode` 求逆（prev=0 分支候选表先升序排序）；④ 修 DC 首行/首列边界滤波（无条件 `3*dc+2` 式）与通用 `predict_intra` 语义；⑤ 参数集按档位分离（`log2_diff_max_min_transform_block_size`：PCM=3 / Intra=0）、`CabacEncoder::init_contexts` 复原用 `init()`，**G0 回到 6/6 GREEN**（20433 B / sha `5d4adef5…` / `dec_U[0:16]` 逐字节相等 / `Y[0]==16` / FFmpeg 3 帧 0 error）；⑥ 新增逐 CU SSD 模式决策（35 模式按真实 TU 次序模拟重建链），QP26 Y-PSNR 38.40→39.14 dB；默认候选集暂排除 mode 0 / 2..9（跨 CTB 行 top-right 扩展参考未与 FFmpeg 帧缓冲对齐，登记为 P0-5）；⑦ `Inter` 档核实无任何实现，保持显式返回空串的 fail-loud 契约并同步公开头注释。**验证**：G0 6/6、G5 硬解 64/64（hardware=ACTIVE，worst Y-PSNR 32.33 dB）、35/35 模式 `recon == dec` maxdiff=0、256×128 多 CTB 模式决策 `recon == dec`、QP 22/26/30/34 体积与 PSNR 单调、doctest **1418 用例 / 238946 断言 0 失败**、ctest **91/91**、`check_all` 全绿（451 头独立编译 / 生成物一致 / 分层通过）。 | 9 |
| 2026-11-07 | **新增 HEVC 编码器演进路线（阶段 9）**：I_PCM 保留为显式档位、不参与分层拆分。新增 `VideoEncodeMode{Pcm,Intra,Inter}`（默认 `Pcm`），`encode_hevc_pcm_annexb` 自包含；`diag_hevc.py` 钉死 6 条严格验收（逐字节 `dec_U[0:16]==enc_U[0:16]`、`Y[0]==16`、FFmpeg 3 帧 0 error、码流 20430B / sha 精确），**6/6 GREEN**。H.264 侧同步修正帧内 CAVLC 码流语法与重建一致性（intra CBP 表、TrailingOnes、level 遍历范围、8×8 象限块序、DC 边界预测等）：16 组分辨率 FFmpeg 0 error、纯色 MAE 0、渐变亮度 Y-MAE < 2。**进度口径**：阶段 9 条目暂不勾选——单测与 `check_headers.sh` 未补，按「勾选即完成」四条件执行。 | 9、16 |
| 2026-10-31 | **WebP 无损解码落地（`image` 缺口再收口）**。`decode_image` 的 WebP 分支改走**原生零依赖 VP8L 解码器** `image/webp_decode`（与 `encode_webp` 对称）：从码长独立复算 canonical 码（按位反转发射、按反转键查表），**不信编码器自己的码字**，规避"同错同过"；约束与 v1 编码器输出一一对应——**无 Predictor/ColorTransform/SubtractGreen/ColorIndex 变换、无色彩缓存、单一 htree group**；含变换的外部 WebP 仍回落 stb（可选依赖）或如实 `Unsupported`（fail-loud，不做猜着解）。公开 API `decode_webp(data,size)->Result<Bitmap>`。`tests/unit/image/test_webp_decode.cpp` 9 用例覆盖 4/3 通道回环、渐变全 256 值、容器损坏→`ParseError`、含变换→`Unsupported`；另更新 `test_stb_image`（WebP 已原生，移出"需 stb"断言）。**验证**：doctest 1365 用例 / 233147 断言 0 失败（含 WebP 编解码回环逐像素精确）；ctest 90 项中 89 通过、1 项 `example_33_mermaid_flowchart` Not Run（**预存、他人 mermaid/dsl 在途工作，与本次无关**）。**未做**：外部带变换 WebP 的本地解码（仍依赖 stb）、AVIF 编码/解码、WebP 体积优化（LZ77/变换）。 | 5、16 |
| 2026-10-31 | **WebP 无损编码落地（`image` 缺口收口）**。`image/webp_encode`（自包含零依赖 VP8L lossless 编码器）：RIFF/`WEBP`/`VP8L` 容器（奇数 chunk 补 RIFF pad）、LSB-first 位写器、canonical Huffman 码字**按位反转**发射（解码器按反转键查表，反了则 Kraft 失败）、码长码（CLCODE，simple_code=0 / 13 个 3 bit 长度 / `use_length=0`）、平面码长表（GREEN 280 字母表 = 232×len8 + 48×len9，R/B/A 各 256×len8，DIST 40 字母表 `{1,1}`）、像素恒发 4 符号 G,R,B,A（`has_alpha=0` 时 alpha 强制 255）；头部顺序严格为 transform 位 → color cache 位 → **meta-Huffman 标志位**（漏发即整流偏移 1 bit，ffmpeg 报 `Invalid data found`）。v1 刻意不做 LZ77/变换/自适应码长（体积约 4 字节/像素），换的是可验证的正确性。`ImageFormat::Webp` 接进 `encode()` 分发器。**验证**：`tests/unit/image/test_webp_encode.cpp` 自带**独立最小 VP8L 解码器**（不信编码器自己的码字，避免同错同过），9 用例 91 断言；libwebp 1.6.0 / PIL / ffmpeg 三方接受且**逐像素零失配**（solid 64×48、gradient 48×32、奇数 pad 5×1）；示例 `85_webp_lossless`（Scene→SceneRenderer→Bitmap→encode_webp，800×450 24 色带，PIL 采样逐点吻合、113 种颜色）；全量 doctest 1360 用例 / 233120 断言 0 失败。**未做**：LZ77 回溯与 Predictor/ColorTransform（体积优化）、WebP *解码*仍走 stb（**已于同日补上原生零依赖解码器**，见上）、AVIF 编码未开工。 | 5、16 |
| 2026-10-28 | **图表统一 `to_scene` 大迁移 + GeoMap 收尾 + GUI 渲染库越界崩溃修复**。`chart/` 各图种（cartesian 核心 7 + 剩余 14、indicator 5、text 4、statistical 11、flow 2、polar 8、geo 5、hierarchical 6、financial 6 等，共 73 个图表头已接 `to_scene`）统一产出 `scene::Scene` IR；`chart/geo`（GeoMap）补齐分级图例/色标、命中测试、离线地图数据分发、TopoJSON 缝合离线回归；`backend` 加 SVG/PDF 文字轮廓化 `text-to-outline`。GUI 平行栈 `termchart_gui` 修复 `lerp_geom`→`recompute_tangents` 的 `ChartGeom` 不变式漏洞（`xs`/`ly` 长度须 == `n`；`gauge`/`polar` 只设 `n=1` 却留空 `xs`/`ly`，动画收敛时喂出畸形几何导致 `vector<double>` 越界 abort；天气 demo 联网必崩），并加 `--dump`/`--simulate`/`--feed` 诊断开关，gdb 复现并验证真实联网交互不崩。 | 12、13、22 |
| 2026-10-25 | **图表走真实 `scene::Scene`（不是 Canvas 转录）+ 按名动画**。用户明确纠偏"不要虚假的 SCENE CANVAS"，故不把 `Canvas` 逐格抄成 `TextOp`，而是新增真正的几何通路：①**落笔口抽象** —— `layout::Sink`（`put`/`text`/`line`）+ `CanvasSink` + `chart::SceneSink`，轴/网格/标题/图例的**布局决策仍只有一份**（此前想导 Scene 就得把布局抄一遍，两份迟早算出不同列）；②**分组指令** `scene::GroupOp` 追加在 `Command` variant 末尾（index 8，`kind_of` 是直映射），`SceneBuilder::begin_group/end_group` 可嵌套，`Visitor` 默认空实现 → SVG/PDF/ScenePainter 零改动；`StatefulVisitor` 显式空 override 且**不得**调 `finish_path()`（否则路径被劈断、SVG 多一条 `<path>`）；③**图种迁移**：`LineChart::to_scene`（折线 `StrokeOp+MoveTo/LineTo`，动画部分段按数据坐标插值后 `LineTo`，标记点保留 `●`）、`BarChart::to_scene`（`FillOp+RectOp` 实心柱，半块光栅）；**实线判定按轴向**——轴向走 `─│` 字符（盲文会把 `│` 光栅成 `⣿`、`─` 成 `⠛`，细线胖成实心带），只有非轴向几何才走矢量；④**API**：`api::chart_to_scene` / `chart_to_svg`，未迁移图种返回 `Unsupported`（fail-loud，不给假几何）；SVG 字号默认 1.0（场景单位=字符格）；⑤**动画**：`Chart::add_animation/tick/progress_of`（`AnimationTrack{duration,delay,easing}`，未 `tick` 恒 1.0 → 静态路径零影响；零时长先判 `!(duration>0)` 避免首帧卡 0）；缓动下沉 `util/easing`（L1）解 chart 不可依赖 app 的层号倒挂。**示例** `77_chart_animation`、`78_chart_scene`（一份 Scene 三个消费者：终端/`<path>` SVG/`GroupOp` 命中）；`api::chart_to_scene` 的 0 尺寸返回 `InvalidArgument`。**验证**：doctest **1253 用例 / 232092 断言 0 失败**（新增 `test_chart_animation`、`test_chart_scene` 8 例）、ctest **86/86**、`check_all` 全绿（429 头独立编译、生成物一致、分层通过）；修复 `layout/label.hpp` 重写时丢失的 `HAlign::Center` 默认实参（示例 4 参调用编译失败）。**未做**：其余 21 个 cartesian 图种与 polar/statistical/… 仍只有 `draw()`（`to_scene` 返回 false），口径见 `backlog.md`。 | 3、7 |承接真实脏订单表的探针验收：①**裁剪口径单一真源**——`ChartCandidate::final_clip_desc` 由路由用全量值算出，图题后缀与 `api` 的 WARN 逐字同源，`final_clip_count()` 给真裁数；api 不再读 `features.outliers[].decision`（抽样口径，旧病根：图题"裁去 2 个极值"、告警"不裁轴"）。②**X 轴假精度**——真凶不是错年，而是 `scale::ticks_for` 在 epoch 秒上按 10 的幂取整（50000s），把 41 小时的数据撑到 02-28 17:33 ~ 03-03 01:06；改走 `nice_bucket_seconds` 日历档位 + `LineChart::set_x_ticks` 显式下发，刻度落在整 12 小时。③**JSONL 位置式数组行**——宽度严格等于列数才按列序 zip，计 `lines_zipped`；不匹配照旧丢弃计 `lines_skipped`（错位比丢行更糟，不猜列）。④**百分比量纲**——JSON 轨补 `map_scalar_flags`（此前只有 CSV 轨打标记，同一数据两条入口两套结论）、`ColumnProfile` 增 `percent_cells/mixed_units`（仅"部分格是百分比"算冲突），混列时百分比格按缺失处理并留 note，**拒绝 ×100**（伪造金额）。连带修掉：空值率对量纲闸双重记账（1 个 15% 把折线解析分 0.968 压到 0.918、输给事件柱状图）、`features` 离群扫描误报"0.15 是极端离群值"、时间行无法解析时静默跳过。**文档**：新增 `docs/limits.md`（位置数组 / 百分比混列 / epoch 数字串 / 时间格式族与时区），`guide/autoviz.md` 同步单一播报口径，CHANGELOG 建立 0.9.0 段。**验证**：doctest **1223 用例 / 231819 断言 0 失败**（新增 10 例）、ctest **84/84**、`check_all` 全绿（425 头独立编译、生成物一致）。 | 4、21 |
| 2026-10-28 | **GeoMap 收尾 + `chart/geo` 文档同步**。**上轮**把 `examples/67–69` 改成真 GeoJSON 栅格化（盲文/象限/半块三条出口 + 分级着色 + 位图/PNG），并修掉 `tools/topojson_to_geojson.py::stitch_arc` 两处解码错误；本轮补齐此前**没做**的收尾项：①分级 `bins` 在 `draw`/`to_scene`/`rasterize`/`render_bitmap` 三字符出口一致生效（抽出 `geo_bin_of`/`geo_bin_center` 单一分级函数）；②图例/色标——`legend_bins`/`render_legend` 复用 `layout::draw_legend`，`render_bitmap` 加 `draw_legend` 在右侧留白带画纵向色标；③`hit_test(lon,lat)` 命中（even-odd 射线法，HitRegion 基础）；④删 `GeoMapOptions` 三个死字段（`width`/`height`/`show_graticule`）；⑤`install(DIRECTORY assets/maps ...)` 随安装包分发离线地图。`tests/unit/chart/geo/test_geo_map.cpp` 现 48 用例 0 失败。**文档**：`capability_matrix.md` 第五节模块表加 `chart/geo` 行（登记 GeoMap 4 投影/3 字符出口+位图/图例/命中）、示例索引补 67–69；本文件补此行；`docs/charts/geo.md` 早于本轮同步（口径从"库不做投影/不带数据"改为现状）。**验证**：doctest 1318 用例 / 232525 断言 0 失败、ctest 全绿、`check_all` 全绿。 | 12 |
| 2026-10-21 | **network 支持代理：显式 + 自动探测（Clash / Cloudflare WARP 场景）**。承接用户"支持 Clash 那类和 WARP 那类"的需求。经批判性修订后的设计：①**两种协议都做**——HTTP CONNECT（企业代理主流、Clash mixed 端口原生、系统代理设置自动对接）+ SOCKS5（WARP proxy 模式与 `ALL_PROXY=socks5://` 的唯一覆盖）；②**自动探测链**：显式 `HttpOptions::proxy` > 环境变量（https 目标 HTTPS_PROXY→ALL_PROXY，http 目标 HTTP_PROXY→ALL_PROXY，curl 语义不交叉；NO_PROXY 命中跳过）> Windows 注册表系统代理（`ProxyEnable`/`ProxyServer` 两种格式/`ProxyOverride` 绕过）/ macOS SystemConfiguration > 直连——Clash 开"系统代理"后**零配置可用**；③**分级 fail-loud**：显式代理连不上/隧道失败 → 报错不回落；探测来的不可用 → warn 后回落直连（用户 10-15 定过的原则）；④SOCKS5 用 ATYP=03 域名——DNS 交代理解析（分流生效与防污染的关键）；⑤实现纪律：探测/隧道全部落 `src/network/proxy_*.cpp`（私有头 `proxy_internal.hpp`，平台差异按既有隔离纪律进 `proxy_sys_win32/posix`），公开面只有 `HttpOptions::proxy` 一个字段；字节构造/响应解析抽纯函数离线测试（`tests/unit/network/test_proxy.cpp` 9 用例）。**验证**：doctest **1113 用例 / 194806 断言 0 失败**、ctest **79/79**、`check_all` 全绿；**端到端协议实测**（本机 Clash 未运行，用 127.0.0.1 假代理服务完整跑通两种隧道：SOCKS5 与 CONNECT 均 200 + body 正确；真机 Clash 待用户开启后验收）。已知边界：HTTP 代理的摘要/协商类代理认证未做（CONNECT 407 时 fail-loud 报 Unsupported）。 | 7 |
| 2026-10-21 | **修复 network https 全线瘫痪（MinGW SCH_CRED_VERSION 宏坑）+ 凭据自检回归**。用户报"network 模块根本跑不了，真机也不行"。分层探针定位：TCP/HTTP 明文全绿，卡在 SChannel 握手。**根因**：MinGW-w64 的 `schannel.h` 结构体已是 V4（含 `dwCredFormat`），但 `SCH_CRED_VERSION` 宏仍停留在 `0x00000002`；SChannel 对 `dwVersion` 精确匹配，传 2 被 `AcquireCredentialsHandleW` 以 `SEC_E_UNKNOWN_CREDENTIALS`（0x8009030D）拒绝——**所有** https 握不了手。该现象 10-19 首落地时即已命中，却被误判为"沙箱策略限制"写进 `schannel.hpp` 头注释（错因已修正）。**修复**：`make_cred()` 显式 `dwVersion=4`（握手与自检共用，零初始化保证 `dwCredFormat=0`）；新增 `schannel::credentials_available()`（不联网，只验证本机凭据获取）；`test_network.cpp` 加 dwVersion 回归用例（Windows CI 的 msys2 MinGW 同编译器家族，真实验证；POSIX 恒 false）。**附带澄清**：示例 71 的两个镜像域名（raw.githubusercontent / jsdelivr）在当前网络被 SNI 阻断（同环境 curl/PowerShell 同样失败，example.com/baidu.com 正常）——网络层问题非库缺陷，示例注释已修正归因。**验证**：修复后对可达站点全链路绿（http/https GET example.com 均 200、schannel handshake Ok）；doctest **1104 用例 / 194734 断言 0 失败**、ctest **79/79**、`check_all` 全绿。 | 7 |
| 2026-10-21 | **「数据帝国」三模块落地（math=1 / io=2 / data=4）+ dsl/json 收编下沉 + CI 三平台 + 示例 75**（承接用户"一图胜千言，数据定乾坤"方案，按批判性评估后的修正版执行）。①**新模块 math（L1，header-only 5 头）**：`stats`（mean/median/variance/stddev/covariance/pearson，Kahan 补偿求和 + Welford 单遍方差，NaN 跳过）；`interpolate`（`linear_at` + `CubicSpline` 自然三次样条——Thomas 三对角 O(N) 构造、越界钳位不外推、坏输入 `valid()=false` fail-loud）；`fft`（radix-2 Cooley-Tukey 原地，twiddle 每 stage 直算不用递推，逆变换缩放 1/N 保证往返恒等，非 2 幂/空/不等长返回 false 不改数据）；`linalg`（稠密 n×n 行主序 multiply/inverse/solve/determinant，部分选主元高斯消元，span+double 无模板）；`special`（gamma/ln_gamma/erf/erfc 统一 NaN 边界口径 + Bessel J0/J1/Jn 级数+Hankel 渐近自实现）。②**新模块 io（L2）**：`csv`（零拷贝 `string_view` 状态机 + `from_chars` 数值列，delimiter 自动嗅探/引号转义/BOM/注释行/CRLF，空与 NA 族→NaN，非数字字段带行列诊断报错）；`json`（**dsl/json 核心 git mv 下沉收编**——`ParseDiagnostic` 随之下沉 `io/diagnostic`，`dsl/json.hpp` 与 `dsl/parser_base.hpp` 改 using 别名 shim，dsl 全部测试与调用点零改动）；`stream`（`LineReader` 64KB 分块 + compact/翻倍超长行路径，任意长行不 OOM）。③**data 模块上移**：util/data 四件套 git mv 至 data=4（上移前仅 1 个测试消费者，成本最低时点），namespace `termchart::data`；新增 `ema`（O(N) 递推，alpha=2/(window+1)）与 `ohlc`（时间轴等宽分桶 → open/high/low/close，桶边界锚定首样本，K 线图数据入口）。④**两处"刻意不合并"的裁定**：io/csv 与 dsl/csv_sniff 并存——后者输出 `document::TableModel`(L8)，结构上无法下沉 io=2；HTTP 拉取归 network(L7)，io=2 不得触层（原方案此点违反层号硬墙，已规避）。⑤**示例 75_gui_cjk_4d**（缝合怪：4D 超立方体固定帧角 + TTF 中文标题/注脚 + SceneRenderer 3× 超采样 → PNG；无头确定性实证两次 SHA256 一致、标题带 ink 656px=真字形量级）+ Win32 `hypercube_4d_demo` 加中文叠加层（`--png` 无头实证 860×620）。⑥**CI 三平台**：ci.yml 补 macos (clang, macos-14) 作业；核实两个风险均不存在——仓库无任何目标启用 `-Werror`、`TERMCHART_BUILD_PYTHON` 默认 OFF。⑦WASM (Emscripten) 目标记入 backlog（未开工）。**验证**：doctest **1103 用例 / 194733 断言 0 失败**（较此前 +29 用例）、ctest **79/79**、`check_all` 全绿（每头独立编译、生成物一致、header_only 自洽——独立编译护栏抓住 `math/stats.hpp` 缺 `<algorithm>` 一处真缺陷）、分层校验通过；实测 372 头 / 309 cpp / 75 示例 / 156 测试。已知遗留（backlog 已登记）：SMA 的 O(N) 滑动求和化、PCHIP 单调样条、io/csv 与 csv_sniff 的远期合并、data/io 到 chart 的端到端接线。 | 1、2、4、8、21 |
| 2026-10-21 | **4D 超立方体 GUI 真窗口版（`plugins/gui_win32/example/hypercube_4d_demo`）**。承接用户选的 C 方案：把终端 braille 版（`74`）的同一套 4D 数学搬到 GUI 平行栈的**真实窗口**。`Hypercube4DWidget : Widget` 每帧 `chart4d::wire4d` 产 Scene → `gui::SceneRenderer`（超采样抗锯齿真彩）→ `image::Bitmap` → `TkCanvas::blit` 上 `Win32Backend`（GDI StretchDIBits）；复用 `panda_demo` 的 Scene→blit 骨架与 `win32_demo` 的 OS 消息泵事件循环。交互：拖拽=轨道旋转（方位/仰角）、滚轮=缩放、空格=暂停/继续自转、R=复位、ESC/关闭按钮退出；自转由主循环每帧 `tick()` 推进（XW+YZ 两平面）。`--png` 无头出图（不需桌面、CI 可验证）、`--shot` 窗口首帧快照、`--size/--ss`。`Wire4dOpts` 增 `edge_width`（GUI 粗线 2px，终端默认 1.0 不变）。**验证**：doctest **1074 用例 / 194476 断言 0 失败**、ctest **78/78**、`check_all` 全绿（397 头独立编译、生成物一致）；`--png` 无头实证 860×620 RGBA、~7000 点亮像素、包围盒居中、两次运行 SHA256 一致（确定）；独立构建目录（`TERMCHART_BUILD_GUI_WIN32=ON`）下 zoom 1.3–1.7 全程 32/32 边不裁。已知边界：真窗口需交互桌面（`Win32Backend::valid()` 为假时退出码 2 并提示用 `--png`），故默认 ctest 不含（与 panda/win32 等插件 demo 一致）。 | 3、12、22 |
| 2026-10-21 | **4D 管线落地（`math4d` + `chart4d`，此前暂停的 4D 计划重启）**。新模块 `math4d`（L3，header-only）：`Vec4`；`Mat5`（5×5 齐次、矩阵×列向量，`transform` 取前 4 行）；**4D 的 6 个旋转平面** XY/XZ/XW/YZ/YW/ZW（`rotate_plane(i,j)` + 6 个具名，正角把第 i 轴送向第 j 轴）；4D 平移/缩放；`Camera4`（4D→3D 透视投影——相机架在第 4 轴 +W，`w_view=distance-w`，`scale=focal/w_view` 即“除以 W”的齐次除法，相机处/后方/近裁剪内不可见，`focal_from_fov_deg` 与 3D 同式）。新模块 `chart4d`（L12）：`Projector4D` 实现 `chart3d::IProjector`，**两阶段** `Vec4 --(4D 旋转+透视除 W)--> Vec3 --(内层 IProjector，通常 Projector3D)--> 画布`（另覆写 Vec3 入口转发内层，故也能插进 scatter3d/surface3d）；`wire4d` 线框/点云发射器（边按两端平均深度着色 + 顶点方块）。示例 `74_hypercube_4d`：16 顶点/32 边超立方体，绕 XW+YZ 两平面同时自转，braille 光栅 + `app::HeadlessSession` 差分重绘跑 60 帧（帧角=帧号×定步长；真终端按 `term::Terminal::stdout_is_tty` 门控 ~40ms/帧节流看得见旋转，piped/CI 不 sleep 保持确定）。层表登记 `math4d=3`/`chart4d=12`、header_only 登记 4 头。**验证**：doctest **1074 用例 / 194476 断言 0 失败**、ctest **78/78**、`check_all` 全绿（397 头独立编译、生成物无 diff）；实机确认 60 帧全程 32/32 边可见（真实 3D 相机无剔除）、第 0 帧 ASCII 居中成形。 | 3、12 |
| 2026-10-21 | **架构技术债偿还：字体后端与 3D 投影缝多态化**（本会话，尚未提交）。①`gui/font` 抽 `FontBackend` 纯虚基类（`has_glyph`/`measure_width`/`draw_glyph`，定义在 `include/termchart/gui/font.hpp`，vtable 锚点按 key function 惯用法落 `src/gui/font.cpp`）；`src/gui/font.cpp` 实现三派生——`BitmapBackend`（原 5×7 点阵，ASCII 像素契约原样）、`TtfBackend`（封装 `image::TtfFont`，系统字体发现：Windows 试 `msyh.ttc`/`simhei.ttf` 等 + `TERMCHART_GUI_FONT` 覆盖，ASCII 区间主动让位点阵以保像素契约）、`FallbackBackend`（有序组合链，进程内单例装配 `{TTF, 点阵}`，TTF 失败安静降级、全不覆盖才画占位框）。公开 API（`has_glyph`/`measure_text`/`draw_text`）签名与语义零变化。②`chart3d` 抽 `IProjector`（新头 `chart3d/projector.hpp` + `src/chart3d/projector.cpp`），现有 `Mat4`+`Camera` 封成 `Projector3D`；`scatter3d`/`surface3d` 增可选尾参 `const IProjector*`（nullptr = 按 opts 现算 `Projector3D`，既有调用方零改动、默认路径逐字节不变），为未来 `Projector4D`（4D→3D 产出 Vec3 再经本接口 3D→2D，两阶段各自可替换）铺路。**验证**：doctest **1059 用例 / 194415 断言 0 失败**（新增 4 个投影缝用例；字体契约 41 例全绿）、ctest **77/77**、护栏四件套全过（357 头 / 304 实现、392 头独立编译、生成物一致、分层过）、单头已重生成 | 8、12、22 |
| 2026-10-21 | **gui/font 接系统 TTF：中文上屏**（本会话，尚未提交；TTF 路径随后被上一行的多态化重构吸收进 `TtfBackend`）。非 ASCII 码点改走 `image::TtfFont`：Windows 自动试 `C:/Windows/Fonts/msyh.ttc`（ttc 集合取首字体）/`simhei.ttf`；CJK 宽度仍按显示宽度占 2 列（TTF 只画格内像素，advance 不变）；字体缺失/解析失败/码点无轮廓一律 fail-safe 退回 5×7 虚线占位框，不崩不留白。示例 `examples/73_gui_cjk`：中文标题「中文测试」+ X 轴「一月/二月/三月」折线图导出 PNG。实机验证：msyh.ttc 命中、标题带 ink 446px（占位框仅 ~150px 量级）、当轮 doctest 全绿 | 8、22 |
| 2026-10-20 | **单头生成器修复**（7127fda）。`gen_single_header.py` 的 `RE_INTERNAL` 要求 include 行尾仅空白，漏剥带尾随 `//` 注释的内部 include（`chart3d/common.hpp` 首例），活动 include 与已内联内容重复定义、example 42 单头用法编译失败；改为容忍行尾注释后重剥并 `--check` | 7 |
| 2026-10-20 | **Python 跨语言绑定打通**（8530c3f，handoff ④① 收口）。修掉写坏的 pybind11 脚手架（原调用不存在的 `api::quick::*` 与 `termchart::version`），改接真实 API `api::render_plot/render_scatter/render_bar`（返回字符串、不写 stdout）+ `TERMCHART_VERSION_STRING`；`import termchart` 下 plot/scatter/bar/version 四入口验证可用，返回真实 ANSI 真彩图；pyproject.toml 走 scikit-build-core | 21 |
| 2026-10-20 | **工程卫生补齐**（c190fa2，handoff ④④ 收口）。新增 `bench/bench_render`（`app::render_once` 在 80×24/120×40/200×60 的耗时）与 `bench/bench_svg_backend`（Scene→SVG 在 60/240/480 段），均 `--quick` 进 ctest 冒烟；新增 release.yml / docs.yml 两个 GitHub 工作流 | 0–8 |
| 2026-10-20 | **3D 图表落地**（545d142 + c2bc7fa，handoff ④P1 收口）。新模块 `math3d`（L3，header-only：vec3 / mat4 行主序 / camera 轨道相机+透视投影+视锥裁剪，深度只供着色、刻意不另造 z-buffer）与 `chart3d`（L12，header-only：`scatter3d`/`surface3d` 产出 `scene::Scene`，复用 render_scene 终端管线与超采样位图）；示例 `72_3d`（马鞍面线框 + 散点叠绘）。层表登记 `math3d=3`/`chart3d=12`、header_only.txt 登记 6 头；单测覆盖矩阵乘法/平移缩放旋转/投影到画布中心/相机后方剔除。**已知边界**：曲面只到线框档，无实体填充/遮挡排序/轨道交互（投影缝已于 10-21 多态化，见上） | 8 |
| 2026-10-19 | **image 解码扩容 + 自主 TTF 解析**（643f1e4 / e22bf8c / 454e172）。BMP 解码器原生零依赖落地（1/4/8 调色板、24/32/16-BITFIELDS、bottom-up/top-down、行 4 字节对齐；RLE4/RLE8、OS/2 12 字节旧头、尺寸/掩码越界显式拒绝）接入 `decoder.cpp` 分发；vendored `stb_image`（`third_party/stb/`，CMake 可选、默认 OFF）接入 JPEG/GIF/WebP，配套 `examples/71_fetch_stb` 用自带 network 后端拉取 STB 头（`--fetch` 才联网）；**自主 TTF 解析** `image/ttf_font`（sfnt 目录 / cmap format 4+12 / glyf 简单+复合轮廓 / loca 短长 / hmtx + 二次贝塞尔自适应展平→扫描线非零环绕→4×4 超采样 + glyph cache；CFF/OTTO 显式 Unsupported）。验证（454e172 轮）：doctest 1036 用例 194158 断言零回归、ctest 74/74、护栏四件套过、349 头独立编译 | 5、16 |
| 2026-10-19 | **network 模块 + widget 控件三件 + dsl/JSON**（e8cef17 / 0c2808f / dc205d1）。`network`（L7）：TCP socket + HTTP/1.1 客户端（Uri 解析、chunked、HEAD/204-304 规则）+ TLS 注册表 + SChannel 后端（fail-loud，不静默降级）；`graph/route` 修 `bfs_around` 重复展开与绕行预算收口；dsl 增 JSON 解析；widget 即时控件三件（dropdown 键盘导航 / 文本框码点编辑 / radio 组），延续无状态即时模型 | 7、13、17、18 |
| 2026-10-18 | **真地图 + 数据处理层 + PDF 导出**（bc8f005，三项此前的真实缺口同时收口）。`util/geo/geojson` 解析 + `chart/geo` 真地图图种（GeoJSON 投影，示例 67/68/69）；`util/data` 聚合与重采样（数据处理层从零到有）；`backend/vector/pdf_backend`（PDF 导出从零到有，示例 `70_export_pdf`）；顺带修路由缺陷与单头拼接 | 1、5、8、13、16 |
| TBD | 初稿建立 | 全部 |
| 2026-10-17 | **ChartWidget 窄窗适配 + demo 观测者效应修复**（用户实机截图驱动）。①**X 轴标签抽稀**：`build_base_scene` 按可用槽宽算步长 `lstep=ceil(44/slot)`，窄时隔 lstep 个画一个（保留首末），消除标签重叠成乱码块；②**图例窄窗隐藏**：plot 宽 <150px 不画图例；③**掉帧自诊断默认静默**：`paint_all>33ms` 告警原每掉帧 `fprintf` 到控制台，而往 PowerShell 刷屏比画图还慢 → 观测者效应越刷越卡，改仅 `--profile` 时打印；④**Win32 最小窗口尺寸**：`WM_GETMINMAXINFO` 设 `ptMinTrackSize=320×240`。**验证**：doctest 953/192680 绿、gui 30/30、`chart_demo --smoke` 默认静默 exit 0、零告警 | 8 |
| 2026-10-17 | **Win32 上屏"拉伸旧图"消灭 resize 黑洞**。`Win32Backend` 增 `last_frame_`（上一帧）+ `canvas_fresh_` 标志：`resize_to` 在画布为已渲染帧时存旧图源并标记非新鲜；`present` 渲染完置新鲜；`blit`(WM_PAINT) 新鲜则画布 1:1 上屏，**否则把上一帧 `StretchDIBits` 拉伸铺满新客户区**（硬件级微秒拷贝，消除拖拽/最大化时暴露区涂黑与残影，画面死咬鼠标边缘）。平滑"对焦"由 ChartWidget 指数趋近动画承担，故不做交叉淡入/防抖（功能重叠、且动画已无跳变）。仅改 Win32 插件，核心/ChartWidget 零侵入。**验证**：doctest 953/192680 绿、`chart_demo --smoke` exit 0、零告警；拉伸路径需真机窗口观测（无头 smoke 只覆盖 1:1 新鲜路径） | 8 |
| 2026-10-17 | **resize 拖动中实时更新（Win32 模态 resize 循环不卡）**。病因：拖动边框时系统进入**模态 resize 循环**（卡在 `DefWindowProc`），应用 `tick+paint` 主循环被阻塞、跑不到；此前 WndProc 的 `WM_SIZE` 只把 Resize **入队**，等松手、模态循环退出才处理 → "拖动卡住、松手才动"。修法：`Win32Backend` 加 `set_render_hook(w,h)`，`WM_SIZE` 里 `resize_to` 后**就地回调**（传新客户区宽高）；`ChartWidget` 加 `request_snap()`（下次 paint 直接吸附到目标几何，不等 tick）。demo 接线：钩子内 `root.set_bounds(新尺寸) + chart.request_snap() + mount.paint_all()` → 拖动全程图表实时跟着变（吸附式，无过渡残留），松手即已到位。仅改 Win32 插件 + demo，核心零侵入；`WM_SIZE` 用局部变量避开 `handle(HWND h,...)` 形参对高度成员的遮蔽。**验证**：doctest **954 用例 / 192683 断言、0 失败**（gui 31/31，新增 `request_snap` 吸附到位例）、`chart_demo --smoke` exit 0、零告警；实时拖拽需真机窗口观测 | 8 |
| 2026-10-17 | **resize 拆分策略：拖动调柱宽/间距、松手重算抽稀**（对齐 ECharts"内容随容量变"）。`ChartWidget` 加 `set_dragging(bool)` + `label_step_`（已提交抽稀步长）：`build_base_scene` 里抽稀步长**松手/静态时按可用宽度现算并提交**、**拖动中冻结**（标签数量不变、只随宽度重排位置，避免拖动时标签反复增删闪烁）；`set_dragging` 同时废底图缓存，确保松手那帧按最终宽度重建、一次性增删数据显示。柱宽/间距本就从宽度逐帧算，故拖动中天然实时调。demo：`WM_SIZE` 钩子内 `set_dragging(true)`（拖动只调柱），主循环 Resize 处理里 `set_dragging(false)`（松手重算抽稀）。**验证**：doctest **955 用例 / 192686 断言、0 失败**（gui 32/32，新增"拖动冻结抽稀、松手重算"例：宽=step1/拖动中仍=1/松手窄=step>1）、`chart_demo --smoke` exit 0、零告警 | 8 |
| 2026-10-17 | **resize 平滑：拖动走低清快路径 + 高清帧交叉淡入 + 大尺寸降 supersample**（治"小窗→最大化卡一下"）。病因：最大化时尺寸骤变→snap 成 settled→直接跑那帧昂贵的"精美帧"（三层 SS2 全分辨率，几百 ms）→ 卡住。修法三连：①paint 的 fast/pretty 判定由 `!animating()` 改为 `provisional = animating() || dragging_`——**拖动/变大期间一律走低清快路径**（降分辨率 + 最近邻放大，~30ms，不卡）；②`ChartWidget` 加 `prev_base_` + `fade_`（`kFadeDur=0.16s`）：**低清(降分辨率)→高清切换时交叉淡入**（旧低清帧打底 + 高清帧 `blit(opacity=smoothstep(fade_))` 铺上），掩盖硬切与高清帧耗时；③高清帧 `supersample` 按面积封顶（`W*H>40万` 用 SS1，否则 SS2），缩短大尺寸高清帧耗时。`tick` 同时推进几何趋近与 `fade_`；`animating()` 纳入 `fade_<1`。仅低清降分辨率→高清才淡入（小改动 d=1 不淡，避免无谓）。**验证**：doctest **956 用例 / 192689 断言、0 失败**（gui 33/33，新增"低清→高清交叉淡入"例：拖大到 d=2→松手 `animating` 真→tick 收敛）、`chart_demo --smoke` exit 0、零告警 | 8 |
| 2026-10-17 | **ChartWidget 交互高光（"残影高光"/丰富动画）**。用户要 ECharts 那种"多轨道演出"的丰富感，先上其指定的最低成本一招：`glow_` 强度——resize/数据变化时 boost（连续交互维持、`kGlowDecay=5` 指数衰减回 0），柱/折线/marker/图例色按 `glow_color(基色, 高亮色, glow_)` 向亮色插值（变化时"闪一下"再衰减回基色，Material-Ripple 质感）；首帧不 boost（保底图基色断言）；`animating()`/`tick` 纳入 `glow_`（衰减期间持续重绘）。**性能回归修复**：上上轮"折线采样同一条 smooth_y_at"误用"每 2px 采样"，全屏时折线暴涨到近千段粗线 → 1080p fast raster 从 ~30ms 反弹到 42–107ms；改为**按数据段采样（每段 16 点，点数 ~N·16 与宽度无关）**，既保几何一致又压回成本（pretty 199→87ms）。**验证**：doctest **956 用例 / 192691 断言、0 失败**（gui 33/33，request_snap 例纳入高光衰减）、`chart_demo --smoke` exit 0、全量零告警 | 8 |
| 2026-10-17 | **高光衰减调快 + 悬停强调（axisPointer/emphasis）**。①用户反馈交互高光衰减偏慢（像"慢渐变"），`kGlowDecay` 5→9（时间常数 ~200→110ms），更利落；澄清：柱子是纯色、无渐变，"渐变慢"即高光衰减。②新增**悬停强调**：MouseMove 命中后，画布上叠加**竖向十字轴线**（1px 竖条，ECharts axisPointer）+ **悬停柱亮描边**（ECharts emphasis），再画 tooltip。**未做（说明原因，非偷懒）**：标签淡出/淡入——文字渲染不吃 alpha，要单独文字层，不值；面积透明度错峰——需把面积层从缓存底图拆出单独逐帧合成；柱子错峰长高/弹性回冲——与指数趋近打架或显廉价（ECharts resize 也不过冲）。**验证**：doctest **956 用例 / 192692 断言、0 失败**（gui 33/33，新增悬停十字轴线像素断言）、`chart_demo --smoke` exit 0、全量零告警 | 8 |
| 2026-10-17 | **ChartWidget「骨肉分离」：万能插座 + 渲染插头**。用户方案：把"画什么"从 ChartWidget 抽成可插拔 `IChartRenderer`，让同一套动画/缓存/降分辨率/淡入/悬停十字轴同时驱动 combo 与 K 线。①新增 `widget_tree/i_chart_renderer.hpp`：`ChartGeom`（共享几何）+ `geom_equal`/`lerp_geom`/`recompute_tangents`/`smooth_y_at`/`nice_step`/`fmt_int` 等 inline 辅助 + `IChartRenderer` 接口（`compute_geom`/`build_base/area/line_scene`/`hit_index`/`draw_hover`/`draw_tooltip`/`set_combo_data` + `background`/`foreground`/`empty`/`label_step`）；②`ComboRenderer`（`combo_renderer.hpp`）从原 ChartWidget 掏出柱+折线+面积+网格+图例+标题+tooltip+悬停柱描边；③`HsiKLineRenderer`（`hsi_kline_renderer.hpp`，浅色 MSN 风、绿涨红跌、价格头/统计条/OHLC 信息框）即用户"心心念念"的 HSI K 线，复用同一插座；④ChartWidget 改为持有 `std::shared_ptr<IChartRenderer> renderer_` + `set_renderer(...)`，动画/tick/缓存/fade/降分辨率/悬停十字轴全保留，构造函数默认 `ComboRenderer`（向后兼容：未显式 set_renderer 仍是 combo，已落地的 956 测试零改动）；⑤`chart_demo.cpp` 演示左 combo + 右 HSI 并排（root 改 `FlexDirection::Row`）；`candlestick_demo.cpp` 仍走独立 `CandlestickWidget`（未删，避免动已注册示例）。**验证**：doctest **956 用例 / 192692 断言、0 失败**（gui 33/33 全过，combo 路径像素级断言不变）、`check_headers` 全绿（三新头独立编译）、`chart_demo`/`candlestick_demo` 构建零告警 | 8 |
| 2026-10-17 | **城市生活气象看板（Open-Meteo 真实数据）接入「万能插座」**。用户转向大众场景：手机天气 App 每天几亿人看，最契合本库渲染能力。①`ComboRenderer` 泛化：支持**多折线系列 + 可选柱 + 可选面积（指定 series）+ 指定 series 最大值高亮点 + 每系列颜色/图例/标题可配**，默认参数完全复刻原组合图外观（柱蓝/面积青/折线青/标题），故 956→957 用例零改动；新增 `ChartWidget::invalidate()`（绕过 `set_data` 的多系列/天气数据更新后请求重绘）。②新增 `weather_demo.cpp`：上下堆叠（FlexLayout Column）两块 ChartWidget——上半屏 24h 温度（平滑折线+面积+最高温高亮，降水% 作第二条折线），下半屏 7 天降水概率柱+最高/最低温折线；数据走 **Open-Meteo**（免费/无 Key/HTTPS），**后台线程 WinInet** 拉取（主循环零卡顿），极简 JSON 提取（零第三方依赖，不引入 httplib/nlohmann/json）；拉取失败/超时回退**内嵌真实样本**（北京 2026-10-17 实拉数据）。③`test_gui_stack.cpp` 新增"ComboRenderer 多系列（天气看板）渲染"无头用例，验证 24h 橙线/最高温高亮与 7 天柱/双折线关键色像素、且关面积后无青色层。**验证**：doctest **957 用例 / 192698 断言、0 失败**（gui 34/34，新增天气渲染用例通过）、`check_headers` 全绿。`weather_demo`/`chart_demo`/`candlestick_demo` 构建零告警。已知差距（待 phase 2）：用户设想的 AQI 仪表盘/环形图、风向极坐标/雷达图**仅存在于核心库**（`chart/polar/*`、`indicator/gauge_kpi.hpp`，终端/SVG 管线），尚未封装成 GUI `IChartRenderer` 插头；7 天图目前单 Y 轴（降水%与温度共用），双 Y 轴为后续打磨 | 8 |
| 2026-10-17 | **天气看板 phase 2：双 Y 轴 + AQI 仪表盘 + 风玫瑰（全原生 GUI 插头）**。用户拍板"全部都做"。①**双 Y 轴**（`ComboRenderer`）：`set_right_axis(bool, vector<int> idx)` 指定哪些折线系列走独立右轴量纲；`compute_geom` 左/右量纲分别 nice 取整、右轴预留右边距；网格只按左轴画保持背景干净；右轴右侧加刻度字（与右轴系列同色提示）；图例/tooltip 对右轴系列标 "(R)"；默认关闭 → 既有用例零改动。②**两个原生 GUI 渲染插头**：`GaugeRenderer`（270° 分段色带 + 指针 + 轴帽 + 中心值/类别，AQI 优/良/轻度/中度/重度分色）与 `PolarRenderer`（同心环 + 8 向方位辐条 N/NE/…/NW + 24h 风 向×速 极坐标折线 + 当前点高亮）——均直接吐 Scene 指令、复用 `ChartWidget` 动画/缓存/暗色主题，**不桥接核心库字符化 gauge/polar**（核心 `chart/polar/*` 输出终端字形而非 Scene 矢量，桥接只会得低分辨率字符表盘，故手搓像素版，借力在 Scene IR 这一层）。③`weather_demo.cpp` 升为四联屏（手写布局，规避未接线的 Flex 权重）：左上 24h 温度（平滑折线+面积+最高温高亮）、右上 AQI 仪表盘、左下 7 天（降水柱左轴 + 温度折线右轴）、右下 风玫瑰；全 ASCII 标签（GUI 字体仅 95 ASCII 点阵，中文会成占位框）；数据扩为 Open-Meteo 实拉 + 空气质量的 `us_aqi`（后台双 WinInet 拉取）；**修掉用户指出的 pending 数据竞争**——`pending` 读写加 `std::mutex` 并 `memory_order_release/acquire`（原 seq_cst flag 其实已建立 happens-before，加锁是防御性加固，防有人改 relaxed 或静态分析误报）。④`test_gui_stack.cpp` 新增 Gauge/Polar 渲染（断言红区分段与指针、风折线关键色）+ 7 天测试启用右轴覆盖双 Y 轴路径。**验证**：doctest **959 用例 / 192701 断言、0 失败**（gui 36/36，新增 3 例通过）、`check_headers` 全绿、`weather_demo` 等构建零告警 | 8 |
| 2026-10-17 | **图布局硬化：force 引力收拢 + 硬碰撞移除；sugiyama 连通分量预分离**。用户审查指出五点（force 防飞散/碰撞、NS 迭代上限、多重连通图间距、KK 抖动、metro 性能）。逐条用源码裁决：①force 已是 Fruchterman-Reingold+温度退火（位移被温度钳死、递减），连通图不飞散 → 保留；但**无连边分量**缺向心力、**硬碰撞检测为零** → 真缺口，已补：朝原点的弱引力（`ForceOptions::gravity` 默认 0.05，确定性、不破 golden）+ 落格前 O(n²) 重叠分离趟（最小穿透轴推开、8 轮松弛）。②NS `while(true)` 内 `break`（无任何割能改进→最优）已保证合法 DAG 上必然终止，且管线先跑 cycle_removal，无需迭代上限防护。③**多重连通图**只 sugiyama 缺（kamada_kawai 已有 connected_components+cursor 错开）→ 已补：sugiyama 末段按分量整体 X 向错开 cluster_gap（=max(2·h_gap,4)）重排，分量内几何逐字节不变、长边骨架随分量平移，**单分量图 golden 不受影响**；此改动暴露并修正两处测试隐患——`make_crossing_graph` 与单层交叉图原本是**断开图**（两条无桥链），旧代码把它们糊进同列，现分离后顺手用桥接边（`c→f`、共享子节点 `X`）补成连通图以保留"降交叉"测试原意。④KK 局部极小/卡死：`kMaxLocalSteps=100`+`max_iterations=200` 已封顶成本，**推回"随机抖动"**——撞本库确定性 golden 硬约束（force.hpp 明写"同图两次布局一致才能进回归"），正确做法是 node-id 种子确定性扰动，优先级垫底。⑤metro Z 绕行受链拓扑约束，标"待观察"。**验证**：doctest **961 用例 / 192717 断言、0 失败**（graph 布局 52/52 全过，新增 force 断开图不重叠 + sugiyama 双分量错开 2 例）；`check_headers` 全绿、全量零告警 | 8 |
| 2026-10-17 | **util 层三处审查回应（format 缓冲 / text 丢字 / grapheme RI）**。用户以"首席审查官"抠出三雷：①**format.cpp 缓冲溢出（雷区1，真）**——`kBufferSize=64` + `to_chars(fixed,12)` 对 1e300（~313 字符）返回 `value_too_large` → 空串 → 坐标轴标签变空白。已修：`kBufferSize` 提到 512（任意有限 double 的 fixed 表示 < 512）+ 极端值退回 `scientific`（有界可读、永不为空）双保险。②**text.cpp wrap 丢字（雷区2，真，但机制与描述不同）**——`piece` 本就是 `next_boundary` 切出的**单个簇**，`prefix_by_width` 对"超宽首簇"返回 0 → `substr(pos,0)` 空行、`pos+=step` 把整簇吞掉；用户提的 `pos += prefix_by_width(...)` 反而会**死循环**（前进 0 步）。正确修法：超宽单簇整簇保留（宁可越过行宽也不丢字）。③**grapheme.cpp RI+Extend（雷区3，误报，已推回）**——用户称 `🇨🇳︎🇺🇸`（RI RI FE0F RI RI）应并一簇；但按 UAX #29 GB12/GB13，RI 连缀计数遇非 RI（FE0F 是 Extend）即清零，其后第二个旗标对与前者不配对（prev 不再是 RI）→ 必在 Extend 与第二个旗标间断开，**应为 2 簇**；现行实现已与参考算法一致，按用户建议改反而背离标准。已加回归用例把"RI 间夹 Extend 断开"锁死，防后人误"修"。**验证**：doctest **968 用例 / 192738 断言、0 失败**（util 61/61 含新增 format 极端值不空白 + text 超宽单簇不丢字 + grapheme RI+Extend 断开 3 例）；`tests_golden` 全过（format 改动未动任何快照） | 8 |
| 2026-10-17 | **ChartWidget 面积/折线几何统一 + 数据更新过渡动画**。①**面积与折线共用同一条 Catmull-Rom**：面积顶边此前用"数据点直线段插值"，与可见平滑折线几何不一致 → 中段翘离/漏光、两端段外 extrapolate 歪斜（用户截图 observed 的"左右不落底/不贴合轴 + 折线下漏光"）。新增 `smooth_y_at`（Fritsch-Carlson **单调三次**，端点复制切线防幽灵切线、x 越界钳首/末点得干净垂直边、陡峭数据不下凹/不过冲）；**折线与面积都采样这一个 `smooth_y_at`**（折线每 ~2px 取样连线，取代两套并行 Catmull-Rom）——从结构上保证两者几何绝对一致；面积列中心 `x+0.5` 采样 + 0.5px 上补偿，半透明顶边被 3px 不透明折线完全盖住、防亚像素漏光。②**数据更新平滑过渡**：`lerp_geom` 改为逐元素插值 xs/ly 数组、`geom_equal` 纳入 ly/xs，`set_data` 由"硬切"变为走同一条指数趋近（柱子长高、折线形变），与 resize 过渡复用同一机制、零新增状态。**验证**：doctest **953 用例 / 192680 断言、0 失败**（gui 30/30，新增"数据更新过渡"例）、1080p 快路径 raster ~30ms、`chart_demo --smoke` exit 0、全量零告警 | 8 |
| 2026-10-17 | **GUI 组合图全屏掉帧性能修复（数据驱动定位）**。`chart_demo` 加 `--bench W H N` 无头基准 + `ChartWidget` A(建Scene)/B(光栅)/C(合成) 插桩，二分定位掉帧元凶：①**dt 被钳**：主循环 `chart.tick(dt<0.05?dt:0.05)` 把推进量钉在 50ms，帧越慢每帧推进越少 → 指数趋近被拉长成"PPT 慢放"（改传**真实 dt**，帧率解耦恢复）；②**热循环跨 TU `at()/set()`**：`Bitmap::scaled_to`、`BitmapCanvas::blit`、Win32 上屏三处逐像素走带边界检查的跨 TU 访问器，1080p 每帧数百万次 → 单帧数百 ms（实测 res-cap 引入的 `scaled_to` 反而把快路径打到 400–660ms）。系统性修复：SceneRenderer 降采样 SS1 走 memcpy、`blit`/新增 `upscale_nearest`/Win32 `blit` 全改**裸指针 `data()`**、Win32 上屏改只读引用 + **复用 BGRA 缓冲**（去 `snapshot()` 整图拷贝与每帧 8MB 分配）；运动中按 `anim_downscale` 降分辨率光栅再放大（成本与窗口大小解耦）、静止才落全分辨率精美帧。demo 主循环**空闲才 sleep**（不人为限帧）+ `paint_all>33ms` 掉帧自诊断。实测 1920×1080 快路径 raster **~400–660ms → ~27–30ms**、pretty composite 163→14ms。**验证**：doctest **952 用例 / 192676 断言、0 失败**、gui 29/29、`chart_demo --smoke` exit 0、全量零告警 | 8 |
| 2026-10-17 | **GUI 平行栈 `termchart_gui` 交互件 + 零依赖 TUI 后端 + `api::quick` 一行出图**。①**常用控件**：新增 `widget_tree/label`/`button`/`slider`/`checkbox`/`progress_bar`（Label 文本、Button 按下态 + `on_click`、Slider 拖拽归一化 0..1 + `on_change`、Checkbox 切换 + `on_toggle`、ProgressBar 按 value 填充比例）；`BitmapCanvas::draw_text` 由点占位改为调用已落地的 `termchart::gui` 5×7 点阵字体（advance=6 / 格高=8），文本真正栅格化进 `image::Bitmap`。②**零依赖"真可见"后端** `backend_tk/tui_backend.hpp`：`bitmap_to_halfblock`（每 2 像素行合成一行 `▀`，上/下像素映射为前景/背景真彩 SGR，颜色经 `color::resolve` 按目标深度量化、保证与终端同色；不引入 `render/sgr`，用自包含 `38;2/48;2` 序列，守"不把终端 SGR 机制带进 GUI"边界）+ `TuiBackend`（`present` 光标归位后刷屏，支持连续帧覆盖）。接真实 Qt/Tk/Win32 仍需外部依赖 + 编译型平台代码，违背零强制依赖硬约束，按既定原则归为**可选插件**，核心只提供无头 + 终端两种参考后端。③**P0 一行出图** `api/quick`：新增 `PlotOpts` 与 `render_plot/plot`、`render_scatter/scatter`、`render_bar/bar`——`api::plot(x, y)` 一行见图，backend 选择 / 颜色降级 / 坐标轴 / 网格 / 标题全藏壳里，新手零配置、**无需注册 backend**；headless / 测试用 `render_*`（不写 stdout）。测试：`test_gui_stack.cpp` 补 7 例（Button 命中/回调/按下态、Slider 拖拽钳制、Label 点阵落像素、halfblock 含真彩 SGR、Button/Slider 像素级着色、Checkbox 切换+勾选像素、ProgressBar 比例填充）、`test_api.cpp` 补 3 例（plot 零配置 + 标题、scatter/bar、尺寸 0 报错）。示例：`57_gui_app`（PNG 端到端）、`61_gui_terminal`（TuiBackend 真彩刷终端）、`59_quick_plot`（一行出图三连）、`65_gui_interactive`（合成鼠标事件驱动 Slider→ProgressBar/Label、Button 计数、Checkbox 切可见性的交互闭环）。**验证**：doctest **949 用例 / 192651 断言、0 失败**；全量重建零告警（`-Wall -Wextra -Wconversion`）；示例 57/59/61 ctest 通过。编号说明：并行会话同期落地 `58_gui_hover`/`60_dense_sugiyama`，本栈 TuiBackend 示例由 58 顺延至 **61** 避免撞号 | 8、21 |
| 2026-10-17 | **GUI 平行栈原生 Win32 后端（可选插件 `plugins/gui_win32`）**。新增真实窗口后端 `termchart_gui_win32`（STATIC 库 + `win32_demo`）：离屏 `image::Bitmap` 作画布（复用 `BitmapCanvas`，控件树/布局/绘制**零改动**——只实现 `Backend`/`TkWindow` 接口）；`present()` 触发 `WM_PAINT`，用 GDI `StretchDIBits` 把位图（RGBA→BGRA 重排、32bpp 自上而下 DIB）贴进窗口 DC；OS 鼠标/键盘消息在窗口过程里经 `tk_event_map` 翻成 `Event` 入队，`poll_event` 非阻塞抽取（`PeekMessage` 全线程、含 `WM_QUIT`），`run_loop` 走 `GetMessage`。平台隔离：`<windows.h>`（`NOMINMAX`/`WIN32_LEAN_AND_MEAN`）完全关在 `.cpp`，公开头用 PIMPL 不泄漏任何 Win32 类型。接线：CMake 选项 `TERMCHART_BUILD_GUI_WIN32`（**默认 OFF**）+ 顶层 `if(... AND WIN32)` 门控；仅链系统 `user32/gdi32`，无第三方依赖；**OFF 时核心零依赖构建逐字节不变**（实测 `build.ninja` 无插件引用）。`--smoke` 模式无交互自关闭，实证 建窗→渲染→`WM_QUIT`→干净退出（exit 0）。GUI 后端矩阵成形：无头 `HeadlessBackend`（测试/PNG）、终端 `TuiBackend`（halfblock 真彩）、原生 `Win32Backend`（真窗口）；Qt/Tk 后续研究。**验证**：插件与 demo 构建零告警（平台胶水层用轻量告警集，规避 Win32 转换在 `-Wconversion` 下的必然刷屏）；默认 OFF 门控生效 | 8 |
| 2026-10-17 | **GUI 组合图表控件 + 悬停信息框 + 缩放自适应（`widget_tree/chart_widget`）**。复刻 Web 组合图（柱 + 折线 + 面积 + 网格/轴/图例/标题，深色主题）：新增 `ChartWidget`，把"数据 → Scene → `SceneRenderer` 光栅化缓存 → 贴上屏"封成控件。**补了一个缺失 API**：`TkCanvas::blit(Bitmap,x,y)`（组合图出口——控件用 Scene 拼好图、光栅化成 Bitmap 再合成上屏，抗锯齿白拿）；`BitmapCanvas` 用 alpha 合成实现，Tui/Win32 后端复用 `BitmapCanvas` 自动获得。面积填充因 `SceneRenderer` 不填任意多边形，改用终端 `AreaChart` 同款"逐列填充矩形"。**悬停**：MouseMove 命中测试（按类别 x 轴，近似 ECharts axisPointer）→ 重绘在数据点旁画信息框（类别/折线值/柱值，贴边翻向 + 夹回控件内）。**缩放自适应**：窗口 Resize → 父容器重排 → 控件按新 `global_bounds` 尺寸重建 Scene（缓存按尺寸失效）→ 图表随之变化。示例 `plugins/gui_win32/example/chart_demo`（原生窗口演示悬停 + 缩放；`--smoke` 实证渲染并干净退出 exit 0）。测试：`test_gui_stack.cpp` 补 `blit` alpha 合成 + `ChartWidget`（combo 渲染出柱色 / 悬停命中 / 信息框像素 / 缩放自适应重排）两例。**验证**：doctest **951 用例 / 192665 断言、0 失败**；chart_demo 构建零告警。**半透明面积（alpha 合成）**：`scene/command.hpp` 明令 Scene IR 不带渐变/alpha/图层（"属于 canvas L10"），故不给 `Color`/`FillOp` 加 alpha（违架构 + 牵动终端 SGR）；改为在**合成层**做——`TkCanvas::blit` 加 `opacity` 参数（`BitmapCanvas` 缩放源 alpha 后混合），ChartWidget 拆 底[网格/柱/标签]/面积/折线 三层光栅，面积层以 ~0.45 不透明度合成到柱子之上（柱子透出来）、折线层压最上，零改 Scene/Color。**视觉打磨 + 过渡动画**：Y 轴 nice 取整（0/100/200/300 而非 71/143/…）、面积退到底层并调暗（不再糊成一片/吃柱子）、折线加粗 + Catmull-Rom 平滑、tooltip 投影/左强调条/系列色块；**resize 过渡动画**——尺寸/数据变化时不硬切，对 plot 矩形做指数趋近平滑（`tick(dt)` 驱动，复用 `util::lerp/smoothstep`；ytop 只随数据变故插值集中在四边），Win32 离屏双缓冲平滑无闪烁、TUI 不 tick 硬切以规避终端每帧全刷闪烁 | 8 |
| 2026-10-16 | **Sugiyama 布局引擎补齐四步之间的缺口 + 密集图指标化回归**。①**跨层边插 dummy**（经典第 1.5 步，此前长边被当成"一跳邻居"参与 median，排序拿到的位置信息本身失真）：dummy 只活在布局内部的 `ext_layers`/`dummy_column`/`dummy_row`，**不进 `Graph`**（`node_count()`/`edges()` 对调用方一个都没变），邻居条件随之收紧为相邻层段。②**长边走 dummy 槽位的正交骨架**（横→竖→横穿槽位行→竖→横，竖直段落在列间通道列），新增 `RouteOptions::keep_existing_routes`（默认 false，不打扰其他调用方）让路由沿用而非覆盖。③**修端口侧选择**：原先按中心连线主导轴选侧，跨列边会被选成"从南侧出框"，第一段贴着自己框的边框走、被自己框否决而退化成斜线；改为按两框间隙选侧（间隙 < 2 格才回退）。④`RouteOptions::avoid_wires`（连线是否互避，默认 true；密集图关闭后由 `junction_char` 合成 ┼）。⑤**降交叉升级**：transpose 相邻交换 + best-so-far（median 本身会震荡）+ dummy sifting + **走线长度作 tie-break**（判优改为"交叉数优先、长度次之"，等价于"交叉不升"约束下的 λ 加权）。⑥**坐标分配**（第三步）：离散吸附——节点 y 只能从"刚好对齐某个邻居"的候选值里挑、逐层 DP 最大化直线边数，带 `LayoutOptions::align_slack` 膨胀上限（超限层回退紧堆）；默认 `align_rows = false`（实测对齐会把图高抬 41%，见下）。⑦**不等高节点**：`node_box_for` 支持多行标签（`\n` 分行，高 = max(min_height, 行数+2)），单行行为不变。⑧**`VectorOptions::font_size`**：修 SVG 文字被放大 `scale` 倍糊满全图（默认 0 = 旧行为，金标未动）。⑨**`examples/60_dense_sugiyama`**：全指标可打印（交叉/斜线段/长边穿框/总长度/拐点/bend_ratio/直线占比/高度分布）+ `--align` `--tall` `--cycle` `--svg` `--png` 对照开关。密集图实测（70 节点/98 边）：**交叉 687→37、斜线段 31→0、长边穿中间列框 >0→0**；不等高场景下对齐可把直线边占比 0.21→0.33 但图高 +41%，故默认关。测试：sugiyama 补 3 个用例（dummy 槽位不穿框、`keep_existing_routes`、7×5 确定性图回归上界）、layout 补多行标签 1 个 → **doctest 942 用例 / 192624 断言 0 失败**，护栏四件套全绿（333 头自洽 / 分层 / 生成物一致 / 359 头独立编译）。另：恢复工作区被删的 `SECURITY.md`（根配置十三件之一，内容自 `53680db`）。 | 4 |
| 2026-10-14 | **GUI 悬停出数据打通（`gui/hover_chart`）**。新增 `include/termchart/gui/hover_chart.hpp` + `src/gui/hover_chart.cpp`：把“图表登记 `chart::HitRegion` → 鼠标位置 `interaction::topmost_hit`（纯几何，与终端同一套契约）→ 命中 payload 查表取文本 → 浮层翻译成 `RectOp`/`TextOp` 叠进 Scene 副本 → `SceneRenderer` 重放”封成可复用 `HoverLayer`（`render_base`/`render_at`/`hit_label`）。渲染器**零改动**——悬停是“上游换 Scene”而非“渲染器加功能”；浮层刻意不共用终端 `interaction::Tooltip`（那是 cell 补丁语义，像素域每帧重放无需 patch/restore），只保留“贴边翻方向 + 夹进画布”。新增 `tests/unit/gui/test_gui_hover.cpp`（4 用例：命中文本、payload 0 不出浮层、悬停帧像素有差异、未命中逐像素等于底图、贴边不越界）与 `examples/58_gui_hover.cpp`（三系列折线图 + 36 个数据点命中圆；`58_gui_hover out.png X Y` 出悬停帧）。**验证**：`ctest` **69/69**、doctest **900 用例 / 192306 断言、0 失败**、`check_all` 全绿（355 头独立编译、单头已再生）。 | 4、16 |
| 2026-10-14 | **GUI 平行栈 `termchart_gui`（独立顶层库，与 `termchart` 并列）脚手架落地**。复用边界到 `scene`(L4)：渲染核心（`gui/font`+`gui/scene_renderer`）直接复用，其余 `layout/widget/interaction/app` 由平行栈自写，且**不碰** `cell/SGR/CellBuffer`（原子是像素矩形+真彩，颜色经 `color::resolve` 量化保证与终端一致）。新增 `include/termchart_gui/`（header-only 库 `termchart_gui`，链接 `termchart`）：`layout/`(constraint/flex/grid/layout)、`widget_tree/`(node/widget/container/mount/reconcile)、`native_loop/`(event/loop/dispatcher/invalidate/timer)、`backend_tk/`(backend/tk_window/tk_canvas/tk_event_map + 无头 `BitmapCanvas`/`HeadlessBackend`)、`render/`(style_map)，共 22 头。新增 `tests/unit/gui/test_gui_stack.cpp`（9 用例：flex/grid 几何、控件树全局坐标+命中、style_map 量化、无头画布绘制+快照、定时器周期、事件 post/poll、reconcile 差异）。新增 `examples/57_gui_app.cpp`（控件树→HeadlessBackend→Mount::paint_all→Bitmap 快照→encode_png 端到端，登记进 ctest）。CMake 加 `termchart_gui` INTERFACE 目标并接入测试链接。**验证**：doctest 全量 **896 用例 / 192293 断言、0 失败**（含新增 9 例）；`example_57_gui_app` ctest 通过；全库 `termchart_tests` 构建零告警。已知边界：`backend_tk` 仅无头参考实现，接真实 tk（Qt/Tk/Win32）只需替换 backend、平行栈其余不动；`draw_text` 暂用点占位，待接 `gui/font` 点阵 | 8 |
| 2026-10-14 | **GUI 平行栈渲染核心落地（`gui/` 层 22）**。新增 `gui/font`（95 字形 5×7 点阵等宽字体，UTF-8 + 显示宽度口径，缺字画占位框；零外部依赖、零系统调用）与 `gui/scene_renderer`（Scene → RGBA Bitmap：与终端/SVG 共用 `StatefulVisitor` 时序，几何走 3× 超采样盒式降采样抗锯齿，线宽随 autofit 缩放与 SVG 同构，文本下采样后按最终分辨率直绘并置顶，autofit 按 bounds 变换，ImageOp 画棋盘占位）。新增 `tests/unit/gui/`（字体度量/绘制/越界、像素级颜色归属、AA 过渡、autofit、透明背景、PNG 往返逐字节一致）与 `examples/55_gui_scene_png.cpp`（深色主题三系列折线图 → PNG，1200×675）。层表登记 `gui=22` 并加禁止边 `gui>canvas/term/app`（平行栈不得碰终端呈现）。**验证**：`ctest` **69/69**、doctest **900 用例 / 192306 断言、0 失败**、`check_all` 全绿（314 头独立编译、单头已再生）。已知边界：文本仅内置 ASCII 字形；hover 交互待 native_loop（终端侧管线已全，见 `examples/21`） | 4、10、16 |
| 2026-10-13 | **Scene 消费者时序收敛为一份（`scene::StatefulVisitor`）**。新增 `include/termchart/scene/stateful_visitor.hpp` + `src/scene/stateful_visitor.cpp`：把"MoveTo 不收口、LineTo 首个触发 begin_path、Fill/Stroke 先按旧状态收口再更新、Rect 按有无填充分两支"这套时序收在一处；`end_path()` **无条件**调用（判空交给实现的 dirty/空串守卫，加 `if (path_open_)` 会漏收口），`finish_path()` 收口后重置 `path_open_`。消费者改造：`canvas::ScenePainter` 与 `vector::SvgVisitor` 各自只剩"往哪画"的 8 个 hook（canvas 侧 `begin_path` 是 no-op，填充改为即时收口）；SVG 侧顺带修掉"非路径图元后紧跟 LineTo 会丢 M"的隐患。新增 `tests/unit/scene/test_stateful_visitor.cpp`（4 用例，用只记日志的派生类钉住时序）。**验证**：golden 一份未动（canvas 与 SVG 两条路径字节级不变，证实原行为与本时序等价）、`ctest` **69/69**、doctest **900 用例 / 192306 断言、0 失败**、`check_all` 全绿（`check_header_only` 333 头 / 282 实现）。未动：`BoundsVisitor`（无状态消费者，不需要时序）、`visitor.hpp/.cpp` | 4、10、16 |
| 2026-10-13 | **剩余小项清零（不含 GUI）**。工程设施：根配置 5 件（`.clang-format`/`.clang-tidy`/`.cmake-format.yaml`/`.editorconfig`/`.gitattributes`）+ `scripts/` 三件 shell 封装（各跑通），CI 补 `gen_fixtures`/`gen_single_header`/`check_headers` 三条护栏并新增 **tsan**/**coverage** 两个 ubuntu 作业。具名色：68 → **CSS 全集 148 色**（新增结构不变量用例）。夹具：`tools/gen_fixtures.py` 补齐 `sample_data.hpp`/`sample_graph.hpp`/`sample_grapheme.txt`，**7/7 就位**且都有消费方。P2：`api::render_chart`/`print_chart` 一行出图 + `quickstart_30s.md` 新段 + `mkdocs.yml` nav 补齐（此前整个 `docs/guide/` 不可达）。P3：Renderer 改 `unique_ptr`+移动、`at()` 断言 + `try_at()`、dsl 8 处 `const_cast` 由非 const 重载替掉、`StopCache` 改 append-only shared_ptr、`put`/`set_sides` 返回 bool、metro 步数断言。P4：路由新增 **Z 形绕行**（跨链边不再走直线兜底，新增严格断言回归）、`d2`/`mermaid_sequence` 超限不再造节点。P1（兼容式）：数据动词口径、`Opts`→`ApiOptions`（别名保留）、默认配色收敛为 `default_color()`（色号不变）、Funnel 选项封装、`Graph::find_edge`。sanitizer 确权结论：**本机 MinGW 无 libasan/libubsan/libtsan，只能在 Linux 跑**（已挂 CI）；coverage 本机可跑但需 `GCOV_PREFIX` 指 ASCII 目录。文档：`capability_matrix` 全文按 310/267 实测刷新、`ROADMAP` 现状节改写并标注其历史滞后、backlog/quality_todo 打勾。**最终验证**：`ctest` **69/69**（开 fuzz 73）、doctest **900 用例 / 192306 断言、0 失败** | 0–7 |
| 2026-10-13 | **示例表收口：37_export_latex 落地**（规格示例 01–54 全部齐备）。消费本轮落地的 latex_backend：片段（裸 tikzpicture）与 standalone（独立可编译文档）双模式对照。`ctest` **73/73 全绿**。规格示例缺口清零；剩余 backlog 全部为小项（具名色 148、根配置 5 件、shell 封装、sanitizer/coverage 确权、capability_matrix 数字刷新）与阶段 8 GUI 线 | 3 |
| 2026-10-13 | **backlog 轮收口（3 并行包 + 子文档勾选）**：`layout/kamada_kawai` 落地；obstacle 横挡绕障缺陷修复 + 回归测试；`latex_backend` + `png_screenshot` 落地（L16 收尾，TikZ 映射 / PNG 复用 image 编码器）；**examples 补齐 21 个**（23/24/25/26/27/31/39/40/41/43–54，规格表仅余 37_export_latex）；fuzz 条目实证修正（8 目标已齐）。**最终验证**：`ctest` **72/72 全绿**（示例占 51 项）、doctest **848 用例 / 191580 断言、0 失败**；护栏全绿（333 头 / 282 实现）。`docs/reference/backlog.md` 已按实证打勾；剩余：37 示例、具名色 148、根配置 5 件、shell 封装、sanitizer/coverage 全绿、capability_matrix 第五节数字刷新（310/267 实测口径） | 2–6 |
| 2026-10-13 | **阶段 7 落地：单头生成**。`tools/gen_single_header.py`（拓扑排序 = TermchartModules.cmake 层号 L0→L20、内部 include 剥除、系统 include 按名去重保序、`#pragma once` + `TERMCHART_SINGLE_HEADER` 卫哨、`--check`）；产物 `single_include/termchart.hpp`（自包含证明：产物拷出后**不含 include/ 与 src/ 路径**即可编译通过）；`examples/42_single_header_usage.cpp`（只 include 单头，链接 termchart 库，进 ctest）。接线：`generate_single_header` 进 `generate_all` 与 `check_generated`（纯头拼接，确定性）；示例 42 的目标单独挂 `single_include/` 路径。**最终验证**：`ctest` **51/51 全绿**、doctest 832 用例 / 191305 断言、0 失败；四护栏全绿。阶段 7 出口条件闭合——v0.x 规格的模块与横切全部落地 | 7 |
| 2026-10-13 | **阶段 6 落地（widget L18 / plugin L19 / app L20）+ 横切收尾，4 并行包**。widget：主头 + 三主题（header-only 常量表，已登记 header_only.txt）+ immediate_widget（无状态组件）+ reactive（响应式通知）；plugin：manifest/dynamic/loader/plugin 注册表（快照语义，供插件注册 Chart 子类）；app：animation/config/post（真跨线程队列）/loop（注入时钟 + max_delta 钳制）/headless（渲染不碰 stdout）。docs 全套 72 个 md（design/guide/charts×9/graph×8/reference/contrib——**只写磁盘真实 API，未做项如实标注**）；tools 新增 gen_gallery.py（跑示例收快照 + 索引）与 gen_api_docs.py（头文件 API 简表），均带 `--check`；根目录配置补齐 13 件（vcpkg/conan/SECURITY/CHANGELOG/CONTRIBUTING/Doxyfile/mkdocs/ABI_POLICY/ERROR_POLICY/AGENTS 等）。**最终验证**：`ctest` **50/50 全绿**、doctest **832 用例 / 191305 断言、0 失败**；四护栏全绿（`check_header_only` 307 头 / 264 实现）。阶段 6 出口条件就此闭合 | 6 |
| 2026-10-13 | **生成器接线收口**：`gen_api_docs.py`（纯头扫描）纳入 `check_generated`（CI 强制）；`gen_gallery.py` 纳入 `generate_all` 并以 `--build-dir ${CMAKE_BINARY_DIR}` 传参，但**刻意不进 check_generated**——它依赖已构建的示例二进制，在 CI 的"构建后"阶段单独跑（TermchartGenerate.cmake 有注释说明）。`_termchart_add_py_target` 顺带支持附加参数。验证：目标 `generate_api_docs`/`generate_gallery` 实跑一致（无 diff），`ctest` 50/50 保持全绿 | 7 |
| 2026-10-13 | **diagrams 已知红清零 + examples 13/18 落地**。5 处失败的判定：knowledge_graph 实现的分离步长不足（修实现）；sankey 的丝带 ░/标签/条高为实现缺口（修实现）；petri_net 与 chord 的标签落行为断言行位偏差（修测试）；uml_activity 的"显式 initial/final 优先"语义实现缺失（按头文件承诺修实现）。examples 新增 `13_sankey`（漏斗四段流）与 `18_uml_sequence`（三方两次往返 + Note）。**最终验证**：`ctest` **50/50 全绿**、doctest **794 用例 / 190985 断言、0 失败**、四护栏全绿。**已知红归零** | 2–4 |
| 2026-10-13 | **质量收口清单纳入子文档** `docs/reference/quality_todo.md`（体检 P0–P4：文档数字同步 / API一致性 / DX一行出图 / 现代C++毛刺 / 算法局限），索引见此 | 7 |
| 2026-10-13 | **「还没做」总览清单纳入子文档** `docs/reference/backlog.md`（功能/规格/工程/质量/文档同步/GUI 六类缺口），索引见此 | 0–8 |
| 2026-10-12 | **graph/diagrams 15 图种落地（3 并行包）**：uml_class / uml_sequence（本层模型，留 dsl AstSequence 映射注释）/ uml_usecase / uml_activity / er_diagram / knowledge_graph / social_network / petri_net / metro_map（组合 layout/metro）/ sankey（可组合 cartesian/streamgraph）/ chord / arc_diagram / adjacency_matrix / syntax_tree / circuit，共 30 文件 + 3 测试文件。全库编译通过（`check_header_only` **290 头 / 250 实现**）。**已知红（5 用例 / 9 断言，暂不接入 ctest，清单在 tests/CMakeLists.txt 注释）**：knowledge_graph 力导向盒子重叠 ×2；petri_net 变迁标签行位；sankey 丝带 ░/src/dst 标签/dst 条高 ×4；chord 标签 B 行位；uml_activity 显式 initial/final 时自动 ● 仍出现。**ctest 48/48 全绿**（doctest 757 用例 / 190809 断言，0 失败）。graph/diagrams 规格 **21/21 齐备** | 2–4 |
| 2026-10-12 | **metro 已知红清零（graph 6 缺口收口）**。根因与修法：①`layout_metro` 的 3 例期望未含实现的"step 抬升到最大框宽高"——测试改为压小框（5×2）使有效 step 恰为 opts.step；②`route_metro` 的真 bug：恰为 45° 对角时 `side_towards` 主选锚点被自身节点框堵死，直连/45°步进/L形全失败后落到"直线兜底"（不查障碍 → 穿框 + 非 45° 段）。修法是**锚点候选对**：主选优先、4 边全枚举（16 对），每对走完 直连→45°步进→L形 阶梯，全败才退直线。**最终验证**：`ctest` **42/42 全绿**、doctest **757 用例 / 190765 断言、0 失败**；四护栏全绿（275 头 / 235 实现）。graph 规格缺口（metro/cluster/bundling/obstacle/dag/dependency）就此闭合 | 2–4 |
| 2026-10-12 | **graph 6 缺口落地（并行包）**：`layout/metro`（链分解 + 45° 之字 + 栅格路由：直连→45°步进→L形→直线兜底）、`layout/cluster`、`route/bundling`、`route/obstacle`、`diagrams/dag`（含环检测）、`diagrams/dependency`，3 个测试文件 17 用例。**最终验证**：`ctest` 全绿（除 metro 3 用例）、doctest **756 用例 / 190874 断言、753 通过**；`check_header_only` 275 头 / 235 实现。**已知红（3 用例 / 23 断言，均在 metro）**：①测试期望用 opts.step=8，未考虑实现文档化的"step 抬升到最大框宽高"（测试需按有效 step 断言）；②metro 路由 45°/穿框检查未过，需跑实现定位（可能实现有真 bug）。这 3 例是下一切片的第一件事 | 2–4 |
| 2026-10-12 | **chart 全子目录铺开（68 文件）**：6 个并行包 + 主线收口。cartesian 23/23（补 16 图种）、polar 8/8、statistical 11/11、hierarchical 6+1、financial 7/7、flow 2/2、geo 4/4、text 4/4、indicator 5/5——规格 9 子目录全部齐备。断点恢复过程中修掉 2 处遗留编译错误（choropleth 的 Color/Style 误用、wordcloud 重复定义）、补齐 drawdown/yield_curve 两个缺失实现；statistical/financial 各缺的 7+3 个图种与 4 个缺失 cpp 由收尾包补齐。测试：chart 九个子目录的测试文件**全部接入**（polar/statistical/financial/hierarchical/flow/geo/text/indicator/cartesian×3，**ctest 42/42、doctest 739 用例 / 190594 断言全绿**；此前登记的 13 处"已知红"实为中断瞬间的中间态二进制，干净重跑即全绿）。横切：`.github/workflows/ci.yml` 新增（护栏三件套 + ctest 双平台必过）。 | 3 |
| 2026-10-12 | **阶段 5 收口：interaction 深化（5 个缺失组件）+ example 30**。新增 `highlight`（高亮放大：锚点居中、跨度 /magnify，复用 zoom 的锚点性质措辞）、`select`（框选状态机：begin/update/end/cancel，矩形归一化、payloads_inside 保持输入顺序）、`brush`（刷选：半径吸附、首次顺序去重）、`legend_toggle`（图例点击切换：id→可见性，payload 0 永不生效，click_at 复用 topmost_hit）、`immediate`（Event 流路由进 Viewport/BoxSelect 的最小粘合层）。新增 `examples/30_legend_toggle.cpp`（三帧脚本：全可见 → 点 sin 隐藏 → 恢复 sin 隐藏 cos，图例 `[x]` 标记与状态同源）。两个并行工作包（hl-select / brush-imm）+ 主线（legend_toggle + example 30 + 收口），共享文件仍只有 tests/CMakeLists.txt 与 examples/CMakeLists.txt 的追加行。**最终验证**：`ctest` **42/42 全绿**（doctest **601 用例 / 190214 断言**），四护栏全绿（`check_header_only` 205 头 / 165 实现）。阶段 5 出口条件（example 21·22·30·33 / 三个 dsl fuzz / dsl_registry 快照语义）**全部满足** | 5 |
| 2026-10-12 | **L17 `dsl` 开始落地（第一批 6/14 文件）**：`parser_base`（递归下降框架：peek/advance/match + 首个诊断定位）/ `lexer`（箭头感知切分、UTF-8 标签、注释前缀可配、非法字节不阻塞游标）/ `ast`（中立模型 + `to_graph` 唯一适配点）/ `dsl_registry`（function-local static + 快照语义 + 幂等内置登记）/ `mermaid`（首行指令识别与分发，未实现图种返回 `NotImplemented`）/ `mermaid_flow`（头部/方向、8 种节点形状、9 种箭头、两种边标签写法、边链、subgraph 归属）。配套：`tests/unit/dsl/test_dsl_mermaid.cpp`（15 用例 / 153 断言）、`tests/fixtures/sample_mermaid.txt`（生成器 `tools/gen_dsl_samples.py`，已接入 `generate_all` / `check_generated`）、`examples/33_mermaid_flowchart.cpp`（解析→Graph→复用 `render_flowchart`）。本轮在独立构建目录 `build/dsl-ninja` 验证：构建 96 目标零失败、`ctest` **32/32 全绿**（doctest 513 用例 / 189524 断言）、四护栏全绿。**未做（明确留给后续切片）**：`mermaid_sequence`/`mermaid_class`/`mermaid_state`、`graphviz_dot`、`plantuml`、`d2`、`table_markdown`、`csv_sniff`，以及 `fuzz_mermaid`/`fuzz_dsl_lexer`/`bench_mermaid_parse`、`sample_dot.txt`/`sample_plantuml.txt`/`sample.csv` | 5 |
| 2026-10-10 | 阶段 0 / 阶段 1 落地并跑通：护栏三件套全绿、ctest 7/7、doctest 84 用例 / 4154 断言 | 0、1 |
| 2026-10-11 | 风险登记册新增 R9（GLOB 重扫描瞬时失败），记录新增源文件后首次构建的假失败陷阱与对策 | 0–8 |
| 2026-10-11 | Windows 控制台 VT 修复：新增 `platform::enable_vt/disable_vt`（`enter()` 开 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`，`leave()` 按原 mode 恢复）；`caps` 用 `WT_SESSION` 识别真彩色宿主、删除「TERM 为空即无色」误判；风险登记册新增 R10（ANSI/编码陷阱）与 R11（图像尺寸须自适应终端）；新增示例 `03_function_plot`、`04_heatmap`（自适应终端尺寸），改写 `02_sine_wave` 为盲文高精细版，修正 examples 中指向缺失源文件的坏引用 | 0–8 |
| 2026-10-11 | **深度代码审计（L0–L21 真实落地度）**：逐模块 LOC 统计 + 桩标记扫描 + 崩溃桩扫描 + 四护栏重跑。结论：L0–L16 全部有真实实现（合计约 16.7k 行，生成代码 0 行混入源树：graph 1636 / term 1432 / backend 1134 / interaction 814 / util 1192 / core 1175 / render 721 / raster 689 / color 674 / chart 577 / protocol 492 / canvas 435 / layout 429 / scene 399 / scale 449 / document 271 / image 157 header-only / api 366）；L17 dsl / L18 widget / L19 plugin / L20 app 为零代码空洞（代码与构建脚本均无引用）；L21 api 有实现。护栏全绿（check_layers、check_header_only 135头/101实现、check_headers 135头、4 生成器 --check、ctest 全绿）。**关键发现**：审计当时 `canvas/scene_renderer.cpp` 的 Scene→Canvas 桥接尚用占位字符（`kStrokeChar=U'*'`、ImageOp 用 `kMediumShade`），即 R12 缺口。**该缺口已于后续提交收口**：描边改走 braille 2×4、填充改走 halfblock 1×2，golden `scene_raster/` 锁外观；`chart`/`graph` 示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text` + 显式字符，从未出占位符（实测 `*` 计数为 0）。桩标记 5 处（scene_renderer 仅剩 ImageOp、layout/internal.hpp、canvas.cpp、api/quick.cpp、core/error.cpp）均为文档化占位、非隐藏空函数；崩溃桩扫描无异常（仅注册表 return nullptr）。登记风险 R12（现已收口），并修正「现状」对 graph 的漏判，以及 ctest/doctest 计数（早期构建子集曾写 16/16 / 279、17/17 / 303 / 185209，均陈旧；2026-10-11 及本轮干净重跑 `ctest --preset ninja` 确权为 24/24 / 335 用例 / 185558 断言，与 CODEBUDDY 一致）。 | 0–8 |

---

## 一、项目现状

**一句话**（2026-10-21 复核，覆盖 2026-10-13 的旧口径）：规格完备；**L0–L22 全部 27 个模块都有真实实现**
——阶段 5–7 已把曾经的零代码空洞 `dsl` / `widget` / `plugin` / `app` 全部填上；10-18 之后又新增
`math3d`(L3) / `chart3d`(L12) / `network`(L7) 三个模块（实测 **357 头 / 304 实现**）。
3D 散点/曲面、PDF 导出、数据处理层（聚合/重采样）、BMP/JPEG/GIF/WebP 解码、自主 TTF 解析、
Python 绑定均已落地；剩余缺口见 `docs/reference/backlog.md`。
阶段 4 图可视化已接通：`graph` 根 + 布局（网格/分层/根树/力导向/**径向/圆周/metro/cluster/kamada_kawai**）
+ 路由（直线/正交/**样条/弧线/捆绑/障碍避让**）+ 形状八种 + 21 个图种，示例 13–20 跑通。
**跨层缝 R12 已收口**：`canvas/scene_renderer.cpp` 描边走 braille 2×4、填充走 halfblock 1×2，
`Scene → Canvas` 出真图，外观由 golden `scene_raster/basic.txt` 锁住。

> ⚠️ 读进度的正确姿势：**本小节（「一、项目现状」）历史上多次落后于真实状态**
> （本行往上那段"L17–L20 是零代码空洞"就是 2026-10-11 的旧文，已于 2026-10-13 改写）。
> 唯一可信的进度来源是下方 **「修订记录」表的最新一行** + 各阶段的勾选清单；
> 数字口径以 `docs/reference/capability_matrix.md` 开篇为准。

- 已完成：`结构.md` 已定义 22 个模块（L0–L21）、三条渲染路径、三物种后端、注册表语义、完整测试/文档/工具链。
- 已落地模块真实代码量（include/ + src/ 的 cpp+hpp，排除生成物 `detail/`，单位行）：`graph` 2467 · `term` 1480 · `backend` 1134 · `util` 1192 · `core` 1175 · `interaction` 814 · `render` 721 · `raster` 689 · `color` 674 · `chart` 577 · `protocol` 537 · `layout` 467 · `scale` 449 · `canvas` 614 · `scene` 399 · `api` 366 · `document` 271 · `image` 157（header-only）。**生成代码 0 行**：**五个**生成器产物（unicode / grapheme / braille / **turbo** / module_graph）落在 `detail/` 不参与此统计，未灌入主源树（`detail/` 合计 2209 行，含 `ansi_tables`/`sixel_table`/`sort_network` 三份手写表）；`assets/colormaps/turbo.dat` 是数据而非代码。
- 桩标记扫描（5 处，均为文档化占位，非隐藏空函数）：`scene_renderer.cpp`（**仅剩 ImageOp**——Scene 里没有像素数据）、`layout/internal.hpp`（占位级线/网格/刻度字符）、`canvas.cpp` / `api/quick.cpp`（宽字符续接占位，属正常渲染逻辑）、`core/error.cpp`（"not implemented yet" 为不支持操作的报错文案）。**R12 收口后 `kStrokeChar = U'*'` 已从代码中删除**，描边改走真 braille 子像素。**已澄清一处误报**：`graph/wiregrid.cpp` 是**真的占用栅格**（被 `route_edges` 消费、有原子占用语义与测试），此前的"占位"是关键词误读。崩溃桩扫描（`std::terminate` / `abort` / `assert(false)`）**无异常**，仅注册表查不到时 `return nullptr`，属正常 not-found 返回。
- 验证状态（2026-10-13 干净重跑，独立构建目录 `build/init-ninja`）：`check_layers.py` 通过；
  `check_header_only.py` 通过（**333 头 / 282 实现**）；**七个**生成器 `--check` 一致
  （结构.md / module_matrix / unicode / grapheme / braille / turbo / dsl_samples / **fixtures** /
  **single_header** 无 diff）；**`ctest` 65/65 全绿**（开 fuzz 预设为 73/73）；
  **doctest 855 用例 / 192155 断言全绿**；golden 9 份；fuzz 8 个目标 `--self-test` 全绿。
  当前真实能力、示例索引与规格差距总览见手写文档 `docs/reference/capability_matrix.md`。
  历史数字（10/10、16/16、…、31/31、50/50）均为历史快照，不再引用。
  **平台现实**：本机 MinGW-w64 GCC 15.2 不带 `libasan`/`libubsan`/`libtsan`，
  ASan/UBSan/TSan **只能在 Linux 上跑**（已挂 CI）；coverage 本机可跑，
  但需 `GCOV_PREFIX` 指向 ASCII 目录（详见 `capability_matrix.md` 第五节）。
- **R12 已收口（跨层缝，本轮）**：`src/canvas/scene_renderer.cpp` 原先把 `LineTo`/`ArcOp`/`RectOp` 轮廓画成 `kStrokeChar = U'*'`、`ImageOp` 填 `kMediumShade`。现在描边累积进 **braille 2×4** 子像素场、填充累积进 **halfblock 1×2** 子像素场，按当前 `StrokeOp`/`FillOp` 的颜色光栅化后叠加（只覆盖非空格，因此多份 Scene 可叠画同一 Canvas）；`Fill`/`Stroke` 的"作用于其后图元"语义与 `backend/vector` 的 SVG 后端正向同构。收口时抓到一个真 bug：**盲文的空白是 `U+2800` 而不是空格**，按空格判空会让每次收口用空白铺满整张画布、把先前画好的内容擦掉（实测折线被后续圆弧的收口抹去），已修并有 `canvas: 多次收口互不擦除` 回归测试。
  **同时澄清一处误判**：`chart`/`graph` 的示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text` + 显式字符，**从未出占位符**（实测 `*` 计数为 0）；R12 的真实影响面是"同一份 `scene::Scene` 在终端侧能否出真图"，现在能了。
- 未落地（2026-10-13 重列）：**`termchart_gui`**（GUI 平行栈，25 文件，`结构.md` 标为平行项目、
  刻意未开工）；`graph` 的 `sugiyama` 独立入口（分层部分已被 `layout/layered` 覆盖）与
  `18_uml_sequence` 示例（等序列图渲染器）；各图种的多系列/堆叠变体。
  **规格差距量化**（代码清点，详见 capability_matrix 第五节）：`include/` 现有 **314 头**，
  按 `结构.md` 具名文件表核对**尚缺约 26–28 个**；`tests/fixtures/` **7 份夹具已全部补齐**
  （4 份来自 `gen_dsl_samples.py`、3 份来自新增的 `gen_fixtures.py`）。
- **已落地但曾在本清单里**（2026-10-13 核销）：`dsl`（14/14 文件 + 4 方言 + fuzz/bench）、
  `widget`（6 头 / 3 实现）、`plugin`（4 头 / 4 实现）、`app`（7 头 / 7 实现）、
  `graph/layout/kamada_kawai`、`graph/route/obstacle`、`graph/diagrams/dag`+`dependency`、
  `interaction` 的 `highlight`/`select`/`brush`/`legend_toggle`/`immediate`、
  `latex_backend` / `png_screenshot` / `37_export_latex`、`chart` 九个子目录 68 文件、
  `single_include/` 单头分发。
- 权威来源分工不变：`结构.md` 回答"做成什么样"，本文件回答"什么时候做、做到哪"，`CODEBUDDY.md` 是给 AI 的架构摘要。

---

## 二、推进原则

| # | 原则 | 具体含义 |
| --- | --- | --- |
| P1 | **护栏先行** | 分层校验、生成物校验、头文件自洽校验必须先于库代码落地。一旦铺开上千个 `#include`，回头补规则等于重写。 |
| P2 | **垂直切片** | 每个版本打通一条端到端最小链路（能出图 / 能导出 / 有 golden），不横向铺开。 |
| P3 | **四件套同步** | 新增一个头文件 = CMake 引用 + `src/` 同名 `.cpp` + 单元测试 + 头文件自洽校验，同时完成。 |
| P4 | **契约前置** | 可扩展性契约（三个 `replace_*` 实验）在对应第一个实现出现时就建立，不放到最后补。 |
| P5 | **边界即承诺** | 复用边界（`scene(L2)` 及以下）、唯一真源（cmake 规则源）、唯一生成点（`render/sgr.hpp`）一旦确定，不在后续阶段放宽。 |

---

## 三、阶段总览

| 阶段 | 切片目标 | 核心交付 | 出口条件（机器可验证） |
| --- | --- | --- | --- |
| **0** | 地基与护栏 | 规则源、三校验脚本、仓库骨架、构建预设、doctest | 假违规被拦；生成物与规格一致 |
| **1** | v0.1 终端上色闭环 | core / util / color / term / 最小 render / api | example 01 跑通；终端恢复测试过 |
| **2** | v0.2 光栅与场景 | raster / scene / canvas / scale 起步 / detail 数据表 | example 02 跑通；utf8·grapheme fuzz 点亮 |
| **3** | v0.3 图表与导出 | layout / chart 起步 / document / backend 三物种 / interaction 起步 | example 03·04·34·35·36；三个 `replace_*` 过 |
| **4** | v0.4 图可视化 | graph 根 / layout / route / shape / diagrams 起步 | example 14–17；sugiyama·force bench |
| **5** | v0.5 DSL 与交互 | dsl 全套 / interaction 深化 | example 21·22·30·33；mermaid fuzz |
| **6** | v0.6 图像·协议·组件 | image / protocol / widget / app / plugin | example 24–27·38·40·41；并发测试 |
| **7** | v0.7 全谱与发布 | chart 剩余子目录 / diagrams 补齐 / 单头 / 文档 | 覆盖率·sanitizer·fuzz 全绿 |
| **8** | GUI 平行栈 | `termchart_gui` 全套 | 复用边界不越界 |

---

## 四、阶段详情

### 阶段 0 — 地基与护栏（最高优先级）

**目标**：在写第一行业务代码前，让所有机器强制规则可运行、可自测。

#### 任务清单

- [x] `cmake/TermchartModules.cmake`：`TERMCHART_MODULE_LAYERS`（22 项）+ `TERMCHART_NARROW_EDGES`（1 项）+ `TERMCHART_FORBIDDEN_EDGES`（3 项）
- [x] `tools/check_layers.py`：判定顺序 禁止边 → 收窄边 → 层号；`detail/` 返回 -1；顶层工具头返回 0；伞形头豁免
- [x] `tools/gen_module_graph.py`：读 cmake 生成附一 + `docs/reference/module_matrix.md`（现已支持 `--check`）
- [x] `cmake/header_only.txt`：12 个纯头清单
- [x] `cmake/TermchartGenerate.cmake` / `TermchartSingleHeader.cmake` / `FindTermchartDeps.cmake` / `CompilerWarnings.cmake` / `Sanitizers.cmake` / `TermchartOptions.cmake` / `TermchartAbiVersion.cmake` / `TermchartConfig.cmake.in`
- [x] `scripts/check_headers.sh` / `check_generated.sh` / `check_license.sh` —— 2026-10-13 补齐并各跑通；`check_generated.sh` 的清单与 cmake 的 `check_generated` 目标逐一对应（新增生成器时两处都要加，避免"cmake 侧红、shell 侧绿"的假绿）
- [x] `.clang-format` / `.clang-tidy` / `.cmake-format.yaml` / `.editorconfig` / `.gitattributes`（2026-10-13 补）；`.gitignore` 早已有，本轮补 `site/`、补丁残留、编辑器本地覆盖三类。**定位：编辑器与增量改动用，不进 CI 强制**（310 头全量格式化会产出上万行无关 diff，理由写在各文件头）
- [x] `.github/workflows/ci.yml`（2026-10-13 核实确已建：guardrails / linux / windows 三作业 + asan；本轮补 `gen_fixtures` / `gen_single_header` / `check_headers` 三条护栏，并新增 **tsan** 与 **coverage** 两个 ubuntu 作业）
- [x] `CMakeLists.txt` + `CMakePresets.json`（现八套：debug/release/relwithdebinfo/asan/tsan/coverage + ninja/ninja-release + debug-windows/release-windows/relwithdebinfo-windows）
- [x] `third_party/doctest/doctest.h` vendored + `tests/main.cpp` 空测试跑通（现已 84 用例）
- [x] `detail/` 数据表生成器 + 产物：`gen_unicode_tables.py`（`unicode_tables` / `box_chars` / `block_chars`）/ `gen_grapheme_tables.py` / `gen_braille_table.py`；`ansi_tables` / `sixel_table` / `sort_network` 为手写常量表（头部注释已说明，不由生成器产出）

#### 出口条件

- [x] 故意放一条违规 `#include` → `check_layers.py` 退出码 1；清理后退出码 0（已实测）
- [x] `gen_module_graph.py` 产物与 `结构.md` 附一 / 附四、`docs/reference/module_matrix.md` 一致（`--check` 幂等）
- [ ] `check_headers.sh` 对 `detail/` 下每个头独立编译通过
- [x] 测试在 debug preset 下 ctest 通过（7/7）

> **为什么最先**：仓库尚未 populate 时脚本扫不到东西，所以这是唯一能用"假输入"做 TDD 的阶段。`detail/` 数据表层号为 -1、被大量模块依赖，是全局地基，必须此刻生成。

---

### 阶段 1 — v0.1 终端上色闭环

**目标**：打通最短端到端链路，把最关键的三个接口一次定对。

```
core → util/color → term(RAII) → render(renderer/sgr/diff) → api/quick
```

#### 任务清单

- [x] `core/`：`rgb` `color_depth` `attr` `style` `cell` `grapheme` `buffer` `point` `rect` `span_utils` `error` `expected` `arena`
- [x] `util/`：`utf8` `grapheme` `wcwidth` `text` `bits` `math` `assert` `logging` `format` `geom`（点线距离/线段关系/多边形/凸包）`timer`（单调时钟/秒表/帧计时/超时）`scheduler`（单线程定时任务，注入时间源）`task`（可取消句柄）`base64`（**规格清单外**的补充：OSC 52 剪贴板与 kitty/iterm2 协议共用，term 不能反向依赖 protocol，故唯一实现下沉到 L1）
- [x] `color/`：`palette` `quantize` `named_colors` `gradient` `colormap`（viridis/inferno/magma/plasma 用公开多项式拟合；**turbo 改用官方 256 级色表**，见 `assets/colormaps/turbo.dat` → `tools/gen_turbo_table.py` → `detail/turbo_table.hpp`）`resolve`（Color/Style → 真正 RGB：None 走默认色、按 ColorDepth 先量化再取色，**GUI 与位图输出的分界点**）
  > Turbo 的历史实现踩过两次坑：先是多项式系数坏掉（输出恒为蓝/青/紫），改成锚点插值后又只是"形状像"。Turbo 本身没有解析式定义，Google 发布的就是一张 256 级查找表——所以现在的口径是"**只查表，不近似**"，级点处与官方表逐字节一致（采样时的浮点吸附见 `turbo_sample` 注释）。
- [x] `term/`：`terminal` `caps` `raw_mode` `size` `ansi` `daemon_safe` `signal` `waker` `mouse_mode`（模式与坐标编码两组独立开关 + 切换前清场）`clipboard`（OSC 52 读写与回包解析）`hyperlink`（OSC 8，参数位与 URI 位分别转义）`capability_query`（DA1/DA2 序列与回复解析）`color_fallback`（TerminalCaps → ColorDepth 桥接，算法唯一实现在 color/quantize）
- [x] `render/`：`renderer` `sgr` `diff` `damage`
- [x] `api/` 起步：`quick`（含 `std::call_once` 的 `init_core()`）`opts` `print`
- [x] `version.hpp` `export.hpp` `deprecate.hpp` `fwd.hpp` `termchart.hpp`
- [x] `examples/01_hello_color.cpp` `examples/32_quick_one_liner.cpp`（两者都注册进 ctest）
- [x] `tests/unit/core/*` `util/*` `color/*` `render/test_render.cpp` `render/test_sgr.cpp` `render/test_diff_damage.cpp` `core/test_buffer.cpp` `scale/*` `canvas/*` `scene/*` `api/*` `term/*` `test_contracts.cpp`（确定性 UTF-8/grapheme/宽度 fuzz 含于内）
- [x] `tests/integration/test_terminal_restore.cpp` `test_full_render.cpp`
- [x] `tests/golden/ansi_props/` 框架建立（**解析成属性再比对，不直接比字节**）；快照由 `python tools/gen_golden.py` 生成，已产出 5 份

#### 关键约束（本阶段一旦定错，后续全错）

- `core/cell.hpp` 只存**单个主码点** `char32_t`；组合序列结构放 `core/grapheme.hpp`（只定义不分割）；**分割在 `util/grapheme.hpp`**。
- `render/sgr.hpp` 是**唯一 SGR 序列生成点**——所有 snapshot 后端将来都复用它。接口要一次定对。
- `term/terminal.hpp` 是 RAII 门面，异常退出必须恢复终端。

#### 出口条件

- [x] example 01 上色并正常退出
- [x] `test_terminal_restore` 通过
- [x] `ansi_props` golden 首次生成并稳定
- [x] `check_headers.sh` 对阶段 1 全部头通过

---

### 阶段 2 — v0.2 光栅与场景

**目标**：打通 `Scene → scene_renderer → Canvas → to_buffer() → CellBuffer → renderer`。

#### 任务清单

- [x] `raster/`：`rasterizer` 基类 + `braille` `halfblock` `quadrant` `ascii` `block` + `registry` + `auto_select`（六分块 `sextant` 用 U+1FBxx 新区块，暂未实现，见下注）
- [x] `scene/`：`scene` `command` `builder` `visitor`（矢量 IR 本体；`command.hpp` 是 header-only）+ `bounds`（几何包围盒；GUI 侧 `scene_renderer` 要先知道 Scene 多大才能定画布/无效区，故放在 scene 而非 canvas）
- [x] `canvas/`：`canvas` `primitives` `scene_renderer` + **`fill`（扫描线填充，横向精确覆盖 + 纵向超采样）**、**`path`（曲线自适应细分展平，四段贝塞尔圆）**、**`layer`（Overlay/Replace/Tint 三种合成 + 最小重绘包围盒）**、**`transform`（2D 仿射，含字符宽高比补偿）**、**`clip`（矩形 + 凸多边形；非凸只支持逐点判定并在接口上报 false）**
- [x] `scale/` 起步：`scale` `linear` `log` `ticks` `nice` `auto_domain`（纯函数部分；`category` / `band` / `registry` 待补）
- [x] `image/bitmap.hpp`（`Bitmap`：RGBA 像素缓冲，宽/高/通道/行步长）+ `image/field.hpp`（`FloatField`：标量强度场，`raster/` 的输入）；两者都是 header-only。拆开是因为"图片像素"与"标量场"是两种东西，不该挤在一个 `bitmap.hpp` 里
- [x] `examples/02_sine_wave.cpp`
- [x] `tests/unit/raster/*` `image/*` `canvas/*` `scene/*` `scale/*`
- [x] `layout/`：`box` `axis` `grid` `legend` `title` `label`（在 canvas 之上做轴/网格/图例/标题，依赖 scale，不碰 render）
- [x] `chart/`：`chart` `series` `registry` `cartesian/line_chart` `cartesian/bar_chart`（端到端折线/柱状图，不碰 render）
- [x] `examples/03_line_chart.cpp` `04_bar_chart.cpp`
- [x] `tests/unit/layout/*` `chart/*`
- [ ] `tests/fuzz/fuzz_utf8.cpp` `fuzz_grapheme.cpp`
- [x] `bench/` 骨架建立：`bench/bench.hpp`（零依赖计时骨架，中位数取样 + `--quick` 冒烟档）、`bench/CMakeLists.txt`（`TERMCHART_BUILD_BENCH` 开关，`--quick` 进 ctest）；本阶段三条（`bench_raster` `bench_utf8` `bench_scene`）待补

> 注：`sextant`（2×3 六分块）映射到 Unicode "Symbols for Legacy Computing" 区块 U+1FB00+，属于较新范围，终端支持度低且与 blind/quadrant 重叠；当前阶段先交盲文/象限，sextant 留待阶段 7 图元数据密度需求再补。若补，需先扩展 `detail/` 六分块表。

#### 关键约束

- `Scene` 是**非完备图形 IR**，只表达 vector 后端能消费的图元。**不要往 scene 里塞 canvas 才有的绘制能力。**
- `canvas/scene_renderer.hpp` 是**终端路径上 Scene→Canvas 的唯一桥接点**。
- `detail/` 数据表（盲文/块字符/制表符）已在阶段 0 就绪，此处直接消费。

#### 出口条件

- [x] example 02（正弦波，ASCII 线/填充占位；盲文/半块等子像素光栅器已实现，可在 canvas 上叠加）跑通
- [ ] `bench_raster` 有基线数值写入 `baseline.json`
- [ ] utf8 / grapheme 两个 fuzz 目标可运行（fuzz 性质已以确定性循环形式落地在 `test_contracts.cpp`，独立 fuzz 目标待补）

---

### 阶段 3 — v0.3 图表与导出（架构契约验证点）

**目标**：三条渲染路径、三物种后端、注册表快照语义在此第一次交汇。**这是风险最高的阶段。**

#### 任务清单

- [x] `layout/`：`layout` `box` `axis` `grid` `legend` `title` `label` + `facet`（纯几何矩形划分，不认识 chart）`auto_margin`（由标签宽度/标题/图例反推留白）`label_avoid`（通用避让；graph 侧转引同一实现，规则只有一份）`annotation`（参考线/区域高亮/标注）。**规格 11 个文件全部落地**
  > `annotation` 有两条硬规则：**域外的值不画**（比例尺默认 clamp，硬画会得到一条"看着像真值"的边界线）；**域内的值要夹进 `[inner.y, inner.bottom()-1]`**（比例尺把域端点映射到半开边界上，不夹会整条消失）。区域高亮默认只改背景色、保留数据字符。
- [x] `chart/` 根：`chart` `registry` `series` `hit_region` + `context`（绘制开关与样式覆盖，默认全开）`dataset`（系列集合 + 范围查询）
- [x] `chart/cartesian/`：`line_chart` `bar_chart` `scatter_chart`（含气泡四档标记）+ **`area`（面积图：逐列填充到基线、baseline 纳入定义域、先填后描线）** + **`step`（阶梯图：`Pre`/`Post`/`Mid` 三档）** + **`hist`（直方图：Sturges 自动分箱、`set_bins` 可覆盖并 clamp 到 1..64、`bin_edges`/`bin_counts` 对外可查）** + **`heatmap`（热力图：二维点集→网格计数→`colormap` 真上色，色表可选）** + `sparkline`（迷你线，非 Chart 子类）。规格其余图种（15 个）待补；`area` 的堆叠/百分比/流图未做
- [x] `document/`：`document` `block` `span` `table_model`（structured 物种的输入 IR）
- [x] `backend/` 起步：根 `backend` + `snapshot/`（`backend` `ansi_backend` `text_backend` `html_pre_backend` `json_backend`，`ansi_backend` 复用唯一收窄边 `> render/sgr`）+ `vector/`（`backend` `svg_entities` `svg_backend`）+ `structured/`（`backend` `html_backend` `markdown_backend`）。三物种各至少一个后端；`png_screenshot` 待补
- [x] `tests/unit/document/*` `backend/*`
- [x] `interaction/`：`event` `key` `mouse` `input` `input_parser`（ASCII/UTF-8/CSI/SS3/SGR 鼠标/OSC/Alt/Ctrl，含跨包不完整序列与 **256 KiB 缓冲上限**）`focus` `hit_test`（只碰 `HitRegion` 几何，**无图表名**）`dispatcher` + **`zoom`（`Viewport`：锚点缩放 + 跨度/原点双向夹取，夹取时贴边优先）**、**`pan`（`PanGesture`：纯几何拖拽状态机，按"内容跟随指针"折算）**、**`crosshair`（准线 + 轴侧读数；默认只改样式保留数据字符）**、**`tooltip`（边框浮层 + 自动翻边 + 宽字符 2 列 + 超长截断）**
  > `crosshair`/`tooltip` 的唯一核心不变量是 **`clear()` 逐格逐属性精确还原**：覆盖层记下被改写的 `(格, 样式)`，撤销时放回去 —— 否则"悬停一次"就永久污染画布。两者都只在绘图区内绘制、退化输入（空文本/锚点在画布外/区域仅 1 格）**明确不画**而不是硬塞。
- [x] `scale/registry.hpp`（`ScaleRegistry`：function-local static 单例、工厂登记、内置 `linear`/`log`、`names()` 去重有序；与 chart/raster 同一套契约）
- [x] `examples/03_line_chart.cpp` `04_bar_chart.cpp` `34_export_svg.cpp` `35_export_html.cpp` `36_export_json.cpp`（`37_export_latex.cpp` 待补）
- [x] **`tests/integration/replace_backend_html_pre.cpp`（实验 A）**——实现 `SnapshotBackend` 接口即接入，核心零改动
- [x] **`tests/integration/replace_chart_radial.cpp`（实验 B）**——实现 `Chart` + `ChartRegistry::register_chart`，核心零改动
- [x] **`tests/integration/replace_raster_custom.cpp`（实验 C）**——实现 `Rasterizer` + 新增的 `RasterRegistry::register_raster`，核心零改动
  > 实验 C 的发现：`RasterRegistry` 最初是**封闭枚举 + 硬编码 switch**，不可扩展——已补 `register_raster(name, creator)` / `supports_name` 与 `ChartRegistry` 对齐。此外 `RasterKind` 仍是封闭枚举，自定义光栅器的 `kind()` 需借用现有值（若将来需要可改为可选 name）。文件暂名 `replace_raster_custom`：六分块 `sextant` 的 60 格局字形表（U+1FB00+）易错，待有把握时再补。
- [x] `tests/golden/test_golden_exports.cpp`（text / svg / json 三类快照）
- [ ] `tests/integration/test_roundtrip_scene.cpp` `test_headless_pipeline.cpp`
- [ ] `bench/bench_render.cpp` `bench_diff.cpp` `bench_svg_backend.cpp`

#### 关键约束（禁止边在此显形）

- **`backend > chart` 禁止**：backend 走 Document，绝不碰 chart 类型。
- **`chart > render` 禁止**：chart 不碰渲染管线，保住 `to_document()` 通路。
- **`render > interaction` 禁止**：维持单向。
- **唯一收窄边 `backend/snapshot/ansi_backend > render/sgr`**：只有 `ansi_backend.hpp` 能 include `sgr.hpp`，其余文件对全部关闭。**PR 必须写明为何不能放宽到模块级。**
- 写后端前先回答："输入是 CellBuffer、Scene 还是 Document？"——**后端不做跨渲染模型转换。**
- `interaction/hit_test.hpp` 只 switch `HitRegion::Shape`，**不得出现任何图表名**。
- `layout/facet.hpp` 只做纯几何划分，禁止创建/绘制 chart。
- `App` 构造时对全部注册表做浅拷贝快照并冻结；**运行期注册 = UB**。

#### 出口条件

- [x] 三个 `replace_*` 实验全部通过（新增 backend / chart / raster，**核心零改动**）
- [x] example 03·04 跑通；34（SVG）35（HTML）36（JSON）跑通（37 LaTeX 待补）
- [x] text / svg / json 三类 golden 建立（`tests/golden/{snapshot_text,snapshot_json,vector_svg}/basic.txt`）
- [x] `check_generated` 稳定（单头、module_matrix 无 diff）——由 `check_all` 覆盖

> **不要把这些实验留到阶段 7**。它们是可扩展性契约，越早建立越能约束后续所有设计。

---

### 阶段 4 — v0.4 图可视化

#### 任务清单

- [x] `graph/` 根：`graph`（节点/有向边容器 + 邻接/度数/并集包围盒）`node` `edge` `attributes`（字符串键值袋）`wiregrid`（原子占用，避免压线）`arrow`（头部字符与占格）`port`（锚点/对侧/朝向）`label_place`（右/左/上/下避让）`crossing`（真交叉计数）
- [x] `graph/layout/`：`layout`（选项 + 节点尺寸）`grid`（网格）`layered`（分层，含"整图成环时按入度最小者破环、回边不抬层号"）`tree_layout`（根树，支持左右分列）`force`（力导向，确定性圆环初始 + 降温收敛）+ **`radial`（径向树：父按子树扇区均分、半径按层递增、极坐标转格带字符宽高比压缩）**、**`circular`（圆周：按外接圆判据推半径，保证相邻框不重叠）**。`sugiyama`（本仓用 `layered` 覆盖其分层部分）`kamada_kawai` 待补
- [x] `graph/route/`：`route`（同侧端口均匀扇出 + 锚点吸附整数格 + 节点框登记为障碍）`straight` `orthogonal`（Z 形 → L 形 → 直线兜底，方向按节点框相对位置判定）+ **`spline`（Catmull-Rom + 弦长自适应采样 + 逐格去重 + 8 连通补齐；压框即回退正交——宁可折线也不穿框）**、**`arc`（三次贝塞尔隆起，控制点沿弦外推保证先离框；自环走框外 C 形环，折角二次贝塞尔抹圆）**
  > **交汇口字符已抽成公共件** `graph/route/junction.hpp`（header-only `constexpr`，登记在 `cmake/header_only.txt`）：规则原先在 `orgchart.cpp` 与 `17_uml_class` 示例里各有一份，地铁线路图会是第三个消费者，故在第三个出现前收口。它是"按上/下/左/右四个方向选 `──│┌┐└┘├┤┬┴┼`"的纯数据表判据，并有表驱动测试穷举 16 种组合 + 拓扑一致性检查（防 `┌/┐`、`├/┤` 写反）。
  > 两条新路由的真实签名是 `std::size_t route_spline(Graph&, const RouteOptions& = {}, WireGrid* = nullptr)`（`arc` 同）—— 与既有 `route_edges` 一致，返回写入折线的边数。`grid == nullptr` 时**自建**障碍栅格：不穿框是硬约定，不能因为调用方没给栅格就放弃避让（与 `route_edges` 行为不同，已在头注释写明）。`spline` **不处理自环**（自环两侧锚点重合会退化），自环请用 `arc`。`obstacle` 路由待补
- [x] `graph/shape/`：`shape`（轮廓 + 绘制，单一 `build_shape` 分派，绘制与轮廓不再各写一份 switch）`box` `rounded` `diamond` `ellipse`（逐行半宽取椭圆方程）`stadium` `hexagon`（六顶点 + 格粒度连线）`cylinder`（顶盖/侧边/底弧）`icon`（无边框，内容由调用方提供）；放不下时优雅退化为矩形/圆角
- [x] `graph/diagrams/`：`flowchart` `state_machine`（转移事件标签 + 起止标记，标签放不下时**放弃绘制**而不是涂掉节点）`mindmap`（树布局 + 扇形连线）**`orgchart`（层级树 + 共享树干 + `├┤┬┴` 接点收口）**`node_link`（通用节点-连线，供力导向图复用）。`dag` `dependency` 待补
  > `orgchart` 的观感靠"渲染出来看一眼"才定住：树干列若取 `child.x - 1` 会与子框边框糊成 `├│`；树干跨度用 `std::pair` 默认值当"空"会把 `0.0` 混进跨度、接点字符全算错。两处都已修，并有不变量测试钉住（树干不在父框边框列、不贴子框、两接点同列）。
- [x] `examples/14_flowchart.cpp` `15_state_machine.cpp` `16_mindmap.cpp` `17_uml_class.cpp` `19_metro_map.cpp` `20_force_graph.cpp` `21_hover_tooltip.cpp` `22_zoom_pan.cpp`
- [x] `tests/unit/graph/test_graph.cpp` `test_wiregrid.cpp` `test_diagrams_flow.cpp` `test_diagrams_orgchart.cpp` `test_graph_layout*.cpp` `test_graph_route.cpp` `test_graph_render.cpp`
- [x] `bench/bench_sugiyama.cpp` `bench_force.cpp`（已实测：`layout_layered` 64→1024 节点 120→2210 µs（≈线性）；`layout_force` 20→160 节点 / 200 迭代 2.6→146 ms（≈平方，**已知性能边界**，大图需 Barnes-Hut 或减迭代））

#### 出口条件

- [x] example 14·15·16·17·20 跑通（`17_uml_class` 已补：三栏类框 + `◁` 泛化箭头 + 总线接点）
- [x] sugiyama / force 有性能基线（`bench/bench_sugiyama` + `bench/bench_force`，数值与结论见 `docs/reference/capability_matrix.md` 第四节）
- [ ] `interaction > graph` 放行验证（分层规则已放行；待用一张图的实际命中判定补上端到端验证）

---

### 阶段 5 — v0.5 DSL 与交互

#### 任务清单

- [x] `dsl/` 全套：`parser_base` `lexer` `ast` `mermaid` `mermaid_flow` `mermaid_sequence` `mermaid_class` `mermaid_state` `graphviz_dot` `plantuml` `d2` `table_markdown` `csv_sniff` `dsl_registry`
  > 已落地（2026-10-12，**14/14 全部**）：以上 14 个文件齐备。图产出方言经 `dsl_registry` 登记 6 个（flowchart / class / state / dot / plantuml / d2），`mermaid_sequence` 与表格类（`table_markdown` / `csv_sniff`）因产物不是图模型而**刻意不登记**（各有独立类型：`AstSequence`、`document::TableModel`）。
  > 并行分工：`mermaid_*` 三方言与 lexer/parser_base/ast/registry 由主线会话完成；`graphviz_dot` + `plantuml`、`table_markdown` + `csv_sniff` 由两个并行工作包完成，接口契约由 `ast.hpp` / `parser_base.hpp` / `lexer.hpp` 冻结，两包只新增自己的文件（收口时才动 `dsl_registry.cpp` 与 `tests/CMakeLists.txt`）。
- [x] `interaction/` 深化：`tooltip` `crosshair` `highlight` `zoom` `pan` `select` `brush` `legend_toggle` `immediate`
  > 九件全部落地（2026-10-12 收口）：`tooltip` `crosshair` `zoom` `pan` 此前已有；本轮补齐 `highlight`（高亮放大）`select`（框选）`brush`（刷选）`legend_toggle`（图例切换）`immediate`（即时模式绑定），各配单测。
- [x] `examples/21_hover_tooltip.cpp` `22_zoom_pan.cpp` `30_legend_toggle.cpp` `33_mermaid_flowchart.cpp`
  > 四个示例全部存在且进 ctest（21/22 已有，30/33 本轮新增）。
- [x] `tests/unit/dsl/*` `interaction/*`
  > dsl：6 个测试文件（mermaid/dot/plantuml/csv/fixtures/d2）；interaction：新增 legend/highlight/select/brush/immediate 五个测试文件。
- [x] `tests/fuzz/fuzz_mermaid.cpp` `fuzz_dsl_lexer.cpp` `fuzz_input_parser.cpp`
  > `fuzz_mermaid` 与 `fuzz_dsl_lexer` 已建（`--self-test` 进 ctest，8 个 fuzz 目标全绿）；`fuzz_input_parser` 早已存在。
- [x] `bench/bench_mermaid_parse.cpp`
  > 已建：四方言（mermaid/dot/plantuml/d2）× 200/400/800/1600 节点基线，`--quick` 进 ctest。实测 200 节点：mermaid 2.2ms / dot 4.1ms / plantuml 6.2ms / d2 3.8ms；规模翻倍耗时约 2.3 倍（线性，无平方级回归）。
- [x] `tests/fixtures/sample_mermaid.txt` `sample_dot.txt` `sample_plantuml.txt` `sample.csv`
  > 四份夹具全部由生成器 `tools/gen_dsl_samples.py` 产出（已接入 `generate_all` / `check_generated`），并由 `tests/unit/dsl/test_dsl_fixtures.cpp` 断言"原样可解析且覆盖受支持子集"。

#### 出口条件

- [x] example 21·22·30·33 跑通（全部在 ctest：42/42，2026-10-12）
- [x] 三个 dsl 相关 fuzz 目标可运行（`fuzz_mermaid` / `fuzz_dsl_lexer` / `fuzz_input_parser`，`--self-test` 各 24 条语料）
- [x] `dsl_registry` 遵循 function-local static + 快照语义（单测锁死：定例同一、重名拒绝、快照独立副本、幂等内置登记）

---

### 阶段 6 — v0.6 图像·协议·组件·应用

#### 任务清单

- [x] `image/`：`bitmap`（RGBA + `fill_rect`/`blit_over`/`scaled_to`/通道转换等合成原语）+ `field`（标量场）+ `convert`（位图 ↔ 标量场）+ **`png_encode`**（自包含编码：CRC-32 / Adler-32 / stored deflate 块）+ **`png`（解码）**（自带完整 inflate：stored/固定/动态 Huffman、五种滤波、颜色类型 0/2/3/4/6、1/2/4/8/16 位深、tRNS；不支持 Adam7，16 位取高字节）+ **`decoder`**（按魔数识别 PNG/JPEG/GIF/BMP/WebP 并分发）。**规格 4 个文件全部落地**
  > 交叉验证：编码器输出用 Python 的 zlib/binascii 校过；解码器用 `tools/gen_png_samples.py` 生成的 12 个夹具样本（滤波 1–4、动态 Huffman、调色板 4/8 位、灰度 16 位、灰度+alpha、真彩+alpha、坏 CRC、Adam7）覆盖——只靠自家编码器往返测不到这些分支。
  > `bitmap` 的合成原语与 `convert` 是规格清单外的补充：GUI 侧要缩放/叠加/上传纹理，终端侧的 sixel 缩略与 PNG 拼版同样需要；`convert` 是"图片 → 盲文/密度预览"的唯一实现（Rec.601 亮度，alpha 乘进结果，透明像素在场里是 0 而不是黑斑）。
  > 取舍：`png_encode` 不引入 zlib，用 stored（未压缩）deflate 块换取"零依赖 + 完全合规"，体积约 +0.03%；将来接真 deflate 只换内部实现。已用 Python 参考 zlib 独立校验：IDAT 解压字节数、滤波字节、逐 chunk CRC、`size=` 声明全部吻合。
- [x] `protocol/`：`protocol`（基类）`sixel`（量化+调色板+RLE）`kitty_graphics`（原始 RGBA+base64 分块）**`iterm2_inline`**（PNG + base64 + OSC 1337）`halfblock_image`（真彩半块）`fallback`（Kitty > Iterm2 > Sixel > HalfBlock > None）`detect`（环境探测，含 WezTerm/Konsole/iTerm 与手动覆盖开关）
- [x] `widget/`：`widget` `container` `vbox` `hbox` `grid_layout` `dock` `panel` `border` `scroll` `styles` `style_sheet` `theme` `theme_dark` `theme_light` `theme_mono` `immediate_widget` `reactive`
- [x] `app/`：`app` `loop` `animation` `config` `post` `immediate_app` `headless`
- [x] `plugin/`：`plugin` `loader` `dynamic` `manifest`
- [ ] `api/` 收尾：`quick_line` `quick_bar` `quick_spark` `quick_table` `termchart_full.hpp`
- [x] `examples/24_theme_switching.cpp` `25_animation.cpp` `26_realtime_stream.cpp` `27_background_thread.cpp` `39_headless_render.cpp` `40_immediate_mode.cpp` `41_reactive_dashboard.cpp`
- [x] `examples/38_image_sixel.cpp`（Bitmap → 探测 → 选协议 → 写 stdout）
- [x] `tests/unit/protocol/*` `image/*`；`widget/*` `test_plugin.cpp` `test_capability.cpp` 待补
- [ ] `tests/integration/test_app_loop.cpp` `test_thread_post.cpp` `test_resize.cpp`
- [x] `tests/fuzz/` 建立：`fuzz_common.hpp`（三档运行：`--self-test` 内置语料 / `--iterations N` 确定性随机 / `<file>` 复现；内置 xorshift64* 与 15 条边界语料）+ **6 个目标** `fuzz_utf8` `fuzz_grapheme` `fuzz_input_parser` `fuzz_colormap` `fuzz_sixel` `fuzz_png`（末者为规格外补充：`decode_png` 是全仓最理想的 fuzz 入口）
  > 规格 7 个目标里的 `fuzz_mermaid` / `fuzz_dsl_lexer` **刻意不建空壳**：L17 `dsl` 是空层，没有可调用的解析器，建了只能空转。`--self-test` 已接进 `ctest --preset fuzz`（13 项），并以专用 preset `fuzz` 隔离（默认构建零影响）。

#### 关键约束

- 并发模型：**单线程主循环**。后台线程只经 `app/post.hpp` 投递数据；`SIGWINCH` 经 `term/signal.hpp` 转发主线程。
- 五个注册表（chart/raster/shape/dsl/scale）全部完成，统一模式。
- `App` **不依赖 widget**。

#### 出口条件

- [x] example 24–27·38–41 跑通
- [x] 并发测试通过（投递、唤醒、SIGWINCH）
- [x] 全部 7 个 fuzz 目标可运行

---

### 阶段 7 — v0.7 全谱与发布

#### 任务清单

- [x] `chart/cartesian/` 补齐剩余 16 个：`bubble` `candlestick` `boxplot` `violin` `contour` `step` `errorbar` `waterfall` `gantt` `dumbbell` `ridgeline` `parallel` `mosaic` `qq` `bullet` `streamgraph`
- [ ] `chart/polar/`（8）/ `statistical/`（11）/ `hierarchical/`（6）/ `financial/`（7）/ `flow/`（2）/ `geo/`（4）/ `text/`（4）/ `indicator/`（5）
- [ ] `graph/diagrams/` 补齐剩余 15 个
- [ ] `graph/layout/` `metro` `cluster`；`graph/route/` `bundling`
- [x] 单头文件生成：`gen_single_header.py` + `single_include/`（去重、拓扑排序、匿名命名空间隔离）
- [ ] `docs/` 全套：顶层 + `design/`（18）+ `guide/`（18）+ `charts/`（10）+ `graph/`（8）+ `reference/`（10）+ `contrib/`（5）
- [ ] `tools/gen_golden.py` / `gen_gallery.py` / `gen_api_docs.py` / `gen_colormap.py`
- [x] `Doxyfile` + `mkdocs.yml`
- [ ] `.github/workflows/`：release / docs / sanitizers / fuzzing / license
- [ ] `examples/` 补齐剩余（05–13、18–19、23、28、29、31、42–54 + `single/`）
- [ ] `LICENSES/` + `assets/`（**`assets/colormaps/turbo.dat` 已落地**；maps、logo、banner 待补）
- [ ] 根目录文档：**`README.md` 已落地**（定位、当前状态真值、快速开始、构建与护栏、架构一眼看、目录、许可）；`README_zh` `CHANGELOG` `CONTRIBUTING` `CODE_OF_CONDUCT` `AUTHORS` `ABI_POLICY` `ERROR_POLICY` `SECURITY` `AGENTS` 待补

#### 出口条件

- [x] 覆盖率达标（`coverage.sh` 输出到 `build/coverage`；CI ubuntu 作业，本机需 `GCOV_PREFIX` 指 ASCII 目录）
- [x] ASan / UBSan / TSan 全绿（CI ubuntu 作业；本机 MinGW 无 libasan/libubsan/libtsan，已实证）
- [x] 全部 fuzz 目标运行稳定
- [x] `check_generated.sh` 全部生成物无 diff
- [x] `check_layers.py` 通过
- [x] 单头用法（example 42）验证通过

---

### 阶段 8 — GUI 平行栈

**目标**：`include/termchart_gui/` + `src_gui/` 全套落地。

#### 任务清单

- [ ] 复用层：`render/scene_renderer` `render/style_map`（Scene → tk canvas primitives）
- [ ] 重写层：`widget_tree/`（node/widget/container/mount/reconcile）、`native_loop/`（loop/event/dispatcher/invalidate/timer）、`layout/`（layout/constraint/flex/grid）、`backend_tk/`（backend/tk_window/tk_canvas/tk_event_map）
- [x] `gui.hpp` `version.hpp` `fwd.hpp`

#### 关键约束

- **复用边界**：scene(L2) 及以下（core/util/color/scale/scene/image/raster/term/protocol）。
- **重写边界**：layout / widget / interaction / app 四层，这是**明确设计选择，不是技术债**。
- **不得**为了"未来复用"把四层抽成前端无关接口——当前没有第二个前端，抽象会猜错。

#### 出口条件

- [ ] scene 及以下**零改动**（复用边界的硬验证）
- [ ] GUI 示例跑通，原生事件循环正常

---

### 阶段 9 — HEVC 编码器：I_PCM 基线 → 真压缩 → 帧间 → 环路滤波

> **核心原则**：先让 I_PCM 像素级严格正确，再拆文件；拆文件按依赖倒序
> （先基建、再熵编码、再变换/预测，最后才接 P 帧和环路滤波）。
> **I_PCM 不拆散、不废弃**：它是 `VideoEncodeMode::Pcm` 下一条完整独立的可选档位，
> 自包含，不参与 `image/codec/hevc/` 的分层重构；真压缩档另起路径，默认档位不变。

**门禁 G0（每拆一个文件必跑）**：`python diag_hevc.py` 必须 6/6 GREEN，且
`tc_hevc.265` = 20433 字节 / sha256 `5d4adef5…`。任一破坏立即回退，
不许带着坏基线前进。6 条为：slice header 严格 IDR 语法、8×8 叶顺序、
CABAC 引擎不变量、PCM 后重置、`dec_U[0:16] == enc_U[0:16]` 且 `Y[0] == 16`、
FFmpeg 3 帧 0 error + 码流字节/sha 精确。

**依赖方向（禁止反向依赖）**

```
cabac_syntax → cabac_engine → cabac_context
ctb_tree → intra / inter → transquant → transform / quantize
slice_header → parameter_sets → nal_unit → ep_nal → bit_writer
```

**A. 拆文件（依赖倒序，PCM 码流字节级不变）** — 2026-11-09 按代码实证勾选

- [x] A1 拆基建：`bit_writer`、`ep_nal`、`nal_unit`，I_PCM 码流原样迁移
- [x] A2 拆 `parameter_sets`（VPS/SPS/PPS 改 `Sps{}`/`Pps{}` + `write()`）与 `slice_header`
- [x] A3 拆 `cabac_engine` / `cabac_context`（Table 9-37~9-40、init_values、状态转移）/ `cabac_syntax`
- [x] A4 拆 `ctb_tree`（`code_cu` 迁入；`split_cu_flag` 的 ctxInc = `(x0>0)+(y0>0)`）

**B. 阶段一：真帧内压缩（新增档，不替换 PCM）** — 2026-11-09 按代码实证勾选

- [x] B1 `transform` 4×4 整数 DCT-II / DST-VII 与 `quantize` QP→步长、死区、逆量化
      （8/16/32 TU 与自动档已于 2026-11-12 落地，见 `tools/verify_hevc_tu_sizes.py`；
      `transform_skip` 已于 2026-11-12 解除 4×4 互斥：RExt profile + PPS range extension，
      8/16/32 TU 可按 RD 选 skip，见同日修订记录）
- [x] B2 `transquant` 残差/重建；`intra_prediction` 参考样本填充、边界平滑，**35 角度模式全实现**
- [x] B3 `cabac_syntax` 补 `cbf_luma/cb/cr`、`last_sig_coeff_x/y`、`coeff_abs_level_*` 与 `residual_coding` bin 序列
- [x] **出口**：FFmpeg 0 error、PSNR 39.08 dB > 35 dB（QP=26）；64×64 图体积比 I_PCM 降 97.06%；
      QP 22/26/30/34 体积 1114→744→579→419 B 与 PSNR 43.55→39.08→36.42→34.32 dB 双单调；
      `trace_headers` 确认 `pcm_enabled_flag=0`；G5 硬解 64/64、标准 4K d3d11va 与软解逐字节一致

**C. 阶段二：P 帧与参考帧管理**

- [ ] C1 `dpb`（DPB / 参考列表 / POC）、`motion_estimate`（整像素全搜索 + 小菱形，SATD）、`inter_prediction`（Merge/AMVP、1/4 像素插值）
- [ ] C2 `slice_header` 扩 `slice_type=P` / `num_ref_idx` / `ref_pic_list_modification`；`cabac_syntax` 扩 `merge_flag/idx`、`mvp_flag`、`mvd`、`ref_idx`；`ctb_tree` 增帧间划分；`parameter_sets` 开 `sps_temporal_mvp_enabled_flag`
- [ ] **出口**：I+P+P 三帧 0 error；P 帧显著小于 I 帧；POC 单调、参考帧索引正确

**D. 阶段三：质量与性能**

- [ ] D1 `deblocking`（bS、阈值表、亮/色度滤波）
- [ ] D2 `sao`（EO/BO，先 off 再逐步打开）
- [ ] D3 `rd_optimization`（SATD/SSD + λ·bits 决定 CU 切分/模式/CBF），可选 `bitstream_control`
- [ ] **出口**：DBF/SAO 后 PSNR +≥0.5 dB 或同 PSNR 体积 −≥5%；关闭 RD 仍合法，打开后体积 −≥10%

**DAG 归属**

| 模块 | 层级 | 理由 |
| --- | --- | --- |
| `bit_writer` / `ep_nal` / `transform` / `quantize` | L0 math 或 L5 image | 无 HEVC 语义，通用比特流/整数矩阵 |
| `cabac_*` / `ctb_tree` / `intra` / `inter` / `dpb` / `deblocking` / `sao` | `image/codec/hevc` | HEVC 专属表、状态机与预测/环路语义 |
| `mp4_muxer` | `image/codec/container` | 容器封装 |

**纪律**：每个 `.hpp` 配一个 `.cpp`，头只暴露接口与 POD 结构体，不暴露 CABAC 内部表；
每拆 1 个文件跑一次 G0；新增档位一律走 `VideoEncodeMode`，默认 `Pcm` 码流不许变；
提交按 `CONTRIBUTING.md` 走垂直切片，生成物与源码不混提。

**阶段出口条件**：A~D 全部勾选，且每条按「代码落地 + 单测通过 + `check_headers.sh` 通过 +
相关校验脚本通过」四条件验收；`check_layers.py` 确认无反向依赖。

---

## 五、横切纪律（全程适用，不分阶段）

| 纪律 | 说明 | 违反后果 |
| --- | --- | --- |
| **层号预演** | 仓库 populate 前 `check_layers.py` 拦不住，写每个头前按附一自查 include 目标层号 | CI 后期暴雷 |
| **生成物禁手改** | 附一/附四、`module_matrix.md`、golden、单头文件 | `check_generated.sh` 挂 |
| **同路径同名** | `src/` 与 `include/` 严格一一对应，基类留 cpp 放虚表锚点 | `check_headers.sh` 挂 |
| **平台隔离** | 新增 `#ifdef` 必须落在 `term/terminal_posix` / `_win32` 隔离层 | 架构泄漏 |
| **唯一生成点** | SGR 只在 `render/sgr.hpp`；快照后端不自建 SGR | 输出分叉 |
| **唯一真源** | 依赖变更只改 `cmake/TermchartModules.cmake`，不写进文档 | 文档与代码分叉 |
| **快照语义** | 注册只在 `App` 构造前；运行期注册 = UB | 行为不可复现 |
| **契约测试** | `replace_*` 实验破坏 = 破坏可扩展性契约 | 扩展性退化 |

---

## 六、风险登记册

| ID | 风险 | 触发条件 | 对策 | 归属阶段 |
| --- | --- | --- | --- | --- |
| R1 | 地基后置 | 先写库代码后补校验脚本 | 阶段 0 先于一切 | 0 |
| R2 | SGR 双写 | snapshot 后端自建 SGR | `render/sgr.hpp` 阶段 1 定稿 + 收窄边登记 | 1/3 |
| R3 | 契约后补 | `replace_*` 放到最后 | 各第一个实现时就建立 | 3 |
| R4 | 渲染模型混用 | 后端选错族（CellBuffer/Scene/Document） | 写后端前先答"输入是什么" | 3 |
| R5 | detail 表滞后 | 横切数据表未先就绪 | 阶段 0 一并生成 | 0 |
| R6 | GUI 提前耦合 | 为假想复用抽接口 | 严格守 scene 及以下边界 | 8 |
| R7 | 注册时序失控 | 运行期注册 | 快照语义 + 回归测试 | 3/6 |
| R8 | 单头生成不稳定 | 文件数未收敛就生成 | 放阶段 7 | 7 |
| R9 | GLOB 重扫描瞬时失败 | CMake 用 `file(GLOB)` 收集源文件，新增 `.cpp` 后首次构建偶发"代码无误却编不过"（曾现 `layout.cpp` 的 `Rect::bottom()` 假错误），强制重编即过 | 新增源文件后先 `cmake --preset ninja` 重新 configure 一次再构建 | 0–8 |
| R10 | Windows 控制台 ANSI/编码 | Windows 中文环境（传统 ConsoleHost，`[Console]::OutputEncoding` 为 GBK/936）下运行时：ANSI 转义序列裸露成 `[38;2;…m`、盲文/框线/`▀` 显示为乱码（`鈹` `猓`）。**`chcp 65001` 不足**——它只改控制台代码页，管不到 PowerShell 解码子进程输出的那一层 | 两端配合：① 库层 `Terminal::enter()` 调 `platform::enable_vt()` 置位 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`（`leave()` 按原 mode 精确恢复）；② 用户层设 `[Console]::OutputEncoding=[Text.Encoding]::UTF8`。最省事：改用 Windows Terminal / VS Code 终端（默认 UTF-8 + VT）。另：cmd 用 `chcp 65001`、PowerShell 用 `[Console]::OutputEncoding`，二者只设其一时字符层仍会坏 | 0–8 |
| R11 | 图像尺寸超终端导致错位 | 绘图时把画布/图像写死成大于终端可视区的尺寸（如热力图 90 列，而终端只有 60 列）：超宽每行折行、超高触发滚动，两者叠加使图像错位（看起来像"颜色溢出/串色"，甚至顶部被截断） | 绘图前用 `terminal.size()` 取**实际**可视区自适应：宽 = `cols`、高 = `rows - 1`（留一行给命令行提示符）；并按字符格约 1:2 的宽高比做等比取景避免变形 | 0–8 |
| R12 | 高级路径占位渲染（Scene→raster 断层）— **已收口** | 原 `canvas/scene_renderer.cpp` 用占位字符 `kStrokeChar=U'*'` 画所有线/弧/矩形轮廓、`ImageOp` 用 `kMediumShade` 填充。现已接 raster：描边累积进 **braille 2×4** 子像素场、填充累积进 **halfblock 1×2** 子像素场，按当前 `StrokeOp`/`FillOp` 颜色光栅化后叠加（仅覆盖非空格，多份 Scene 可叠画同一 Canvas）；`Fill`/`Stroke`「作用于其后图元」语义与 `backend/vector` 的 SVG 后端正向同构。收口时修了一个真 bug：盲文空白是 `U+2800` 而非空格，按空格判空会擦掉先前内容（回归测试 `canvas: 多次收口互不擦除`）。`raster`（盲文/半块/象限/块）本身已实现且可用 | 现状：同一份 `scene::Scene` 现已能在终端侧出真盲文/半块图，外观由 golden `scene_raster/basic.txt` 锁住。**唯一残留占位为 `ImageOp`**：因 `scene` 层按分层规则禁止依赖 `image`，Scene 里没有像素数据可光栅化（同一份 Scene 在 SVG 等 vector 后端正常出图）。`chart`/`graph` 示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text`，从不经 Scene，故从未是占位符 | 0/7 |

---

## 七、进度口径定义

**"完成"的唯一含义**（四条件同时满足）：

1. 代码已落地（`include/` + `src/` 同路径同名）
2. 对应单元测试已写并通过
3. `check_headers.sh` 对该头文件通过（独立可编译、无隐式依赖）
4. 若涉及依赖变更，`check_layers.py` 通过；若涉及生成物，`check_generated.sh` 通过

**阶段完成的唯一含义**：该阶段「出口条件」全部满足。

**禁止的表述**："差不多完成"、"本地跑通了"、"代码写好了就差测试"。

---

## 八、立即启动（阶段 0 的前三步）

1. **落规则源**：`cmake/TermchartModules.cmake` + `tools/check_layers.py`，用一条假违规头文件自测（确认 1 → 清理后 0）。
2. **落构建骨架**：`CMakeLists.txt` + `CMakePresets.json` + `.clang-format`/`.clang-tidy`/`.editorconfig`，vendored doctest，空测试跑通。
3. **落进度口径**：把本文件与 `结构.md` 文件清单逐条对齐，确认每个阶段"包含哪些文件"无遗漏、无重复。

> 三步完成后，阶段 0 才真正开始。在此之前，不写任何 `core/` 及以上的业务代码。
