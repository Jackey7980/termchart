// test_render_compositor.cpp —— 多图层合成（render/compositor）
//
// 图层是 GUI / 重叠元素 / 多 Pass 渲染的公共底座，此前零测试。这里钉住覆盖次序、
// transparent 的空格透出语义、偏移定位与自动扩容包围盒。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/core/buffer.hpp"
#include "termchart/render/compositor.hpp"

namespace {

using namespace termchart;
using namespace termchart::render;

CellBuffer filled(std::uint32_t width, std::uint32_t height, char32_t cp) {
    CellBuffer buffer(width, height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            buffer.at(x, y) = Cell{cp, Style{}};
        }
    }
    return buffer;
}

}  // namespace

TEST_CASE("render/compositor: 后绘制的图层覆盖先绘制的") {
    const CellBuffer bottom = filled(4, 4, U'A');
    const CellBuffer top = filled(2, 2, U'B');

    CellBuffer out(4, 4);
    Compositor::composite({{&bottom, 0, 0, true}, {&top, 0, 0, true}}, out);
    CHECK(out.at(0, 0).cp == U'B');
    CHECK(out.at(3, 3).cp == U'A');
}

TEST_CASE("render/compositor: transparent 决定空格是否透出下层") {
    const CellBuffer bottom = filled(2, 2, U'A');
    CellBuffer top = filled(2, 2, U' ');
    top.at(1, 1) = Cell{U'B', Style{}};

    CellBuffer see_through(2, 2);
    Compositor::composite({{&bottom, 0, 0, true}, {&top, 0, 0, true}}, see_through);
    CHECK(see_through.at(0, 0).cp == U'A');  // 上层空格 → 透出
    CHECK(see_through.at(1, 1).cp == U'B');  // 上层非空格 → 覆盖

    CellBuffer opaque(2, 2);
    Compositor::composite({{&bottom, 0, 0, true}, {&top, 0, 0, false}}, opaque);
    CHECK(opaque.at(0, 0).cp == U' ');       // 不透明 → 空格也盖住
    CHECK(opaque.at(1, 1).cp == U'B');
}

TEST_CASE("render/compositor: 偏移定位，负偏移只画落在画布内的部分") {
    const CellBuffer top = filled(2, 2, U'B');

    CellBuffer shifted(4, 4);
    Compositor::composite({{&top, 1, 1, true}}, shifted);
    CHECK(shifted.at(1, 1).cp == U'B');
    CHECK(shifted.at(2, 2).cp == U'B');
    CHECK(shifted.at(0, 0).cp != U'B');

    CellBuffer clipped(4, 4);
    Compositor::composite({{&top, -1, -1, true}}, clipped);
    CHECK(clipped.at(0, 0).cp == U'B');  // 层的 (1,1) 落到画布 (0,0)
    CHECK(clipped.at(1, 1).cp != U'B');
}

TEST_CASE("render/compositor: 自动扩容重载按包围盒分配") {
    const CellBuffer a = filled(2, 2, U'A');
    const CellBuffer b = filled(2, 2, U'B');

    const CellBuffer out = Compositor::composite({{&a, 0, 0, true}, {&b, 1, 1, true}});
    CHECK(out.width() == 3);
    CHECK(out.height() == 3);
    CHECK(out.at(0, 0).cp == U'A');
    CHECK(out.at(2, 2).cp == U'B');
}

TEST_CASE("render/compositor: 空图层与空指针图层不崩") {
    const CellBuffer out = Compositor::composite({});
    CHECK(out.width() == 0);
    CHECK(out.height() == 0);

    const CellBuffer bottom = filled(2, 2, U'A');
    CellBuffer target(2, 2);
    Compositor::composite({{nullptr, 0, 0, true}, {&bottom, 0, 0, true}}, target);
    CHECK(target.at(0, 0).cp == U'A');
}
