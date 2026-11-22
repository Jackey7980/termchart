#include "range_coder.hpp"

#include <bit>

namespace termchart::image::codec::av1 {
namespace {

// 就地把 1 加到 pos 处的字节，并向更低的地址传播进位（libaom propagate_carry_bwd）。
void propagate_carry_bwd(std::vector<std::uint8_t>& buf, std::size_t pos) {
    while (true) {
        const unsigned sum = static_cast<unsigned>(buf[pos]) + 1u;
        buf[pos] = static_cast<std::uint8_t>(sum & 0xFFu);
        if ((sum >> 8) == 0 || pos == 0) return;
        --pos;
    }
}

}  // namespace

void RangeEncoder::normalize(std::uint64_t low, unsigned rng) {
    // rng 归一化到 [32768, 65535] 所需的左移位数
    const int d = 16 - static_cast<int>(std::bit_width(rng));
    int s = cnt_ + d;
    // libaom 只在 low 无法再容纳下一个符号（最多 15 位）时才刷字节：
    // s >= 40 意味着 low 里已攒够 5 个整字节，再留 1 字节给进位。
    if (s >= 40) {
        const unsigned num_bytes_ready = static_cast<unsigned>((s >> 3) + 1);
        const int c = cnt_ + 24 - static_cast<int>(num_bytes_ready << 3);
        std::uint64_t output = low >> c;
        low &= (std::uint64_t{1} << c) - 1;
        const std::uint64_t carry_mask = std::uint64_t{1} << (num_bytes_ready << 3);
        const std::uint64_t carry = output & carry_mask;
        output &= carry_mask - 1;
        const std::size_t offs = buf_.size();
        for (unsigned i = num_bytes_ready; i-- > 0;)
            buf_.push_back(static_cast<std::uint8_t>((output >> (i * 8)) & 0xFFu));
        if (carry != 0 && offs > 0) propagate_carry_bwd(buf_, offs - 1);
        s = (s & 7) - 8;
    }
    low_ = low << d;
    rng_ = rng << d;
    cnt_ = s;
}

void RangeEncoder::write_cdf(const std::uint16_t* cdf, int n, int symbol) {
    if (finished_ || n <= 1) return;
    if (symbol < 0) symbol = 0;
    if (symbol >= n) symbol = n - 1;

    const unsigned r = rng_;
    const unsigned N = static_cast<unsigned>(n - 1);
    const unsigned r8 = r >> 8;
    // 与 dav1d 的解码循环完全对齐：第 i 档上界 u、下界 v，
    // v = range_split(r, cdf[i], N - i)，首符号上界就是 r，末符号下界为 0。
    const unsigned u =
        symbol == 0 ? r : range_split(r8, cdf[symbol - 1], N - symbol + 1);
    const unsigned v =
        symbol + 1 < n ? range_split(r8, cdf[symbol], N - symbol) : 0u;
    normalize(low_ + (r - u), u - v);
}

void RangeEncoder::write_bool(int bit, unsigned f_one) {
    const unsigned v = ((rng_ >> 8) * (f_one >> 6) >> 1) + 4u;  // bit == 1 的区间长度
    if (finished_ || v == 0 || v >= rng_) return;
    normalize(low_ + (bit != 0 ? rng_ - v : 0u), bit != 0 ? v : rng_ - v);
}

std::string RangeEncoder::finish() {
    if (finished_) return result_;

    // 与 libaom od_ec_enc_done 完全一致：s = 10 + cnt 是还需要写出的位数，
    // e 的第 14 位置 1 作为终止位，剩余位全 0，正好满足 decodeframe.c 里
    // check_trailing_bits_after_symbol_coder 的「1 后全 0」校验。
    int c = cnt_;
    int s = 10 + c;
    const std::uint64_t m = 0x3FFF;
    std::uint64_t e = ((low_ + m) & ~m) | (m + 1);
    if (s > 0) {
        std::uint64_t n = (std::uint64_t{1} << (c + 16)) - 1;
        do {
            const std::uint16_t val = static_cast<std::uint16_t>(e >> (c + 16));
            const std::size_t offs = buf_.size();
            buf_.push_back(static_cast<std::uint8_t>(val & 0xFFu));
            if ((val & 0x100u) != 0 && offs > 0) propagate_carry_bwd(buf_, offs - 1);
            e &= n;
            s -= 8;
            c -= 8;
            n >>= 8;
        } while (s > 0);
    }

    result_.reserve(buf_.size());
    for (auto b : buf_) result_.push_back(static_cast<char>(b));
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
        const unsigned v = range_split(r_hi8, cdf[i], static_cast<unsigned>(n - 1 - i));
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
