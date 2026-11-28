// transform.hpp —— HEVC 整数变换（DCT-II 4/8/16/32 + DST-VII 4x4）
//
// 与 ffmpeg 的 idct 完全同构以保证「编码端重建 == ffmpeg 解码」逐比特一致：
//   - inv_* 照搬 ffmpeg libavcodec/hevc/dsp_template.c 的 TR_4/8/16/32 蝶形：
//     等价于用 32 点基函数表 transform[32][32] 按 32/N 抽样得到的 NxN 矩阵做
//     两次 1D 矩阵乘（先列 shift=7、后行 shift=12），每级 av_clip_int16。
//     另外复刻 ffmpeg 的 col_limit 截断（last_x+last_y+4，按 max_xy 上限），
//     否则 8x8 以上会出现「编码端全量变换 vs 解码端截断」的重建漂移。
//   - fwd_* 用同一组矩阵做 adjoint 正变换（两段移位 x265 dct4/8/16/32 的
//     1/8、2/9、3/10、4/11），与 inv_* 构成整数往返（_transform_test 验证）。
// 纯整数（int64 累加，避免 32x32 溢出），无 HEVC 语法。行优先 idx = y*N + x。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// log2_size ∈ [2,5]（4x4..32x32）。use_dst 仅对 4x4 亮度帧内有效（其余一律 DCT）。
// residual / coeff 均为行优先 n*n（n = 1<<log2_size）。
void fwd_transform(const std::int16_t* residual, std::int32_t* coeff, int log2_size,
                   bool use_dst);

// col_limit 必须等于解码端由最后一个非零系数推出的截断上限（见 inverse_col_limit）；
// 传 n 表示不做截断（4x4 即恒为 4）。
void inv_transform(const std::int32_t* coeff, std::int16_t* residual, int log2_size,
                   bool use_dst, int col_limit);

// 复刻 ffmpeg cabac.c:1272-1279 的 col_limit 推导。
int inverse_col_limit(int last_x, int last_y, int log2_size);

// 4x4 兼容包装（= log2_size 2、col_limit 4）。
void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff, bool use_dst);
void inv_transform_4x4(const std::int32_t* coeff, std::int16_t* residual, bool use_dst);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
