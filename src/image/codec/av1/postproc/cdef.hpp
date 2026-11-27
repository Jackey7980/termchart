// cdef.hpp —— AV1 CDEF：8 方向搜索 + 一级/二级约束滤波（规范 7.9.2）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_CDEF_HPP_
#define TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_CDEF_HPP_

#include <cstdint>

namespace termchart::image::codec::av1 {

struct Av1CdefParams {
    int pri_strength = 0;  // 0..31
    int sec_strength = 0;  // 0..31
    int damping = 5;       // 帧头 cdef_damping_minus_3 + 3
};

// 8x8 块的 8 方向搜索，返回方向 0..7，并通过 var 输出方向方差（供 strength 调整）。
int av1_cdef_find_dir(const std::uint8_t* img, int stride, unsigned* var);

// 单个 8x8 块滤波；src/flat 均为 8x8，edges 位：1=左 2=右 4=上 8=下。
void av1_cdef_filter_8x8(std::uint8_t* dst, int dst_stride,
                         const std::uint8_t* src, int src_stride,
                         int dir, const Av1CdefParams& params, int edges);

// 整个平面按 8x8 块做方向搜索 + 滤波（块边界像素取钳制扩展）。
void av1_cdef_plane(std::uint8_t* plane, int stride, int w, int h,
                    const Av1CdefParams& params);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_POSTPROC_CDEF_HPP_
