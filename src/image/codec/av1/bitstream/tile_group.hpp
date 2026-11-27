// tile_group.hpp —— 8x8 单块帧内 TileGroup：块头语法 + 8x8 亮度残差
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../predict/inter_predict.hpp"

namespace termchart::image::codec::av1 {

// 量化后的亮度变换系数，按变换块存储序 rc = x * 8 + y 排列；[0] 为 DC。
struct Residual8x8 {
    std::array<std::int16_t, 64> coeff{};
    // 15 = 不使用量化矩阵；0..14 取规范预设，与帧头 using_qmatrix/qm_y 对应。
    int qm_y = 15;
};

// 一期已验证的平坦流：8x8 DC/SMOOTH 帧内 + 全零残差，dav1d recon 恒为 128。
std::string write_tile_group_8x8();

// 残差实验路径：8x8 亮度变换系数的熵编码（自闭环已验证，跨解码器对拍仍在收敛）。
std::string write_tile_group_8x8(const Residual8x8& residual);

// 用与本编码器相同的反量化 / 反 DCT8 路径算出期望亮度平面，供像素级对拍。
std::array<std::uint8_t, 64> reconstruct_luma_8x8(const Residual8x8& residual);

// 单个 8x8 块的帧内模式选择；模式序号与 default_cdf.hpp 的枚举一致。
struct BlockMode8x8 {
    int y_mode = 9;            // 默认 SMOOTH_PRED
    int angle_delta = 0;       // -3..3，仅方向模式有效
    int filter_intra = -1;     // -1 关闭，0..4 为 filter intra 模式
    int uv_mode = 0;           // 0 = DC_PRED，13 = CFL_PRED
    int cfl_u_idx = 0;         // CFL 的 U/V 幅值索引（0..15）
    int cfl_v_idx = 0;
    int cfl_signs = 0;         // 联合符号索引（0..7）
};

// 8x16 双块帧：块 A（上）固定 SMOOTH + 给定残差，块 B（下）skip=1、
// 模式/角度/filter intra/CFL 任意。块 B 的重建结果就是纯预测输出，因此可以用
// 真实解码器逐像素验证全部帧内预测模式（含非平坦边界、滤波、上采样与 CFL）。
std::string write_tile_group_8x16(const Residual8x8& residual_a, int mode_b,
                                  int angle_delta_b);
std::string write_tile_group_8x16(const Residual8x8& residual_a,
                                  const BlockMode8x8& mode_b);

struct Reconstructed8x16 {
    std::array<std::uint8_t, 128> y{};
    std::array<std::uint8_t, 32> u{};
    std::array<std::uint8_t, 32> v{};
};
Reconstructed8x16 reconstruct_8x16(const Residual8x8& residual_a,
                                   const BlockMode8x8& mode_b);

std::array<std::uint8_t, 128> reconstruct_luma_8x16(const Residual8x8& residual_a,
                                                   int mode_b,
                                                   int angle_delta_b);

// 阶段五：8x8 INTER_FRAME 单块 tile。
// newmv_probe < 0：skip=1 + GLOBALMV，MV 由帧头全局运动决定（零 MV 主链）。
// newmv_probe >= 0：skip=0 + NEWMV，把 (mv_x, mv_y) 以 MV 残差语法（mv_joint +
// 分量 class0/classN/fp/hp）写进码流；joint 由 MV 是否为零推出，残差全零。
std::string write_tile_group_inter_8x8(int mv_x = 0, int mv_y = 0,
                                       int newmv_probe = -1);

// NEWMV + 亮度残差：帧间块的 tx_type 走 txtp_inter3 布尔（1 ⇒ DCT_DCT）。
std::string write_tile_group_inter_8x8(int mv_x, int mv_y, int newmv_probe,
                                       const Residual8x8& residual);

struct RefFrame8x8 {
    std::array<std::uint8_t, 64> y{};
    std::array<std::uint8_t, 16> u{};
    std::array<std::uint8_t, 16> v{};
};
// filter 必须与 INTER_FRAME 帧头里的 interp_filter 一致。
RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                 InterpFilter filter = InterpFilter::Regular);

// 预测 + 亮度残差（色度残差恒为零）。
RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                  const Residual8x8& residual,
                                  InterpFilter filter = InterpFilter::Regular);

// 双参考复合方式。AVG 取平均；WEDGE 用逐像素楔形权重表混合；
// DIFFWTD 按两路预测差值的绝对值推导权重（mask_type 位控制偏移方向）。
enum class CompoundMode8x8 {
    Avg = 0,
    Wedge = 1,
    DiffWtd = 2,
    DistWtd = 3,
};

