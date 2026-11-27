// test_av1_encoder.cpp —— 真实像素输入帧内编码器的确定性 / 选项 / 尺寸门禁。
// 跨解码器逐像素一致性由 tools/verify_av1_minimal.py 的 FRAME ORACLE 覆盖。
#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

#include "src/image/av1_encode.hpp"
#include "src/image/codec/av1/encoder/intra_encoder.hpp"

namespace image = termchart::image;

namespace {

image::Bitmap texture(int w, int h) {
    image::Bitmap bitmap(static_cast<std::uint32_t>(w),
                         static_cast<std::uint32_t>(h), 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                       static_cast<std::uint8_t>((x * 7 + y * 3) & 0xFF));
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                       static_cast<std::uint8_t>((y * 5 + 64) & 0xFF));
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                       static_cast<std::uint8_t>((x ^ y) & 0xFF));
        }
    }
    return bitmap;
}

}  // namespace

TEST_CASE("av1 encoder: RGB 转 BT.601 全范围 4:2:0") {
    image::Bitmap red(2, 2, 3);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 2; ++x) {
            red.set(x, y, 0, 255);
            red.set(x, y, 1, 0);
            red.set(x, y, 2, 0);
        }
    }
    const auto frame = termchart::image::codec::av1::bitmap_to_yuv420(red);
    REQUIRE(frame.y.size() == 4);
    REQUIRE(frame.u.size() == 1);
    REQUIRE(frame.v.size() == 1);
    for (auto v : frame.y) CHECK(v == 77);
    CHECK(frame.u[0] == 85);
    CHECK(frame.v[0] == 255);
}

TEST_CASE("av1 encoder: 非整超级块尺寸显式拒绝") {
    CHECK(image::encode_av1_ivf(texture(16, 16)).empty());
    CHECK(image::encode_av1_ivf(texture(64, 32)).empty());
    image::Bitmap empty;
    CHECK(image::encode_av1_ivf(empty).empty());
}

TEST_CASE("av1 encoder: 同输入同选项码流确定") {
    const image::Bitmap src = texture(64, 64);
    image::Av1FrameYuv a, b;
    const std::string first = image::encode_av1_ivf(src, {}, &a);
    const std::string second = image::encode_av1_ivf(src, {}, &b);
    REQUIRE(first.size() > 32);
    CHECK(first == second);
    CHECK(a.y.size() == 64 * 64);
    CHECK(a.u.size() == 32 * 32);
    CHECK(a.v.size() == 32 * 32);
    CHECK(a.y == b.y);
    CHECK(a.u == b.u);
    CHECK(a.v == b.v);
    CHECK(first[0] == 'D');
    CHECK(first[1] == 'K');
    CHECK(first[2] == 'I');
    CHECK(first[3] == 'F');
}

TEST_CASE("av1 encoder: RDO 与划分决策改变码流") {
    const image::Bitmap src = texture(64, 64);
    image::Av1FrameEncodeOptions fixed;
    fixed.rdo_mode = false;
    fixed.rdo_partition = false;
    image::Av1FrameEncodeOptions no_split;
    no_split.rdo_partition = false;

    const std::string rdo = image::encode_av1_ivf(src);
    const std::string fixed_stream = image::encode_av1_ivf(src, fixed);
    const std::string no_split_stream = image::encode_av1_ivf(src, no_split);
    REQUIRE(rdo.size() > 32);
    CHECK(fixed_stream != rdo);
    CHECK(no_split_stream != rdo);
}

TEST_CASE("av1 encoder: 未闭合的二维放大帧显式拒绝") {
    image::Av1FrameEncodeOptions options;
    CHECK(options.prediction_only);
    CHECK(image::encode_av1_ivf(texture(128, 128), options).empty());
    CHECK(!image::encode_av1_ivf(texture(128, 64), options).empty());
}
