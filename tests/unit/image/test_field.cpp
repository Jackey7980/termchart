// test_field.cpp —— FloatField（标量强度场，raster 的输入）存储不变量
#include <doctest.h>

#include "termchart/media/image/field.hpp"

namespace {

using namespace termchart::image;

}  // namespace

TEST_CASE("field: 构造尺寸与初值") {
    const FloatField f(4, 3, 0.5f);
    CHECK(f.width() == 4);
    CHECK(f.height() == 3);
    CHECK(f.at(0, 0) == doctest::Approx(0.5f));
    CHECK(f.at(3, 2) == doctest::Approx(0.5f));
}

TEST_CASE("field: 越界读返回 0") {
    const FloatField f(2, 2, 1.0f);
    CHECK(f.at(2, 0) == doctest::Approx(0.0f));
    CHECK(f.at(0, 2) == doctest::Approx(0.0f));
}

TEST_CASE("field: 写入与 fill") {
    FloatField f(2, 2);
    f.set(1, 1, 0.25f);
    CHECK(f.at(1, 1) == doctest::Approx(0.25f));
    f.fill(0.9f);
    CHECK(f.at(0, 0) == doctest::Approx(0.9f));
    CHECK(f.at(1, 1) == doctest::Approx(0.9f));
}

TEST_CASE("field: resize 重置尺寸与值") {
    FloatField f(2, 2, 1.0f);
    f.resize(3, 1, 0.0f);
    CHECK(f.width() == 3);
    CHECK(f.height() == 1);
    CHECK(f.at(2, 0) == doctest::Approx(0.0f));
}
