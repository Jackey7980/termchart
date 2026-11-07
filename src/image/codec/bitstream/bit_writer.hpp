// bit_writer.hpp —— H.264 / HEVC 共用的比特流基建（MSB-first）
//
// 设计约束：
//   * 只做「位打包 + 指数哥伦布 + RBSP 收尾 + 防竞争 + Annex-B 起始码」，
//     不含任何 H.264/H.265 语义，两个编解码器与容器层都可以直接复用。
//   * 高位在前（H.264 的 RBSP、HEVC 的 slice_segment_header/SEI 都是 MSB-first）。
//   * 不抛异常、不依赖库内部类型，header-only + inline。
#ifndef TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_BIT_WRITER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_BIT_WRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace termchart::image::codec::bitstream {

// ── MSB-first 位写入器 ─────────────────────────────────────────────────
// 用法：u()/ue()/se() 追加字段，align_zero() 只补零，byte_align() 写停止位再补零
// （即 RBSP 的 rbsp_trailing_bits），finish() = byte_align() + 取出字节串。
class BitWriter {
public:
    void put_bit(int b) {
        cur_ = (cur_ << 1) | (b & 1);
        if (++nbits_ == 8) {
            buf_.push_back(static_cast<char>(cur_));
            cur_ = 0;
            nbits_ = 0;
        }
    }

    // 定长无符号，高位在前。
    void u(std::uint32_t value, int len) {
        for (int i = len - 1; i >= 0; --i) put_bit(static_cast<int>((value >> i) & 1u));
    }

    // 无符号指数哥伦布 ue(v)：H.264 9.1 / H.265 9.2 同构。
    void ue(std::uint32_t v) {
        const std::uint32_t code = v + 1;
        int n = 0;
        for (std::uint32_t t = code; t > 1; t >>= 1) ++n;
        for (int i = 0; i < n; ++i) put_bit(0);
        for (int i = n; i >= 0; --i) put_bit(static_cast<int>((code >> i) & 1u));
    }

    // 有符号指数哥伦布 se(v)。
    void se(int v) {
        const std::uint32_t k =
            (v <= 0) ? static_cast<std::uint32_t>(-2 * v) : static_cast<std::uint32_t>(2 * v - 1);
        ue(k);
    }

    // 只补零到字节边界（pcm_alignment_zero_bit / byte_alignment 的补零段）。
    void align_zero() {
        while (nbits_ != 0) put_bit(0);
    }

    // rbsp_trailing_bits：先写 stop bit(1) 再补零对齐。
    void byte_align() {
        put_bit(1);
        align_zero();
    }

    std::string finish() {
        byte_align();
        return std::move(buf_);
    }

    // 只取已完成的整字节，不写停止位（供需要自定义收尾的调用方）。
    std::string take_bytes() { return std::move(buf_); }

    const std::string& data() const noexcept { return buf_; }
    int pending_bits() const noexcept { return nbits_; }  // 当前未落盘的位数
    std::size_t bit_size() const noexcept { return buf_.size() * 8 + static_cast<std::size_t>(nbits_); }

private:
    std::string buf_;
    int cur_ = 0;    // 正在累积的字节
    int nbits_ = 0;  // 已累积位数（0..7）
};

// ── 防竞争字节（emulation prevention）─────────────────────────────────
// 00 00 后紧跟 ≤0x03 的字节时插入 0x03。H.264 7.4.1 / H.265 7.4.2 同构。
inline std::string emulation_prevention(const std::string& rbsp) {
    std::string out;
    out.reserve(rbsp.size() + rbsp.size() / 64 + 4);
    int zeros = 0;
    for (char ch : rbsp) {
        const int c = static_cast<unsigned char>(ch);
        if (zeros >= 2 && c <= 3) {
            out.push_back(static_cast<char>(3));
            zeros = 0;
        }
        out.push_back(ch);
        zeros = (c == 0) ? zeros + 1 : 0;
    }
    return out;
}

// ── Annex-B 封装 ──────────────────────────────────────────────────────
// header 是 NAL 头的原始字节（H.264 1 字节，H.265 2 字节），先插起始码再写头，
// 最后追加防竞争处理过的载荷。
inline std::string annexb_nal(const std::uint8_t* header, std::size_t hn, const std::string& rbsp) {
    std::string out;
    out.reserve(hn + rbsp.size() + 8);
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(1));  // start code 00 00 00 01
    for (std::size_t i = 0; i < hn; ++i) out.push_back(static_cast<char>(header[i]));
    out += emulation_prevention(rbsp);
    return out;
}

// 8bit 饱和（重建/色彩转换共用）。
inline int clamp_u8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

}  // namespace termchart::image::codec::bitstream

#endif  // TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_BIT_WRITER_HPP
