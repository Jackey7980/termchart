// cabac_arith_dec.cpp —— CABAC 算术解码状态机（CabacArith 的对偶）
#include "cabac_arith_dec.hpp"

namespace termchart::image::codec::hevc {

void CabacArithDec::init(const unsigned char* data, std::size_t size) {
    // 9 bit 的每轮区间宽度 + 7 bit 的窗口归一 = 16 bit 初值。
    br_.init(data, size);
    range_ = 510;
    value_ = 0;
    for (int i = 0; i < 16; ++i) value_ = value_ * 2 + static_cast<std::uint32_t>(br_.read_bit());
}

int CabacArithDec::decode_decision(CtxModel& ctx) {
    const int s = ctx.state;
    const int r_lps = range_lps(s, range_);
    range_ -= r_lps;
    const std::uint32_t scaled = static_cast<std::uint32_t>(range_) << 7;

    int bin;
    if (value_ < scaled) {
        bin = s & 1;  // MPS
        if (range_ < 256) {
            range_ <<= 1;
            value_ = value_ * 2 + static_cast<std::uint32_t>(br_.read_bit());
        }
    } else {
        bin = (s & 1) ^ 1;  // LPS
        value_ -= scaled;
        range_ = r_lps;
        while (range_ < 256) {
            range_ <<= 1;
            value_ = value_ * 2 + static_cast<std::uint32_t>(br_.read_bit());
        }
    }
    ctx.state = next_state(s, bin != (s & 1));
    return bin;
}

int CabacArithDec::decode_bypass() {
    // 编码端：low <<= 1; bin=1 时 low += range。故切分点在 range<<7。
    value_ = value_ * 2 + static_cast<std::uint32_t>(br_.read_bit());
    const std::uint32_t scaled = static_cast<std::uint32_t>(range_) << 7;
    if (value_ >= scaled) {
        value_ -= scaled;
        return 1;
    }
    return 0;
}

int CabacArithDec::decode_terminate() {
    range_ -= 2;
    const std::uint32_t scaled = static_cast<std::uint32_t>(range_) << 7;
    if (value_ >= scaled) return 1;  // 段结束
    while (range_ < 256) {
        range_ <<= 1;
        value_ = value_ * 2 + static_cast<std::uint32_t>(br_.read_bit());
    }
    return 0;
}

}  // namespace termchart::image::codec::hevc
