// cabac_arith.cpp —— 标准 CABAC 算术编码状态机
//
// 结构对偶于 FFmpeg get_cabac / x264 common/cabac.c：
//   range∈[256,510]，decision 用 rangeTabLps 细分，renorm 时按需输出字节，
//   0xff 字节延后处理进位。HEVC 与 H.264 的算术核心相同，差异在上下文初始化
//   与语法层；本项目 I_PCM 段已用 FFmpeg 模型逐位验证过同一套 range/low 语义。
#include "cabac_arith.hpp"

namespace termchart::image::codec::hevc {

void CabacArith::init() {
    low_ = 0;
    range_ = 0x1FE;   // 510
    queue_ = -9;
    outstanding_ = 0;
    out_.clear();
    finished_ = false;
}

void CabacArith::put_byte() {
    if (queue_ < 0) return;
    const std::uint32_t out = low_ >> (queue_ + 10);
    low_ &= (0x400u << queue_) - 1u;
    queue_ -= 8;
    if ((out & 0xFFu) == 0xFFu) {
        ++outstanding_;   // 进位未定，延后写
        return;
    }
    const int carry = static_cast<int>(out >> 8);
    if (!out_.empty())
        out_.back() = static_cast<char>(static_cast<unsigned char>(out_.back()) + carry);
    while (outstanding_ > 0) {
        out_.push_back(static_cast<char>(static_cast<unsigned char>(carry - 1)));
        --outstanding_;
    }
    out_.push_back(static_cast<char>(static_cast<unsigned char>(out & 0xFFu)));
}

void CabacArith::renorm() {
    const int shift = norm_shift(range_);
    range_ <<= shift;
    low_ <<= shift;
    queue_ += shift;
    put_byte();
}

void CabacArith::encode_decision(CtxModel& ctx, int bin) {
    const int s = ctx.state;
    const int r_lps = range_lps(s, range_);
    range_ -= r_lps;
    const bool lps = (bin != (s & 1));
    if (lps) {
        low_ += range_;
        range_ = r_lps;
    }
    ctx.state = next_state(s, lps);
    renorm();
}

void CabacArith::encode_bypass(int bin) {
    low_ <<= 1;
    if (bin) low_ += range_;
    ++queue_;
    put_byte();
}

void CabacArith::encode_terminate(int bin) {
    range_ -= 2;
    if (bin) flush(true);
    else renorm();
}

void CabacArith::flush(bool terminated) {
    if (finished_) return;
    if (terminated) {
        low_ += range_ - 2;
        low_ |= 1;
    }
    low_ <<= 9;
    queue_ += 9;
    put_byte();
    put_byte();
    if (queue_ < 0) low_ <<= -queue_;
    queue_ = 0;
    put_byte();
    while (outstanding_ > 0) {
        out_.push_back(static_cast<char>(0xFF));
        --outstanding_;
    }
    finished_ = true;
}

std::string CabacArith::finish() {
    flush(false);
    return out_;
}

}  // namespace termchart::image::codec::hevc
