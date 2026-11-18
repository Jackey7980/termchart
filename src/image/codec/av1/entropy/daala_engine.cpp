#include "daala_engine.hpp"

#include <algorithm>

namespace termchart::image::codec::av1 {
namespace {

constexpr std::uint32_t kMinRange = 1u << 15;
constexpr std::uint32_t kLowMask = (1u << 24) - 1u;

std::uint32_t av1_split(std::uint32_t range, int prob15, int min_prob) {
    const int p = std::max(0, std::min(32768, prob15));
    return (((range >> 8) * static_cast<std::uint32_t>(p >> 6)) >> 1) +
           static_cast<std::uint32_t>(std::max(0, min_prob));
}

}  // namespace

void DaalaEncoder::emit(std::uint8_t byte, int carry) {
    const int v = static_cast<int>(byte) + carry;
    if (v > 0xff) {
        // 0xff + 进位：此前挂起的 0xff 全部变 0x00，本字节也变 0x00。
        while (pending_ff_ > 0) {
            bytes_.push_back(0x00);
            --pending_ff_;
        }
        bytes_.push_back(0x00);
        return;
    }
    if (v == 0xff) {
        ++pending_ff_;
        return;
    }
    while (pending_ff_ > 0) {
        bytes_.push_back(0xff);
        --pending_ff_;
    }
    bytes_.push_back(static_cast<std::uint8_t>(v));
}

void DaalaEncoder::shift_one() {
    const std::uint32_t carry = low_ >> 24;
    const auto byte = static_cast<std::uint8_t>((low_ >> 16) & 0xffu);
    low_ = (low_ << 8) & kLowMask;
    range_ <<= 8;
    emit(byte, static_cast<int>(carry));
}

void DaalaEncoder::write_bit(int bit, int prob15, int min_prob) {
    if (finished_) return;
    const std::uint32_t split = av1_split(range_, prob15, min_prob);
    if (bit != 0) {
        low_ += range_ - split;
        range_ = split;
    } else {
        range_ -= split;
    }
    while (range_ < kMinRange) shift_one();
}

std::string DaalaEncoder::finish() {
    if (!finished_) {
        // 用 4 字节把区间钉死；与解码端的 3 字节预取 + 1 字节余量对应。
        for (int i = 0; i < 4; ++i) shift_one();
        while (pending_ff_ > 0) {
            bytes_.push_back(0xff);
            --pending_ff_;
        }
        finished_ = true;
    }
    return std::string(bytes_.begin(), bytes_.end());
}

bool DaalaDecoder::init(const std::uint8_t* data, std::size_t size) {
    data_ = data;
    size_ = size;
    pos_ = 0;
    low_ = 0;
    range_ = 1u << 16;
    code_ = 0;
    // 预取 3 字节构成 24 位码字窗口；不足的字节按 0 处理。
    for (int i = 0; i < 3; ++i) code_ = (code_ << 8) | next_byte();
    return size_ > 0;
}

std::uint8_t DaalaDecoder::next_byte() {
    if (pos_ >= size_) return 0;
    return data_[pos_++];
}

bool DaalaDecoder::read_bit(int prob15, int& bit, int min_prob) {
    if (range_ == 0) return false;
    const std::uint32_t split = av1_split(range_, prob15, min_prob);
    const std::uint32_t upper = range_ - split;  // 0 的一侧区间长度
    if (code_ >= low_ + upper) {
        bit = 1;
        low_ += upper;
        range_ = split;
    } else {
        bit = 0;
        range_ = upper;
    }
    while (range_ < kMinRange) {
        low_ = (low_ << 8) & kLowMask;
        range_ <<= 8;
        code_ = ((code_ << 8) | next_byte()) & kLowMask;
    }
    return true;
}

}  // namespace termchart::image::codec::av1
