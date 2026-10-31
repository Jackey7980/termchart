// test_stb_image.cpp —— image 解码分发器对 JPEG/GIF/WebP 的两种状态
//
// 默认构建（TERMCHART_ENABLE_STB_IMAGE=OFF）：这三种格式识别得出但无解码器，
// 必须如实返回 ErrorCode::Unsupported（fail-loud，不静默降级）。
//
// 开启 stb_image 后：用 vendored stb_image_write 造一个真实 JPG 走 decode_image
// 回路，验证 stb 桥接（嗅探→decode_stb→Bitmap）；另内嵌一个 1×1 透明 GIF 验证
// stb 的 GIF 解码路径（GIF 只取首帧）。WebP 与 JPG/GIF 走同一 decode_stb，仅由
// 分发测试保证 fail-loud，解码路径不另作运行时样本。
//
// 注意：stb_image_write.h 会间接引入 C <math.h>→<cmath>；在 MinGW/GCC15 下若排在
// <vector>/doctest 之后会触发 libstdc++ __is_integer 错，故 stb 头必须最先包含。

#ifdef TERMCHART_HAVE_STB_IMAGE
#include <cmath>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/image/decoder.hpp"

namespace {

using namespace termchart;
using namespace termchart::image;

#ifndef TERMCHART_HAVE_STB_IMAGE
TEST_CASE("image/stb: 未启用 stb 时 JPG/GIF 如实 Unsupported") {
    // 仅给魔数、不是合法图像：识别得出格式但无解码器（JPEG/GIF 仍依赖可选 stb）
    // → Unsupported。WebP 现已原生零依赖解码，不在此列（其损坏契约由
    // test_webp_decode 覆盖）。
    const std::vector<std::uint8_t> jpeg = {0xFF, 0xD8, 0xFF, 0x00};
    const auto rj = decode_image(jpeg.data(), jpeg.size());
    CHECK_FALSE(rj.has_value());
    CHECK(rj.error() == ErrorCode::Unsupported);

    const std::vector<std::uint8_t> gif = {'G', 'I', 'F', '8'};
    const auto rg = decode_image(gif.data(), gif.size());
    CHECK_FALSE(rg.has_value());
    CHECK(rg.error() == ErrorCode::Unsupported);
}
#endif

#ifdef TERMCHART_HAVE_STB_IMAGE
// 经典 1×1 透明 GIF89a（43 字节）：黑底、透明索引=1、单像素索引 0（黑、不透明）。
const std::uint8_t kGif1x1[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61,  // "GIF89a"
    0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00,  // W=1 H=1 packed=0x80 bg=0 aspect=0
    0x00, 0x00, 0x00,                          // GCT[0] = 黑
    0xFF, 0xFF, 0xFF,                          // GCT[1] = 白
    0x21, 0xF9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,  // Graphic Control Extension
    0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00,                                // Image Descriptor
    0x02, 0x02, 0x44, 0x01, 0x00,        // LZW 数据（min=2, block=2, 44 01, term）
    0x3B                                 // Trailer
};

TEST_CASE("image/stb: 启用后 JPG 经 stb 桥接往返解码") {
    const int w = 4, h = 3;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w * 4 + x * 4);
            rgba[i + 0] = static_cast<std::uint8_t>(x * 60);
            rgba[i + 1] = static_cast<std::uint8_t>(y * 80);
            rgba[i + 2] = 128;
            rgba[i + 3] = 255;
        }
    }

    // stb_image_write 只给 JPEG 的"写文件 / 回调"两种入口，没有 *_to_mem；用回调
    // 把字节收进 vector，避免落临时文件。
    std::vector<std::uint8_t> jpg;
    const auto sink = [](void* ctx, void* data, int size) {
        auto* out = static_cast<std::vector<std::uint8_t>*>(ctx);
        const auto* p = static_cast<const std::uint8_t*>(data);
        out->insert(out->end(), p, p + size);
    };
    const int written =
        stbi_write_jpg_to_func(sink, &jpg, w, h, 4, rgba.data(), 90);
    REQUIRE(written != 0);
    REQUIRE(!jpg.empty());

    const auto img = decode_image(std::string_view{
        reinterpret_cast<const char*>(jpg.data()), jpg.size()});

    REQUIRE(img.has_value());
    CHECK(img->width() == static_cast<std::uint32_t>(w));
    CHECK(img->height() == static_cast<std::uint32_t>(h));
    CHECK(img->channels() == 4);  // stb 请求 4 通道 → RGBA
}

TEST_CASE("image/stb: 启用后内嵌 GIF 经 stb 解码（首帧）") {
    const auto img = decode_image(std::string_view{
        reinterpret_cast<const char*>(kGif1x1),
        sizeof(kGif1x1)});
    REQUIRE(img.has_value());
    CHECK(img->width() == 1);
    CHECK(img->height() == 1);
    CHECK(img->channels() == 4);
}
#endif

}  // namespace
