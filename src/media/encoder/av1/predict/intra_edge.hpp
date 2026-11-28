// AV1 帧内预测的边界样本预处理（对应规范 7.11.2.2 / 7.11.2.9）。
//
// 预测器只吃“已经整理好的”上/左边界，所有边界相关的滤波与上采样都在这里完成，
// 与方向预测器的取角逻辑完全解耦。样本一律是 8 位（本项目只做 8-bit 纵切）。
#ifndef TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_EDGE_HPP_
#define TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_EDGE_HPP_

#include <cstdint>

namespace termchart::image::codec::av1 {

// 边界缓冲的两侧留白：角像素在 index -1，上采样还会写 -2。
inline constexpr int kEdgePad = 16;
// 单个变换块最大 64x64，上采样后 2*64，再留余量。
inline constexpr int kEdgeCapacity = 132;

struct IntraEdgeRows {
    std::uint8_t above_storage[kEdgePad * 2 + kEdgeCapacity]{};
    std::uint8_t left_storage[kEdgePad * 2 + kEdgeCapacity]{};

    std::uint8_t* above() { return above_storage + kEdgePad; }
    std::uint8_t* left() { return left_storage + kEdgePad; }
    const std::uint8_t* above() const { return above_storage + kEdgePad; }
    const std::uint8_t* left() const { return left_storage + kEdgePad; }
};

// 边界滤波强度：由块尺寸、角度偏移与边界类型决定，0 表示不滤波。
// type 0 = 亮度（阈值更松），1 = 色度。
int intra_edge_filter_strength(int bs0, int bs1, int delta, int type);

// 是否需要把边界上采样到半像素（细角度 + 小块）。
int use_intra_edge_upsample(int bs0, int bs1, int delta, int type);

// 3 抽头 [1,2,1] 族平滑，作用于 p[0..sz-1]。
void filter_intra_edge(std::uint8_t* p, int sz, int strength);

// 左上角样本单独用 5/6/5 平滑（大块的角点滤波）。
void filter_intra_edge_corner(std::uint8_t* above, std::uint8_t* left);

// 插值出半样本位置，结果写到 p[-2 .. 2*sz-2]。
void upsample_intra_edge(std::uint8_t* p, int sz);

// 角度到 dx/dy 的定点导数（以 256 为单位）。
int intra_dx(int angle);
int intra_dy(int angle);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_IMAGE_CODEC_AV1_PREDICT_INTRA_EDGE_HPP_
