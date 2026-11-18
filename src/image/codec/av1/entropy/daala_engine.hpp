// daala_engine.hpp —— AV1 二进制算术编解码引擎（9.3；第一阶段静态概率）
//
// 概率取值 1..254（8bit），split = 1 + ((range-1)*prob >> 8)。区间下界 low
// 保持 24 位精度，range 低于 256 时重归一化并输出一个字节；0xff 挂起以处理进位。
// 这一层只负责「同一概率序列下编码/解码严格互逆」，CDF 表由 cdf_manager 提供
// （第二阶段接入 15 位 CDF → 8 位概率的换算与自适应更新）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_DAALA_ENGINE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_DAALA_ENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace termchart::image::codec::av1 {

class DaalaEncoder {
public:
    // AV1 原生 15 位概率；min_prob 为 EC_MIN_PROB×剩余符号数（布尔符号取 4）。
    void write_bit(int bit, int prob15, int min_prob = 4);

    // 刷出剩余区间字节；可重复调用（多次调用不再产出新字节）。
    std::string finish();

    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    void emit(std::uint8_t byte, int carry);
    void shift_one();

    std::vector<std::uint8_t> bytes_;
    std::uint32_t low_ = 0;
    std::uint32_t range_ = 1u << 16;
    int pending_ff_ = 0;
    bool finished_ = false;
};

class DaalaDecoder {
public:
    bool init(const std::uint8_t* data, std::size_t size);

    // 与 DaalaEncoder::write_bit 严格互逆。
    bool read_bit(int prob15, int& bit, int min_prob = 4);

private:
    std::uint8_t next_byte();

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
    std::uint32_t low_ = 0;
    std::uint32_t range_ = 1u << 16;
    std::uint32_t code_ = 0;  // 当前 24 位码字窗口
};

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_DAALA_ENGINE_HPP
