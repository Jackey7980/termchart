// test_media_registry.cpp —— 媒体层注册表与 gpa 原语回归。
//
// 锁定两件事：注册表名字/容器元数据是 C ABI 路由的唯一真源；gpa 模板的
// 逐像素语义与原内联表达式一致（含双 stride 与累加器类型）。
#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/media/image/png_encode.hpp"

#include "termchart/media/encoder/gpa/bit_ops.hpp"
#include "termchart/media/encoder/gpa/metrics.hpp"
#include "termchart/media/media.hpp"

using termchart::image::Bitmap;
using termchart::media::encoder::gpa::calc_sad;
using termchart::media::encoder::gpa::calc_ssd;

TEST_CASE("gpa: 限幅与位反转原语") {
    using termchart::media::encoder::gpa::clip3;
    using termchart::media::encoder::gpa::clip_pixel;
    using termchart::media::encoder::gpa::reverse_bits;

    CHECK(clip3(-2, 9, -5) == -2);
    CHECK(clip3(-2, 9, 12) == 9);
    CHECK(clip3(-2, 9, 3) == 3);
    CHECK(clip_pixel(-1) == 0);
    CHECK(clip_pixel(256) == 255);
    CHECK(clip_pixel(128) == 128);
    CHECK(reverse_bits(0b1011u, 4) == 0b1101u);
}

TEST_CASE("gpa: SSD/SAD 模板在双 stride 下逐像素语义不变") {
    constexpr int w = 4;
    constexpr int h = 3;
    constexpr int a_stride = 6;
    constexpr int b_stride = 5;
    std::array<std::uint8_t, a_stride * h> a{};
    std::array<std::uint8_t, b_stride * h> b{};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            a[static_cast<std::size_t>(y * a_stride + x)] = static_cast<std::uint8_t>(x + y);
            b[static_cast<std::size_t>(y * b_stride + x)] = static_cast<std::uint8_t>(x);
        }
    }
    // 每行差恒为 y：SSD = 4*(0+1+4)=20，SAD = 4*(0+1+2)=12。
    CHECK(calc_ssd<long long>(a.data(), a_stride, b.data(), b_stride, w, h) == 20);
    CHECK(calc_sad<long long>(a.data(), a_stride, b.data(), b_stride, w, h) == 12);
    CHECK(calc_ssd<std::uint64_t>(a.data(), a_stride, b.data(), b_stride, w, h) == 20u);
}

TEST_CASE("gpa: 扁平 SSD 与既有 AV1 入口一致") {
    const std::array<std::uint8_t, 4> a{0, 1, 2, 3};
    const std::array<std::uint8_t, 4> b{1, 1, 1, 1};
    CHECK(calc_ssd<std::uint64_t>(a.data(), b.data(), a.size()) == 6u);
    CHECK(termchart::media::encoder::gpa::ssd(a.data(), b.data(), a.size()) == 6u);
}

TEST_CASE("media: 注册表覆盖全部内置编码名与容器元数据") {
    termchart::media::register_all_builtin_codecs();
    for (const char* name : {"png", "jpeg", "jpg", "gif", "webp", "mp4", "av1", "ivf", "avif"}) {
        CHECK(termchart::media::has_encoder(name));
    }
    CHECK_FALSE(termchart::media::has_encoder("bmp"));
    CHECK(termchart::media::is_multiframe("mp4"));
    CHECK(termchart::media::is_multiframe("gif"));
    CHECK_FALSE(termchart::media::is_multiframe("png"));
    CHECK(termchart::media::extension_for("jpeg") == "jpg");
    CHECK(termchart::media::extension_for("av1") == "ivf");
}

TEST_CASE("media: 门面编码字节与底层编码器逐字节一致") {
    const Bitmap bmp(8, 8, 4);
    const std::array<const Bitmap, 1> frames{bmp};
    const std::string via_facade = termchart::media::encode(frames, "png");
    const std::string direct = termchart::image::encode_png(bmp);
    CHECK(via_facade == direct);
    CHECK(via_facade == termchart::image::encode(bmp, termchart::image::ImageFormat::Png));
    CHECK(termchart::media::encode(frames, "bmp").empty());
}
