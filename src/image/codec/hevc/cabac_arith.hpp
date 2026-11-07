// cabac_arith.hpp —— 阶段一 B3：标准连续 CABAC 算术编码器接口
//
// 与 I_PCM 专用的「每段 2 字节区间反解」引擎不同，这里维护持续跨越多个 CU 的
// 算术区间，输出连续 bitstream，供真帧内压缩档使用。
// 上下文状态与范围表复用 cabac_context（H.264/HEVC 共用 9-bit range 模型）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_HPP

#include <cstdint>
#include <string>

#include "cabac_context.hpp"

namespace termchart::image::codec::hevc {

class CabacArith {
public:
    void init();

    // 上下文自适应 bin；ctx.state 会按 9.3.4.2 演化。
    void encode_decision(CtxModel& ctx, int bin);
    // 等概率 bypass bin（不占上下文）。
    void encode_bypass(int bin);
    // 终止 bin：bin=1 结束当前 CABAC 段并 flush，bin=0 只做 renorm。
    void encode_terminate(int bin);

    // 结束段、补齐所有待输出字节（不含 rbsp_trailing_bits）。
    std::string finish();

private:
    void put_byte();
    void renorm();
    void flush(bool terminated);

    std::uint32_t low_ = 0;
    int range_ = 510;
    int queue_ = -9;          // 前 9 bit 是算术编码的固有前导，不落盘
    int outstanding_ = 0;     // 0xff 进位待定计数
    std::string out_;
    bool finished_ = false;
};

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_HPP
