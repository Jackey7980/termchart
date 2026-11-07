// cabac_syntax.cpp —— HEVC CABAC 语法元素映射（A3-3 从 code_cu 抽出）
#include "cabac_syntax.hpp"

#include "cabac_engine.hpp"

namespace termchart::image::codec::hevc {

int split_cu_flag_ctx(int x0, int y0) {
    // 仅 CTB 根（64x64）用 (ctb_x>0)+(ctb_y>0)；非根 CU 的 CTB 内左上对角
    // 子块 ctxInc=1、内部子块 ctxInc=2。用 CTB 内坐标会在第 2 个及以后的
    // CTB 左/上边界漏掉邻居，导致右半帧全错，故统一用绝对坐标。
    return (x0 > 0 ? 1 : 0) + (y0 > 0 ? 1 : 0);
}

void write_split_cu_flag(CabacEncoder& enc, int x0, int y0, int bin) {
    enc.encode_bin(split_cu_flag_ctx(x0, y0), bin);
}

void write_part_mode_2nx2n(CabacEncoder& enc) {
    // FFmpeg ff_hevc_part_mode_decode(): if (GET_CABAC(PART_MODE_OFFSET))
    // return PART_2Nx2N，即 bin "1" → PART_2Nx2N，只读 1 个 bin。
    enc.encode_bin(kPartModeCtx, 1);
}

void write_pcm_flag(CabacEncoder& enc, int bit) {
    // pcm_flag 是【终止 bin】：FFmpeg ff_hevc_pcm_flag_decode() 直接返回
    // get_cabac_terminate(&lc->cc)，不占用上下文（PCM_FLAG 的 NUM_BINS=0）。
    enc.encode_terminating(bit);
}

void write_end_of_slice_segment_flag(CabacEncoder& enc, int bit) {
    // HEVC 7.3.8.1：每个 CTB 之后读一次终止 bin；除最后一个 CTB 外必须为 0。
    enc.encode_terminating(bit);
}

}  // namespace termchart::image::codec::hevc
