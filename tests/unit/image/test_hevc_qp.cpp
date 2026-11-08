// test_hevc_qp.cpp —— 真压缩档 slice QP 配置链路回归测试
//
// 锁定三件事：
//   1. 默认 QP=26 时码流与历史字节级一致（slice_qp_delta=0，se(0)=1 bit）；
//   2. QP 变化会写进 slice header 的 slice_qp_delta（否则解码端按 26 反量化，
//      编码端重建与 ffmpeg 解码会发散 —— 曾踩过的坑）；
//   3. QP 22/26/30/34 体积单调不增（ROADMAP:484 出口判据）。
//
// 本测试不依赖 ffmpeg；像素级/硬解判据由 tools/verify_hevc_hwaccel.py 覆盖。
#include <doctest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/hevc_encode.hpp"

namespace {

using namespace termchart::image;

Bitmap make_frame(int w, int h) {
    Bitmap b(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int v = (x * 200) / (w - 1) + 20 + (((x / 8) + (y / 8)) & 1 ? 14 : -14);
            const auto c = static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
            b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, c);
            b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, c);
            b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, c);
        }
    return b;
}

std::string encode_qp(const Bitmap& b, int qp) {
    const std::vector<Bitmap> frames = {b};
    EncodeOptions opts;
    opts.video_mode = VideoEncodeMode::Intra;
    opts.video_qp = qp;
    return encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
}

// 找 IDR slice NAL（type 19）的 RBSP 起点，跳过防竞争字节。
const std::uint8_t* find_idr_payload(const std::string& bs, std::size_t* len) {
    for (std::size_t i = 0; i + 4 < bs.size(); ++i) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(bs.data());
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1 && ((p[i + 3] >> 1) & 0x3F) == 19) {
            *len = bs.size() - (i + 5);
            return p + i + 5;
        }
    }
    return nullptr;
}

// 从 slice header 起点读 ue(v) / se(v) 序列，取到 slice_qp_delta。
// 布局：first_slice(1) no_output(1) ue(pps_id) ue(slice_type) se(qp_delta)。
int read_slice_qp_delta(const std::string& bs) {
    std::size_t len = 0;
    const std::uint8_t* p = find_idr_payload(bs, &len);
    if (!p || len < 4) return 9999;
    std::size_t bit = 0;
    const auto get = [&]() -> int {
        return (p[bit >> 3] >> (7 - (bit++ & 7))) & 1;
    };
    const auto ue = [&]() -> std::uint32_t {
        int zeros = 0;
        while (get() == 0) ++zeros;
        std::uint32_t v = 1;
        for (int i = 0; i < zeros; ++i) v = (v << 1) | static_cast<std::uint32_t>(get());
        return v - 1;
    };
    get();                       // first_slice_segment_in_pic_flag
    get();                       // no_output_of_prior_pics_flag
    ue();                        // slice_pic_parameter_set_id
    ue();                        // slice_type
    const std::uint32_t k = ue();  // slice_qp_delta 的 se(v) 映射值
    return (k & 1) ? static_cast<int>((k + 1) / 2) : -static_cast<int>(k / 2);
}

}  // namespace

TEST_CASE("hevc intra: default QP=26 keeps slice_qp_delta=0") {
    const auto b = make_frame(128, 64);
    const auto bs = encode_qp(b, 26);
    REQUIRE_FALSE(bs.empty());
    CHECK(read_slice_qp_delta(bs) == 0);
}

TEST_CASE("hevc intra: video_qp writes matching slice_qp_delta") {
    const auto b = make_frame(128, 64);
    for (const int qp : {22, 26, 30, 34}) {
        const auto bs = encode_qp(b, qp);
        REQUIRE_FALSE(bs.empty());
        CHECK(read_slice_qp_delta(bs) == qp - 26);
    }
}

TEST_CASE("hevc intra: QP is clamped to [0,51] in the bitstream") {
    const auto b = make_frame(128, 64);
    const auto lo = encode_qp(b, -10);
    const auto hi = encode_qp(b, 100);
    REQUIRE_FALSE(lo.empty());
    REQUIRE_FALSE(hi.empty());
    CHECK(read_slice_qp_delta(lo) == -26);
    CHECK(read_slice_qp_delta(hi) == 25);
}

TEST_CASE("hevc intra: bitstream size decreases monotonically over QP 22/26/30/34") {
    const auto b = make_frame(128, 64);
    std::size_t prev = 0;
    for (const int qp : {22, 26, 30, 34}) {
        const auto bs = encode_qp(b, qp);
        REQUIRE_FALSE(bs.empty());
        if (prev) CHECK(bs.size() <= prev);
        prev = bs.size();
    }
}

TEST_CASE("hevc pcm: video_qp does not alter the golden I_PCM bitstream") {
    const auto b = make_frame(128, 64);
    const std::vector<Bitmap> frames = {b};
    EncodeOptions a;
    a.video_mode = VideoEncodeMode::Pcm;
    EncodeOptions c = a;
    c.video_qp = 10;  // Pcm 档必须忽略 QP
    CHECK(encode_hevc_annexb(std::span<const Bitmap>{frames}, a) ==
          encode_hevc_annexb(std::span<const Bitmap>{frames}, c));
}
