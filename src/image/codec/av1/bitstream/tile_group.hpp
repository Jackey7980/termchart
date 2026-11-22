// tile_group.hpp —— 8x8 单块帧内 TileGroup：块头语法 + 8x8 亮度残差
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP

#include <array>
#include <cstdint>
#include <string>

namespace termchart::image::codec::av1 {

// 量化后的亮度变换系数，按变换块存储序 rc = x * 8 + y 排列；[0] 为 DC。
struct Residual8x8 {
    std::array<std::int16_t, 64> coeff{};
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

// 阶段五：8x8 INTER_FRAME 的单块 tile——skip=1 + GLOBALMV（MV 来自帧头全局运动）。
std::string write_tile_group_inter_8x8(int mv_x = 0, int mv_y = 0,
                                       int joint_override = -1);

struct RefFrame8x8 {
    std::array<std::uint8_t, 64> y{};
    std::array<std::uint8_t, 16> u{};
    std::array<std::uint8_t, 16> v{};
};
RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
