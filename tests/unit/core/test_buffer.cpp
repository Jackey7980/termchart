// test_buffer.cpp —— CellBuffer：全库中枢结构的 invariants
//
// CellBuffer 是终端实时 / snapshot / diff 三条路径共同消费的结构，GUI 平行栈
// 也要复用它的「网格 + 稀疏侧面表」模型。这里钉死它的存储不变量：索引、空白、
// 续接、侧面表稀疏性、resize 清空、等价比较。
#include <doctest.h>

#include <cstdint>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/grapheme.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

CellBuffer make(std::uint32_t w, std::uint32_t h) {
    CellBuffer buffer(w, h);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            buffer.at(x, y) = Cell{static_cast<char32_t>('a' + ((x + y) % 26)),
                                    Style{}.set_fg(Color::ansi16(1))};
        }
    }
    return buffer;
}

}  // namespace

TEST_CASE("buffer: 构造即空白网格，尺寸正确") {
    const CellBuffer buffer(4, 2);
    CHECK(buffer.width() == 4);
    CHECK(buffer.height() == 2);
    CHECK(buffer.size() == 8);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            CHECK(is_blank(buffer.at(x, y)));
        }
    }
    CHECK(buffer.side_entry_count() == 0);
}

TEST_CASE("buffer: 索引按 y*width + x") {
    const CellBuffer buffer(4, 3);
    for (std::uint32_t y = 0; y < 3; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            CHECK(buffer.index_of(x, y) == static_cast<std::size_t>(y) * 4 + x);
        }
    }
    CHECK_FALSE(buffer.contains(4, 0));
    CHECK_FALSE(buffer.contains(0, 3));
    CHECK(buffer.contains(3, 2));
}

TEST_CASE("buffer: 写入后 is_blank 变假，clear 复位") {
    CellBuffer buffer(2, 1);
    buffer.at(0, 0) = Cell{U'X', Style{}};
    CHECK_FALSE(is_blank(buffer.at(0, 0)));
    CHECK(is_blank(buffer.at(1, 0)));
    buffer.clear();
    CHECK(is_blank(buffer.at(0, 0)));
}

TEST_CASE("buffer: 侧面表稀疏，跨 resize 清空") {
    CellBuffer buffer(4, 1);
    char32_t mark[termchart::kMaxSideBearings] = {U'\u0301'};
    buffer.set_sides(buffer.index_of(1, 0), mark, 1);
    CHECK(buffer.has_sides(buffer.index_of(1, 0)));
    CHECK(buffer.side_entry_count() == 1);

    buffer.clear_sides(buffer.index_of(1, 0));
    CHECK_FALSE(buffer.has_sides(buffer.index_of(1, 0)));
    CHECK(buffer.side_entry_count() == 0);

    buffer.set_sides(buffer.index_of(2, 0), mark, 1);
    buffer.resize(8, 2);  // 不同尺寸：旧侧面表必须作废
    CHECK(buffer.side_entry_count() == 0);
    CHECK(buffer.has_sides(buffer.index_of(2, 0)) == false);
}

TEST_CASE("buffer: 等价比较只看单元与侧面") {
    CellBuffer a = make(3, 2);
    CellBuffer b = make(3, 2);
    CHECK(a == b);
    CHECK_FALSE(a != b);

    b.at(1, 1) = Cell{U'z', Style{}};
    CHECK(a != b);

    CellBuffer different_size = make(4, 2);
    CHECK(a != different_size);
}

TEST_CASE("buffer: 续接格是普通单元，可被写入与读回") {
    CellBuffer buffer(2, 1);
    buffer.at(0, 0) = Cell{U'\u4e2d'};
    buffer.at(1, 0) = Cell{kContinuationCp, Style{}};
    CHECK(is_continuation(buffer.at(1, 0)));
    CHECK(buffer.at(0, 0).cp == U'\u4e2d');
}

TEST_CASE("buffer: 拷贝独立于源，改动不互相影响") {
    CellBuffer a = make(3, 2);
    CellBuffer b = a;
    b.at(0, 0) = Cell{U'q', Style{}};
    CHECK(a.at(0, 0).cp != U'q');
    CHECK(b.at(0, 0).cp == U'q');
}
