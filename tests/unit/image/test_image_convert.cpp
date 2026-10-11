// test_image_convert.cpp —— 位图合成原语与"位图 ↔ 标量场"桥接
#include <doctest.h>

#include <cstdint>

#include "termchart/core/rgb.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/convert.hpp"
#include "termchart/image/field.hpp"

namespace {

using namespace termchart;
using namespace termchart::image;

Bitmap solid(std::uint32_t w, std::uint32_t h, std::uint8_t channels, std::uint8_t r,
             std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    Bitmap bitmap(w, h, channels);
    bitmap.fill(r, g, b, a);
    return bitmap;
}

}  // namespace

TEST_CASE("image/convert: 亮度用 Rec.601，透明像素乘进去变成 0") {
    Bitmap bitmap(3, 1, 4);
    bitmap.set(0, 0, 0, 255);  // 白
    bitmap.set(0, 0, 1, 255);
    bitmap.set(0, 0, 2, 255);
    bitmap.set(0, 0, 3, 255);

    bitmap.set(1, 0, 0, 0);  // 黑
    bitmap.set(1, 0, 1, 0);
    bitmap.set(1, 0, 2, 0);
    bitmap.set(1, 0, 3, 255);

    bitmap.set(2, 0, 0, 255);  // 白但全透明
    bitmap.set(2, 0, 1, 255);
    bitmap.set(2, 0, 2, 255);
    bitmap.set(2, 0, 3, 0);

    const FloatField field = luminance_field(bitmap);
    CHECK(field.width() == 3);
    CHECK(field.height() == 1);
    CHECK(field.at(0, 0) == doctest::Approx(1.0f));
    CHECK(field.at(1, 0) == doctest::Approx(0.0f));
    CHECK(field.at(2, 0) == doctest::Approx(0.0f));  // 透明不是黑斑，而是"没有内容"

    // 纯红：0.299
    const Bitmap red = solid(1, 1, 3, 255, 0, 0);
    CHECK(luminance_field(red).at(0, 0) == doctest::Approx(0.299f).epsilon(0.01));
}

TEST_CASE("image/convert: 带目标尺寸的转换先做最近邻缩放") {
    const Bitmap bitmap = solid(2, 2, 3, 10, 20, 30);
    const FloatField field = luminance_field(bitmap, 4, 4);
    CHECK(field.width() == 4);
    CHECK(field.height() == 4);
    CHECK(field.at(0, 0) == field.at(3, 3));  // 纯色放大后处处相同

    // 已是目标尺寸时不缩放（值应逐格保留）
    Bitmap gradient(2, 1, 3);   // 左黑右白
    gradient.set(0, 0, 0, 0);
    gradient.set(0, 0, 1, 0);
    gradient.set(0, 0, 2, 0);
    gradient.set(1, 0, 0, 255);
    gradient.set(1, 0, 1, 255);
    gradient.set(1, 0, 2, 255);
    const FloatField same = luminance_field(gradient, 2, 1);
    CHECK(same.at(0, 0) == doctest::Approx(0.0f));
    CHECK(same.at(1, 0) == doctest::Approx(1.0f));
}

TEST_CASE("image/convert: 场按阈值转双色位图，可选 alpha 取场值") {
    FloatField field(2, 1);
    field.set(0, 0, 0.9f);
    field.set(1, 0, 0.1f);

    const Rgb on{255, 255, 255};
    const Rgb off{0, 0, 0};
    const Bitmap bitmap = field_to_bitmap(field, on, off);
    CHECK(bitmap.channels() == 4);
    CHECK(bitmap.at(0, 0, 0) == 255);
    CHECK(bitmap.at(1, 0, 0) == 0);
    CHECK(bitmap.at(0, 0, 3) == 255);  // 默认不透明

    const Bitmap masked = field_to_bitmap(field, on, off, 0.5, true);
    CHECK(masked.at(0, 0, 3) == 230);  // 0.9 * 255 = 229.5 → 230
    CHECK(masked.at(1, 0, 3) == 26);   // 0.1 * 255 = 25.5 → 26
}

TEST_CASE("image/bitmap: fill_rect 与通道转换") {
    Bitmap bitmap(4, 3, 4);
    bitmap.fill_rect(1, 1, 2, 2, 7, 8, 9, 128);
    CHECK(bitmap.at(0, 0, 0) == 0);
    CHECK(bitmap.at(1, 1, 0) == 7);
    CHECK(bitmap.at(2, 2, 3) == 128);
    // 越界部分被裁剪，且不写坏内存
    bitmap.fill_rect(3, 2, 10, 10, 1, 2, 3, 255);
    CHECK(bitmap.at(3, 2, 0) == 1);

    const Bitmap rgb = solid(2, 1, 3, 1, 2, 3);
    const Bitmap rgba = rgb.with_alpha(200);
    CHECK(rgba.channels() == 4);
    CHECK(rgba.at(1, 0, 3) == 200);
    CHECK(rgba.with_alpha(1).channels() == 4);  // 已是 RGBA：原样返回
    const Bitmap back = rgba.without_alpha();
    CHECK(back.channels() == 3);
    CHECK(back.at(0, 0, 2) == 3);
}

TEST_CASE("image/bitmap: blit_over 做 source-over 混合并裁剪偏移") {
    Bitmap target = solid(2, 2, 4, 0, 0, 0, 255);

    Bitmap half = solid(1, 1, 4, 255, 255, 255, 128);
    target.blit_over(half, 0, 0);
    // 白色 50% 覆盖纯黑：结果约 128
    CHECK(target.at(0, 0, 0) == doctest::Approx(128).epsilon(0.02));

    // 全透明源不改变目标
    Bitmap invisible = solid(1, 1, 4, 255, 0, 0, 0);
    target.blit_over(invisible, 1, 1);
    CHECK(target.at(1, 1, 0) == 0);

    // 负偏移：只画落在图内的部分
    Bitmap big = solid(2, 2, 4, 9, 9, 9, 255);
    target.blit_over(big, -1, -1);
    CHECK(target.at(0, 0, 0) == 9);  // 源的 (1,1) 落到 (0,0)

    // 无 alpha 的源直接忽略（调用方须先 with_alpha）
    Bitmap opaque_rgb = solid(1, 1, 3, 200, 200, 200);
    target.blit_over(opaque_rgb, 0, 0);
    CHECK(target.at(0, 0, 0) == 9);
}

TEST_CASE("image/bitmap: scaled_to 用最近邻，不引入新颜色") {
    Bitmap bitmap(2, 1, 3);
    bitmap.set(0, 0, 0, 10);
    bitmap.set(1, 0, 0, 200);

    const Bitmap wide = bitmap.scaled_to(4, 1);
    CHECK(wide.width() == 4);
    CHECK(wide.at(0, 0, 0) == 10);
    CHECK(wide.at(1, 0, 0) == 10);
    CHECK(wide.at(2, 0, 0) == 200);
    CHECK(wide.at(3, 0, 0) == 200);  // 最近邻：只有原图出现过的两种值

    const Bitmap tall = bitmap.scaled_to(2, 3);
    CHECK(tall.height() == 3);
    CHECK(tall.at(0, 2, 0) == 10);

    CHECK(bitmap.scaled_to(0, 0).empty());
}