struct Compound8x8Config {
    CompoundMode8x8 mode = CompoundMode8x8::Avg;
    int wedge_idx = 0;   // 仅 Wedge 有效，0..15
    int mask_sign = 0;   // Wedge 时交换两路；DiffWtd 时为 mask_type 位 0/1
    bool jnt_comp = false;  // DistWtd 需要序列头 enable_jnt_comp=1
};

// 帧级 ROTZOOM(2)/AFFINE(3) 全局运动的 8x8 GLOBALMV 块重建。
RefFrame8x8 reconstruct_gm_warp_8x8(const RefFrame8x8& ref, int type,
                                    int alpha, int beta, int gamma, int delta,
                                    int trans_x, int trans_y);

// 单参考块的 inter-intra 混合：0..3 = DC/VERT/HOR/SMOOTH，wedge=1 时用楔形掩码。
struct InterIntra8x8Config {
    int mode = 0;
    bool wedge = false;
    int wedge_idx = 0;
};
std::string write_tile_group_interintra_8x8(int mv_x, int mv_y,
                                            const InterIntra8x8Config& cfg,
                                            InterpFilter filter = InterpFilter::Regular);
RefFrame8x8 reconstruct_interintra_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                       const InterIntra8x8Config& cfg,
                                       InterpFilter filter = InterpFilter::Regular);

// 需要在帧头打开 switchable_comp_refs 之后才能用：块语法会多一个 is_compound 位。
// ref[0] = LAST、ref[1] = GOLDEN，块 MV 全部来自帧头 GMV 矩阵（GLOBALMV_GLOBALMV）。
std::string write_tile_group_compound_8x8(const Compound8x8Config& cfg = {});

// 本地复算：两路 MC + 复合（与解码端 blend 逐像素同构）。
RefFrame8x8 reconstruct_compound_8x8(const RefFrame8x8& last, int mv_last_x, int mv_last_y,
                                     const RefFrame8x8& golden, int mv_golden_x, int mv_golden_y,
                                     const Compound8x8Config& cfg,
                                     InterpFilter filter = InterpFilter::Regular);

// 阶段五：8x16 双块帧的 motion_mode=OBMC 路径。块 A（上）NEWMV(mv_a)，
// 块 B（下）NEWMV(mv_b) 且 motion_mode=MM_OBMC，帧头必须打开
// switchable_motion_mode（error_resilient 下只有一个 obmc 布尔，没有 warp 位）。
// obmc_enabled=false 只写两个 NEWMV 块、不写 motion_mode 符号，用于隔离
// 「帧头开关」与「块级 obmc 语法」两类错位。
std::string write_tile_group_obmc_8x16(int mv_a_x, int mv_a_y,
                                       int mv_b_x, int mv_b_y,
                                       bool obmc_enabled = true);

// 本地复算 OBMC 结果：A 区按 mv_a 预测；B 区基础为 mv_b 预测，其顶部
// (8*3)>>2 = 6 行再与「B 区按 mv_a 求出的预测」按规范掩码软混合。
// ref 必须是 8x16 帧（KEY 帧的重建），色度按 4:2:0 同步混合。
Reconstructed8x16 reconstruct_obmc_8x16(const Reconstructed8x16& ref,
                                        int mv_a_x, int mv_a_y,
                                        int mv_b_x, int mv_b_y,
                                        InterpFilter filter = InterpFilter::Regular);

// 逐块 warp 语法闭合：8x16 两块的 motion_mode=MM_WARP(2)，PARAMETERS 从邻居
// MV 推导；两邻居 MV 相同时退化为 IDENTITY warp，预测就是普通整帧 MC。
std::string write_tile_group_warp_8x16(int mv_x, int mv_y);
Reconstructed8x16 reconstruct_warp_identity_8x16(
    const Reconstructed8x16& ref, int mv_x, int mv_y,
    InterpFilter filter = InterpFilter::Regular);

// 阶段五多尺寸：16x16 帧单块 tile。64x64 / 32x32 两层的半个块都越过 16x16 帧
// 边界，规范要求解码端隐式取 PARTITION_SPLIT、不读符号；真正读划分符号的是
// 16x16 层（dav1d bl=3），该层 partition_cdf 有 10 个符号。因此 16x16 帧的
// tile 只需「10 符号 partition 写 NONE + 单块模式 + 无 8x8 层分区」。
std::string write_tile_group_intra_16x16(const BlockMode8x8& mode = {});

// 16x16 帧（4:2:0）重建：亮度 256 + U/V 各 64。
struct Frame16x16 {
    std::array<std::uint8_t, 256> y{};
    std::array<std::uint8_t, 64> u{};
    std::array<std::uint8_t, 64> v{};
};
// 帧角单块：上/左邻居都不在帧内，边界样本全不可用（解码端按 base=128 回填）。
Frame16x16 reconstruct_intra_16x16(const BlockMode8x8& mode = {});

