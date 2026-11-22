// AV1 帧内预测：61 个亮度模式（8 个方向基模式 x 7 档角偏移 + 5 个非方向）
// 加色度的 13 个 uv 模式，统一由 predict_intra() 覆盖。
//
// 本模块只做“给定边界样本 → 预测块”，不涉及任何熵编码/语法，
// 因此可以和写码流完全独立地单测。
#ifndef TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_PREDICT_HPP_
#define TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_PREDICT_HPP_

#include <cstdint>

#include "intra_edge.hpp"

namespace termchart::image::codec::av1 {

// 基角度（与 default_cdf.hpp 的 IntraMode 序号一致）。
int intra_base_angle(int mode);

// 方向模式判定 + 角度取值范围。
bool is_directional_mode(int mode);

struct IntraSamples {
    // above[i] 对应 (x + i, y - 1)；left[j] 对应 (x - 1, y + j)。
    const std::uint8_t* above = nullptr;
    const std::uint8_t* left = nullptr;
    int left_stride = 1;
    std::uint8_t corner = 128;

    int num_above = 0;          // 可用的上方样本数（不含 above-right）
    int num_left = 0;           // 可用的左侧样本数（不含 below-left）
    // -1 表示该延伸方向不存在（此时需要更多样本就复制最后一个），>=0 表示存在。
    int num_above_right = -1;
    int num_below_left = -1;
};

struct IntraPredictParams {
    int mode = 0;
    int angle_delta = 0;      // -3..3，仅方向模式有效
    int width = 8;
    int height = 8;
    int filter_intra_mode = -1;   // -1 关闭；0..4 对应 filter intra 5 档
    bool enable_edge_filter = true;
    int plane = 0;                // 0 亮度（边界滤波强度阈值更松）
    // 上/左邻居中只要有一个是 SMOOTH 族模式，边界类型就按 1（色度档）处理
    // （libaom get_intra_edge_filter_type：阈值更松、几乎不做上采样）。
    // 这是解码端从邻居块模式推出的量，调用方必须显式告知。
    bool neighbor_smooth = false;
};

// 把预测块写到 dst（宽度 width，行跨 dst_stride），样本 8 位。
void predict_intra(const IntraSamples& samples, const IntraPredictParams& params,
                   std::uint8_t* dst, int dst_stride);

// filter intra：5 种 8 抽头滤波器（规范 7.11.2.3），w==h 且 <=32。
// above/left 需已按 fill_edges 规则补齐（above[-1] 为角像素）。
void predict_filter_intra(const IntraSamples& samples, int w, int h, int mode,
                          std::uint8_t* dst, int dst_stride);

// CFL 色度预测：luma 是 2w x 2h 的重建亮度，alpha_q3 是带符号的 1/8 精度系数。
// base 是色度基础预测（本项目为 DC 128），结果写进 dst。
void predict_cfl_chroma(const std::uint8_t* luma, int luma_stride, int w, int h,
                        int alpha_u_q3, int alpha_v_q3,
                        std::uint8_t* u, std::uint8_t* v, int stride);

// 单独暴露给单测：各预测器（边界已在内部完成填充与滤波）。
void predict_dc(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                int stride);
void predict_v(const IntraSamples& s, int w, int h, std::uint8_t* dst,
               int stride);
void predict_h(const IntraSamples& s, int w, int h, std::uint8_t* dst,
               int stride);
void predict_paeth(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                   int stride);
void predict_smooth(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                    int stride);
void predict_smooth_v(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                      int stride);
void predict_smooth_h(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                      int stride);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_PREDICT_HPP_
