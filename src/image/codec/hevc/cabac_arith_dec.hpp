// cabac_arith_dec.hpp —— 阶段一 B3：与 CabacArith 对偶的标准 CABAC 算术解码器
//
// 只存在于自检：把 CabacArith 写出的连续 bitstream 按 9.3.4.2 语义解回同一串
// bin，从而在接真码流前证明「bin 序列 -> bits -> bin 序列」可逆。生产路径不使
// 用它（解码由外部播放器负责）。
//
// 与编码端的状态对应关系（推导自 cabac_arith.cpp 的 low/range 语义）：
//   · 编码端区间为 [low, low + range)，range <= 510（9 bit）；
//   · 解码端把 low 已落盘的高位剥掉，只保留「相对当前区间起点的码值」；
//   · 窗口按 16 bit 归一：value ≈ (code - low) 缩放到 2^16，range 映射到 range<<7，
//     故任何切分点都在 value < (range<<7) 处比较；
//   · 不变式 value < range<<7（range<=510 => value<65280<2^16），
//     故 renorm 每次左移补一个 bit 后不变式仍然成立。
// 这套 16 bit 窗口 + range<<7 比较的写法与 JM/HM/FFmpeg 的解码端同构，
// 因此「能往返」同时意味着「与标准解码器同语义」。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_DEC_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_DEC_HPP

#include <cstddef>
#include <cstdint>

#include "cabac_context.hpp"

namespace termchart::image::codec::hevc {

// 逐 bit 读取；流耗尽后返回 0（等价于解码器的补零，只影响 flush 尾部）。
class CabacBitReader {
public:
    void init(const unsigned char* data, std::size_t size) {
        data_ = data;
        size_ = size;
        bitpos_ = 0;
    }
    int read_bit() {
        if (bitpos_ >= size_ * 8) {
            ++bitpos_;
            return 0;
        }
        const int bit = (data_[bitpos_ >> 3] >> (7 - (bitpos_ & 7))) & 1;
        ++bitpos_;
        return bit;
    }

private:
    const unsigned char* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t bitpos_ = 0;
};

class CabacArithDec {
public:
    void init(const unsigned char* data, std::size_t size);
    void init(const char* data, std::size_t size) {
        init(reinterpret_cast<const unsigned char*>(data), size);
    }

    // 上下文自适应 bin；ctx.state 按 9.3.4.2 演化，必须与编码端同一初值。
    int decode_decision(CtxModel& ctx);
    // 等概率 bypass bin（不占上下文）。
    int decode_bypass();
    // 终止 bin：返回 1 表示当前 CABAC 段结束。
    int decode_terminate();

private:
    std::uint32_t value_ = 0;
    int range_ = 510;
    CabacBitReader br_;
};

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ARITH_DEC_HPP
