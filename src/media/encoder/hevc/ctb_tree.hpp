// ctb_tree.hpp —— HEVC 编码树（CTB/CU 四叉树）接口
//
// 头文件只暴露接口与 POD 帧视图，不暴露 CABAC 内部表。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CTB_TREE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CTB_TREE_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

class CabacEncoder;

// YUV420 平面的只读视图：cw/ch 为已按 CTB(64) 对齐后的编码尺寸。
struct CtbFrameView {
    const std::uint8_t* y;
    const std::uint8_t* u;
    const std::uint8_t* v;
    int cw;
    int ch;
};

// 编码一个 64x64 CTB：log2size>3 写 split_cu_flag 并四等分，8x8 叶写 PCM 块。
void code_ctb(CabacEncoder& enc, const CtbFrameView& frame, int ctb_x, int ctb_y);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CTB_TREE_HPP
