// test_image_export.cpp —— 统一编码分发器 + api::save_image 高层入口闭环
#include <cstdint>
#include <cstdio>
#include <string>

#include <doctest.h>

#include "termchart/api/export_image.hpp"
#include "termchart/core/error.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

using namespace termchart;
using namespace termchart::image;

namespace {

Bitmap sample() {
    Bitmap bm(16, 16, 3);
    for (std::uint32_t y = 0; y < 16; ++y)
        for (std::uint32_t x = 0; x < 16; ++x) {
            bm.set(x, y, 0, static_cast<std::uint8_t>(x * 16));
            bm.set(x, y, 1, static_cast<std::uint8_t>(y * 16));
            bm.set(x, y, 2, 128);
        }
    return bm;
}

const char* kTmpPng = "tc_export_test.png";
const char* kTmpJpg = "tc_export_test.jpg";

long file_size(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return -1;
    }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fclose(f);
    return sz;
}

}  // namespace

TEST_CASE("export: encode 分发器按格式产出非空字节") {
    const auto bm = sample();
    const auto png = image::encode(bm, image::ImageFormat::Png);
    const auto jpg = image::encode(bm, image::ImageFormat::Jpeg);
    CHECK_FALSE(png.empty());
    CHECK_FALSE(jpg.empty());
    CHECK(png[0] == '\x89');  // PNG 签名首字节
    CHECK(static_cast<std::uint8_t>(jpg[0]) == 0xFF);
    CHECK(static_cast<std::uint8_t>(jpg[1]) == 0xD8);
    // 空位图 → 空串（与 encode_png/encode_jpeg 约定一致）
    CHECK(image::encode(Bitmap{}, image::ImageFormat::Png).empty());
}

TEST_CASE("export: api::save_image 写盘并返回 Ok") {
    const auto bm = sample();

    const Status sp = api::save_image(bm, kTmpPng, image::ImageFormat::Png);
    CHECK(sp == ErrorCode::Ok);
    CHECK(file_size(kTmpPng) > 0);
    std::remove(kTmpPng);

    const Status sj = api::save_image(bm, kTmpJpg, image::ImageFormat::Jpeg);
    CHECK(sj == ErrorCode::Ok);
    CHECK(file_size(kTmpJpg) > 0);
    std::remove(kTmpJpg);
}

TEST_CASE("export: 空位图 save_image 返回 InvalidArgument") {
    CHECK(api::save_image(Bitmap{}, kTmpPng, image::ImageFormat::Png) == ErrorCode::InvalidArgument);
}
