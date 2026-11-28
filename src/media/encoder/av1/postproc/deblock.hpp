// deblock.hpp —— AV1 8-bit 环路去块滤波：E/I/H 阈值、HEV 判定与
// 4/6/8/16 宽边缘的平坦区滤波，逐式对齐规范 7.9.1。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_DEBLOCK_HPP_
#define TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_DEBLOCK_HPP_

#include <cstdint>

namespace termchart::image::codec::av1 {

// sharpness 与帧头 loop_filter_sharpness 一致；level 为 0..63。
struct Av1DeblockParams {
    int level = 0;
    int sharpness = 0;
};

// 单条边缘。vertical=true 时 x 是边界列（左侧 p、右侧 q），否则 y 是边界行
// （上方 p、下方 q）；一次处理 length 个样点，width 为该段边缘的抽头宽度
// （4/6/8/16，取 4*相邻变换块边长）。
void av1_deblock_edge(std::uint8_t* buf, int stride, int x, int y,
                      bool vertical, int length, int width,
                      const Av1DeblockParams& params);

// 对整个平面按 8x8 变换块网格滤波所有内部边界（不含帧边界）；
// 适用于本库当前的 8x8 均匀分块帧。
void av1_deblock_luma_grid(std::uint8_t* y, int stride, int w, int h,
                           const Av1DeblockParams& params);
void av1_deblock_chroma_grid(std::uint8_t* plane, int stride, int w, int h,
                             const Av1DeblockParams& params);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_DEBLOCK_HPP_
