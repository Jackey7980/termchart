#include "range_coder.hpp"

#include <bit>

namespace termchart::image::codec::av1 {

void RangeEncoder::normalize(std::uint64_t low, unsigned rng) {
    // rng 归一化到 [32768, 65535] 所需的左移位数
    const int shift = 16 - static_cast<int>(std::bit_width(rng));
    int have = cnt_ + shift;
    if (have >= 0) {
        int base = cnt_ + 16;  // 当前 low 中第一个尚未写出的字节起始位
        std::uint64_t keep = (std::uint64_t{1} << base) - 1;
        if (have >= 8) {
            staged_.push_back(static_cast<std::uint16_t>(low >> base));
            low &= keep;
            base -= 8;
            keep >>= 8;
        }
        staged_.push_back(static_cast<std::uint16_t>(low >> base));
        low &= keep;
        have = base + shift - 24;
    }
    low_ = low << shift;
    rng_ = rng << shift;
    cnt_ = have;
}

void RangeEncoder::write_interval(unsigned lo, unsigned width) {
    if (finished_ || width == 0 || width > rng_ || lo + width > rng_) return;
    // 解码器比较的是「码字补数」的高位，因此规范空间的 [lo, lo+width)
    // 对应编码器码字空间的 [range - lo - width, range - lo)。
    normalize(low_ + (rng_ - (lo + width)), width);
}

void RangeEncoder::write_bool(int bit, unsigned f_one) {
    const unsigned v = ((rng_ >> 8) * (f_one >> 6) >> 1) + 4u;  // bit == 1 的区间长度
    if (bit != 0)
        write_interval(0, v);
    else
        write_interval(v, rng_ - v);
}

std::string RangeEncoder::finish() {
    if (finished_) return result_;

    // 选最小的位数，使 [edge, edge + mask] 覆盖住当前区间，
    // 这样后续无论补 0 还是补 1 都能解码出同样的符号。
    const std::uint64_t l = low_;
    const unsigned r = rng_;
    int s = 9;
    std::uint64_t mask = 0x7FFF;
    std::uint64_t edge = (l + mask) & ~mask;
    while ((edge | mask) >= l + r) {
        ++s;
        mask >>= 1;
        edge = (l + mask) & ~mask;
    }
    s += cnt_;

    int shift = cnt_ + 16;
    while (s > 0) {
        if (shift >= 0) {
            staged_.push_back(static_cast<std::uint16_t>(edge >> shift));
            edge &= (std::uint64_t{1} << shift) - 1;
        } else {
            staged_.push_back(static_cast<std::uint16_t>(edge << -shift));
            edge = 0;
        }
        s -= 8;
        shift -= 8;
    }

    // 进位传播：暂存字节可能因进位超过 255，从后向前逐字节归一。
    int carry = 0;
    for (std::size_t i = staged_.size(); i-- > 0;) {
        const int sum = static_cast<int>(staged_[i]) + carry;
        staged_[i] = static_cast<std::uint16_t>(sum & 0xFF);
        carry = sum >> 8;
    }

    result_.reserve(staged_.size());
    for (auto b : staged_) result_.push_back(static_cast<char>(b));
    finished_ = true;
    return result_;
}

bool RangeDecoder::init(const std::uint8_t* data, std::size_t size) {
    data_ = data;
    size_ = size;
    pos_ = 0;
    dif_ = 0;
    rng_ = 0x8000;
    cnt_ = -15;
    refill();
    return true;
}

void RangeDecoder::refill() {
    int c = 64 - cnt_ - 24;
    while (c >= 0) {
        if (pos_ >= size_) {
            // 越界后按「无限补 1」处理，与真实解码器一致。
            if (c + 8 <= 64) dif_ |= (std::uint64_t{1} << (c + 8)) - 1;
            break;
        }
        dif_ |= static_cast<std::uint64_t>(data_[pos_++] ^ 0xFF) << c;
        c -= 8;
    }
    cnt_ = 64 - c - 24;
}

void RangeDecoder::normalize(std::uint64_t dif, unsigned rng) {
    const int d = 16 - static_cast<int>(std::bit_width(rng));
    const int cnt_before = cnt_;
    dif_ = dif << d;
    rng_ = rng << d;
    cnt_ = cnt_before - d;
    // 用无符号比较：cnt 为负时不应触发补充。
    if (static_cast<unsigned>(cnt_before) < static_cast<unsigned>(d)) refill();
}

unsigned RangeDecoder::decode_symbol(const std::uint16_t* cdf, int n) {
    // n 个符号只需 n - 1 个分界：cdf[0..n-2] 依次对应从上往下第 1..n-1 档，
    // 末符号的区间直接落到 0，不再读 cdf[n-1]。
    const unsigned c = static_cast<unsigned>(dif_ >> 48);
    const unsigned r_hi8 = rng_ >> 8;
    unsigned upper = rng_;
    for (int i = 0; i + 1 < n; ++i) {
        const unsigned v = range_split(r_hi8, cdf[i], static_cast<unsigned>(n - i));
        if (c >= v) {
            normalize(dif_ - (static_cast<std::uint64_t>(v) << 48), upper - v);
            return static_cast<unsigned>(i);
        }
        upper = v;
    }
    normalize(dif_, upper);
    return static_cast<unsigned>(n - 1);
}

int RangeDecoder::decode_bool(unsigned f_one) {
    const unsigned c = static_cast<unsigned>(dif_ >> 48);
    const unsigned v = ((rng_ >> 8) * (f_one >> 6) >> 1) + 4u;
    if (c >= v) {
        normalize(dif_ - (static_cast<std::uint64_t>(v) << 48), rng_ - v);
        return 0;
    }
    normalize(dif_, v);
    return 1;
}

}  // namespace termchart::image::codec::av1
