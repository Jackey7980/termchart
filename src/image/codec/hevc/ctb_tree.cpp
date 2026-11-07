// ctb_tree.cpp —— HEVC 编码树与 I_PCM 叶样本（A4 从 hevc_encode.cpp 原样迁入）
#include "ctb_tree.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "cabac_engine.hpp"
#include "cabac_syntax.hpp"

namespace termchart::image::codec::hevc {
namespace {

// 编码树：log2size > 3（>8x8）写 split_cu_flag=1 并四等分；到 8x8 叶写 PCM 块。
void code_cu(CabacEncoder& cabac, const CtbFrameView& f, int x0, int y0, int log2size,
             int ctb_x, int ctb_y) {
    const int size = 1 << log2size;
    if (log2size > 3) {  // MinCbLog2SizeY = 3（8x8）
        write_split_cu_flag(cabac, x0, y0, 1);
        const int half = size / 2;
        for (int i = 0; i < 4; ++i) {
            const int x1 = x0 + (i & 1) * half;
            const int y1 = y0 + (i >> 1) * half;
            code_cu(cabac, f, x1, y1, log2size - 1, ctb_x, ctb_y);
        }
        return;
    }
    // 8x8 叶（== MinCbLog2SizeY）：HEVC 7.3.8.5 coding_unit() 语法顺序要求先写
    // part_mode，当且仅当 part_mode == PART_2Nx2N 且满足 PCM 条件时才读 pcm_flag。
    // 8x8 恰好等于 MinCbLog2SizeY，故 part_mode 必须显式写出（不可省略）。
    write_part_mode_2nx2n(cabac);
    write_pcm_flag(cabac, 1);
    // 字节对齐（pcm_alignment_zero_bit）后写裸样本，落盘到 final_out。
    std::string raw; raw.reserve(96);
    const int cstride = f.cw / 2;
    for (int yy = 0; yy < 8; ++yy)  // luma 8x8
        for (int xx = 0; xx < 8; ++xx)
            raw.push_back(static_cast<char>(f.y[static_cast<std::size_t>((y0 + yy) * f.cw + (x0 + xx))]));
    for (int yy = 0; yy < 4; ++yy)  // Cb 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(static_cast<char>(f.u[static_cast<std::size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]));
    for (int yy = 0; yy < 4; ++yy)  // Cr 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(static_cast<char>(f.v[static_cast<std::size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]));
    cabac.flush_cu(std::move(raw));  // 字节对齐 + 拼 [cabac][raw] 到 final_out
}

}  // namespace

void code_ctb(CabacEncoder& enc, const CtbFrameView& frame, int ctb_x, int ctb_y) {
    code_cu(enc, frame, ctb_x, ctb_y, 6, ctb_x, ctb_y);
}

}  // namespace termchart::image::codec::hevc
