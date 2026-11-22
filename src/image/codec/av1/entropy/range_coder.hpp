// range_coder.hpp —— AV1 二进制区间编码 / 解码核心（15 位概率，规范分区规则）
//
// 分界值统一按规范计算：
//   split = ((range >> 8) * (cdf >> 6) >> 1) + 4 * remaining
// remaining 是「当前分界之上还剩的符号数」，保证每个符号至少分到 4 个区间单位。
//
// 编码端是 libaom `od_ec_enc` 的逐位等价移植：low 记录区间**下界**（从底部量），
// rng 归一化到 [32768, 65535]，cnt 记录 low 中的有效位数（初值 -9）。
// s = cnt + d >= 40 时刷出 (s>>3)+1 个整字节并就地向前传播进位；收尾按
// e = ((low + 0x3FFF) & ~0x3FFF) | 0x4000 输出「1 后全 0」的终止位模式。
// 这两条是 AV1 规范 5.3.3/5.3.4 与 decodeframe.c 的
// check_trailing_bits_after_symbol_coder 共同要求的，不能凭区间包含关系近似。
// 解码端用 64 位窗口保存「到区间顶部的距离」，流耗尽后按补零处理。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_RANGE_CODER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_RANGE_CODER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace termchart::image::codec::av1 {

// 规范分界值：r_hi8 是当前 range 的高 8 位，cdf 是 Q15 逆累积值。
inline unsigned range_split(unsigned r_hi8, unsigned cdf, unsigned remaining) {
    return (r_hi8 * (cdf >> 6) >> 1) + 4u * remaining;
}

class RangeEncoder {
public:
    // 多符号写入；cdf 为 Q15 逆累积表（cdf[0..n-2] 递减，cdf[n-1] 视为 0）。
    void write_cdf(const std::uint16_t* cdf, int n, int symbol);

    // 布尔符号：f_one 是 bit == 1 的概率（Q15，1..32767）。
    void write_bool(int bit, unsigned f_one);

    // 刷出剩余区间，返回完整字节流；重复调用返回同一结果。
    std::string finish();

    unsigned range() const noexcept { return rng_; }

private:
    void normalize(std::uint64_t low, unsigned rng);

    std::vector<std::uint8_t> buf_;  // 已固化的整字节（进位就地回传）
    std::uint64_t low_ = 0;
    unsigned rng_ = 0x8000;
    int cnt_ = -9;  // low 中的有效位数，初值 -9（跨过零位后即一个字节 + 一个进位位）
    bool finished_ = false;
    std::string result_;
};

class RangeDecoder {
public:
    bool init(const std::uint8_t* data, std::size_t size);

    // 多符号解码；cdf 为 Q15 逆累积表（cdf[0..n-1] 递减，cdf[n-1] == 0）。
    unsigned decode_symbol(const std::uint16_t* cdf, int n);

    // 布尔解码；f_one 是 bit == 1 的概率（Q15）。
    int decode_bool(unsigned f_one);

private:
    void refill();
    void normalize(std::uint64_t dif, unsigned rng);

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
    std::uint64_t dif_ = 0;  // 64 位码字窗口
    unsigned rng_ = 0x8000;
    int cnt_ = -15;  // 窗口内已确定但尚未消费的位数
};

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_RANGE_CODER_HPP