// 16x16 帧间单块：partition NONE + skip=1 + NEWMV(mv)。skip 只关掉变换层符号，
// MV 残差语法照读；16x16 亮度与 8x8 色度各做一次整帧 MC（参考为完整 16x16 帧）。
std::string write_tile_group_inter_16x16(int mv_x, int mv_y);
Frame16x16 reconstruct_inter_16x16(const Frame16x16& ref, int mv_x, int mv_y,
                                   InterpFilter filter = InterpFilter::Regular);

// 16x16 帧的 PARTITION_SPLIT：2x2 个 8x8 子块，顺序按光栅
// (0,0) / (8,0) / (0,8) / (8,8)，与解码顺序一致 —— 每个子块的上/左邻居都已重建。
// 每个子块独立带走 skip、y_mode/angle_delta/uv_mode 与 8x8 亮度残差；
// 4:2:0 下色度是 4 个 4x4 块，残差恒为零。
struct Blocks16x16 {
    std::array<BlockMode8x8, 4> modes{};
    std::array<Residual8x8, 4> residuals{};
    // 1 = 无变换层符号（块重建只等于帧内预测）。
    std::array<int, 4> skip{};
};

std::string write_tile_group_intra_16x16_split(const Blocks16x16& blocks);
Frame16x16 reconstruct_intra_16x16_split(const Blocks16x16& blocks);

// 16x16 帧 PARTITION_SPLIT + 四个 8x8 NEWMV 子块，光栅顺序 (0,0)/(8,0)/(0,8)/(8,8)；
// 前三个子块只走普通 MC，右下子块再带 motion_mode = MM_WARP。它的 PARAMETERS 来自
// 上/左/左上三个已重建邻居（上邻居 = 右上子块、左邻居 = 左下子块、角 = 左上子块），
// 采样点数 3 才足以稳定拟合出非单位仿射矩阵；两个邻居 MV 相同或拟合越界时，
// 解码端退回 IDENTITY，重建自动改走普通整块 MC。
struct WarpSplit16x16Mvs {
    std::array<int, 4> mv_x{};
    std::array<int, 4> mv_y{};
};

std::string write_tile_group_warp_split_16x16(const WarpSplit16x16Mvs& mvs);

// 真实编码主循环的单块载荷：8x8 亮度块 + 4:2:0 色度对（色度残差恒为零）。
struct FrameBlock8x8 {
    BlockMode8x8 mode;
    Residual8x8 residual;
    int skip = 1;  // 1 = 只写模式，不写变换层符号
};

// 一帧的帧内编码计划，按 16x16 单元组织：
// split16=1 ⇒ 四个 8x8 子块各带模式与残差；
// split16=0 ⇒ 单个 16x16 块（skip=1、无残差，仅模式）。
struct IntraFramePlan {
    int width = 8;
    int height = 8;
    std::vector<std::uint8_t> split16;                  // ceil(w/16)*ceil(h/16)
    std::vector<BlockMode8x8> mode16;                   // 同上长度
    std::vector<std::array<FrameBlock8x8, 4>> blocks16;  // 同上长度，子块 z 序
};

// 按 64x64 超级块遍历写出整帧 tile：64/32 层按边界隐式 SPLIT，
// 16x16 层按 plan.split16 选 NONE/SPLIT，8x8 层写 4 符号 NONE + 模式/残差。
std::string write_tile_group_intra_frame(const IntraFramePlan& plan);

// 与解码端完全同一条「反量化 + 反 DCT8」路径的时域残差（供真失真 SSD）。
// base_q_idx 决定反量化步长，须与帧头的 base_q_idx 一致（默认 100 = 93/112）。
std::array<std::int32_t, 64> av1_intra_residual_signal_8x8(
    const Residual8x8& residual, int base_q_idx = 100);

// 真实码率探针：把「模式 + 残差」写进独立 RangeEncoder 后返回字节数。
// 供 RDO 用真实熵编码估比特，而不是幅值位数近似。
std::size_t av1_measure_block_bytes(const BlockMode8x8& mode, int above_ctx,
                                    int left_ctx, int skip_ctx, int skip,
                                    const Residual8x8& residual);

// 帧内模式 → kfym 上/左上下文（与解码端 CDF 选表同一张折叠表）。
int av1_intra_mode_context(int mode);
Frame16x16 reconstruct_warp_split_16x16(
    const Frame16x16& ref, const WarpSplit16x16Mvs& mvs,
    InterpFilter filter = InterpFilter::Regular);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
