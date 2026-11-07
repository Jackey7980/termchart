// cabac_syntax.hpp —— HEVC CABAC 语法元素的上下文映射与 bin 序列
//
// 头文件只暴露接口与常量；ctxInc 推导与 bin 登记规则在 .cpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP

namespace termchart::image::codec::hevc {

class CabacEncoder;

// PART_MODE 首 bin 的上下文（FFmpeg PART_MODE_OFFSET）。
inline constexpr int kPartModeCtx = 13;

// split_cu_flag 的 ctxInc（HEVC 9.3.4.2.2）：均匀四分树全拆到 8x8 叶时，
// 「左侧/上方有已编码 CU」等价于绝对坐标 > 0，故 ctxInc = (x0>0)+(y0>0)。
int split_cu_flag_ctx(int x0, int y0);

void write_split_cu_flag(CabacEncoder& enc, int x0, int y0, int bin);
void write_part_mode_2nx2n(CabacEncoder& enc);
void write_pcm_flag(CabacEncoder& enc, int bit);
void write_end_of_slice_segment_flag(CabacEncoder& enc, int bit);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_SYNTAX_HPP
