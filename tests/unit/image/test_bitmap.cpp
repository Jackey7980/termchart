// test_bitmap.cpp —— Bitmap（RGBA 像素缓冲）不变量
#include <doctest.h>

#include "termchart/image/bitmap.hpp"

namespace {

using namespace termchart::image;

}  // namespace

TEST_CASE("bitmap: 尺寸 / 通道 / 行步长") {
    const Bitmap b(4, 3, 4);
    CHECK(b.width() == 4);
    CHECK(b.height() == 3);
    CHECK(b.channels() == 4);
    CHECK(b.row_bytes() == 16);          // 4 像素 × 4 通道
    CHECK(b.size() == 16u * 3u);
    CHECK_FALSE(b.empty());
}

TEST_CASE("bitmap: 像素读写与越界") {
    Bitmap b(2, 1, 4);
    b.set(0, 0, 0, 10);
    b.set(0, 0, 3, 200);
    CHECK(b.at(0, 0, 0) == 10);
    CHECK(b.at(0, 0, 3) == 200);
    // 越界读返回 0，写静默忽略
    CHECK(b.at(5, 5, 0) == 0);
    b.set(5, 5, 0, 99);
    CHECK(b.at(1, 0, 0) == 0);
}

TEST_CASE("bitmap: fill 覆盖全图并保留 alpha") {
    Bitmap b(2, 2, 4);
    b.fill(1, 2, 3, 128);
    CHECK(b.at(0, 0, 0) == 1);
    CHECK(b.at(1, 1, 1) == 2);
    CHECK(b.at(1, 1, 2) == 3);
    CHECK(b.at(1, 1, 3) == 128);
}

TEST_CASE("bitmap: 3 通道不带 alpha") {
    Bitmap b(1, 1, 3);
    CHECK(b.channels() == 3);
    CHECK(b.row_bytes() == 3);
    b.fill(7, 8, 9);
    CHECK(b.at(0, 0, 2) == 9);
    CHECK(b.at(0, 0, 3) == 0);  // 无第 4 通道
}

TEST_CASE("bitmap: 相等比较考虑尺寸与像素") {
    Bitmap a(2, 1, 4);
    Bitmap b(2, 1, 4);
    CHECK(a == b);
    b.set(1, 0, 2, 5);
    CHECK(a != b);
}
