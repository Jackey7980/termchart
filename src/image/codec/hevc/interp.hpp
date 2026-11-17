// interp.hpp —— HEVC 1/4 像素插值滤波器（对齐 FFmpeg hevcdsp put_hevc_qpel_uni / epel_uni）
//
// 仅用于 P 片单向（uni）预测：编码端重建必须与 ffmpeg 解码端逐位一致，故取整、
// 中间精度、边界扩展严格照 HEVC 规范 8.3.4 / FFmpeg hevcdsp_template.c：
//   * luma 8-tap，系数 ff_hevc_qpel_filters[frac]，frac∈1..3（1/4、2/4、3/4）；
//   * chroma 4-tap，系数 ff_hevc_epel_filters[frac]，frac∈1..7（1/8 精度）；
//   * 单向 8bit：h 先算 Σc·s，v 再算 Σc·t，最终 clip((x + 32) >> 6)；
//   * hv 两趟：水平结果保留 int16（不 clip），垂直结果才 clip（但水平先 >>6 会丢精度，
//     故按 FFmpeg 实现：水平输出 t=(Σc·s)>>0 存 int16，垂直 clip((Σc·t + 32) >> 6)）。
//   * 边界采样按 FFmpeg emulated_edge_mc：滤波抽头越界时向画面边缘复制。
// 本模块是纯函数，独立于 MV/merge/语法；默认 P 帧路径（整数 MV）不调用它。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_INTERP_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_INTERP_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// 1/4 亮度像素插值一块。ref 为整帧 Y 平面（ref_w×ref_h），待预测块左上锚点
// (x_int, y_int)（已含整数 MV 位移），分数位 (frac_x, frac_y)∈0..3（MV&3）。
// dst 尺寸 w×h，行跨距 dst_stride。8bit 单向预测。
void interp_luma(const std::uint8_t* ref, int ref_w, int ref_h, int x_int, int y_int,
                 int frac_x, int frac_y, int w, int h, std::uint8_t* dst, int dst_stride);

// 色度 4-tap：锚点与分数位均按色度平面坐标/单位（x_int,y_int 色度采样，
// frac∈0..7 = 1/8 色度精度；色度 MV = luma MV / 2，frac = (mv & 7)）。
void interp_chroma(const std::uint8_t* ref, int ref_w, int ref_h, int x_int, int y_int,
                   int frac_x, int frac_y, int w, int h, std::uint8_t* dst, int dst_stride);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTERP_HPP
