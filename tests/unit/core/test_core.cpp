// test_core.cpp —— core 层基础类型
#include <doctest.h>

#include <type_traits>

#include "termchart/core/arena.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/span_utils.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

TEST_CASE("core: Rgb 与十六进制互转") {
    constexpr Rgb color = Rgb::from_hex(0x2F6F4F);
    CHECK(color.r == 0x2F);
    CHECK(color.g == 0x6F);
    CHECK(color.b == 0x4F);
    CHECK(color.to_hex() == 0x2F6F4F);

    SUBCASE("相等与不等") {
        CHECK(Rgb{1, 2, 3} == Rgb{1, 2, 3});
        CHECK(Rgb{1, 2, 3} != Rgb{1, 2, 4});
    }
    SUBCASE("亮度排序符合直觉") {
        CHECK(kBlack.luma() < kWhite.luma());
    }
}

TEST_CASE("core: Rgb 编译期字面量") {
    // 整数形式与 "#RRGGBB" 必须等价（两条路径都过 from_hex / 手写解析）。
    constexpr Rgb red = 0xFF0000_rgb;
    constexpr Rgb red_str = "#FF0000"_hex;
    static_assert(red == red_str);
    static_assert(red.r == 0xFF && red.g == 0x00 && red.b == 0x00);
    CHECK(red.to_hex() == 0xFF0000u);

    // '#' 可省；大小写不敏感。
    constexpr Rgb teal = "00ff88"_hex;
    static_assert(teal == Rgb{0x00, 0xFF, 0x88});
    CHECK(teal == Rgb{0x00, 0xFF, 0x88});

    // 3 位缩写按 nibble 展开：#F80 → #FF8800（若忘了 ×17 会退化成 #0F8000）。
    constexpr Rgb orange = "#F80"_hex;
    static_assert(orange == Rgb{0xFF, 0x88, 0x00});
    CHECK(orange.to_hex() == 0xFF8800u);

    // 全零 / 全白边界。
    static_assert("#000"_hex == kBlack);
    static_assert(0xFFFFFF_rgb == kWhite);

    // 类型契约：字面量再怎么加，Rgb 仍是 3 字节、可平凡复制、标准布局。
    static_assert(sizeof(Rgb) == 3);
    static_assert(std::is_trivially_copyable_v<Rgb>);
    static_assert(std::is_standard_layout_v<Rgb>);
}

TEST_CASE("core: ColorDepth 可比较且计数正确") {
    CHECK(ColorDepth::Ansi256 > ColorDepth::Ansi16);
    CHECK(ColorDepth::TrueColor >= ColorDepth::Ansi256);
    CHECK(color_depth_count(ColorDepth::Ansi16) == 16);
    CHECK(color_depth_count(ColorDepth::Ansi256) == 256);
    CHECK(color_depth_count(ColorDepth::TrueColor) == 16777216);
}

TEST_CASE("core: 属性位运算") {
    AttrMask mask = Attr::Bold | Attr::Underline;
    CHECK(has_flag(mask, Attr::Bold));
    CHECK(has_flag(mask, Attr::Underline));
    CHECK_FALSE(has_flag(mask, Attr::Italic));

    const AttrMask removed = without(mask, Attr::Bold);
    CHECK_FALSE(has_flag(removed, Attr::Bold));
    CHECK(has_flag(removed, Attr::Underline));
}

TEST_CASE("core: Point / Rect 几何") {
    const Rect box{10.0, 10.0, 20.0, 10.0};
    CHECK(box.right() == 30.0);
    CHECK(box.bottom() == 20.0);
    CHECK(box.contains(Point{15.0, 15.0}));
    CHECK_FALSE(box.contains(Point{30.0, 15.0}));  // 右边界为开区间
    CHECK(box.center() == Point{20.0, 15.0});

    SUBCASE("相交与包围盒") {
        const Rect other{25.0, 15.0, 20.0, 10.0};
        REQUIRE(box.intersects(other));
        CHECK(box.intersection(other) == Rect{25.0, 15.0, 5.0, 5.0});
        CHECK(box.union_with(other) == Rect{10.0, 10.0, 35.0, 15.0});
    }
    SUBCASE("空矩形参与并集时返回另一方") {
        CHECK(Rect{}.union_with(box) == box);
        CHECK(box.union_with(Rect{}) == box);
        CHECK(Rect{}.empty());
    }
}

TEST_CASE("core: span 安全切片") {
    int data[5] = {0, 1, 2, 3, 4};
    const std::span<int> all(data, 5);
    CHECK(safe_subspan(all, 2, 2).size() == 2);
    CHECK(safe_subspan(all, 4, 10).size() == 1);   // 钳位
    CHECK(safe_subspan(all, 9, 1).size() == 0);    // 越界起点
    CHECK_FALSE(checked_at(std::span<const int>(data, 5), 9).has_value());
    CHECK(checked_at(std::span<const int>(data, 5), 1).has_value());
}

TEST_CASE("core: Style 语义样式") {
    Style style;
    CHECK(style.attrs() == kNoAttrs);
    CHECK(style.fg().is_none());

    style.set_fg(Color::rgb(255, 0, 0)).set_attr(Attr::Bold).set_attr(Attr::Italic);
    CHECK(style.has_attr(Attr::Bold));
    CHECK(style.has_attr(Attr::Italic));
    CHECK(style.fg().kind() == ColorKind::Rgb);
    CHECK(style.fg().rgb() == Rgb{255, 0, 0});

    SUBCASE("相同构造得到相等样式") {
        Style other;
        other.set_fg(Color::rgb(255, 0, 0)).set_attr(Attr::Bold).set_attr(Attr::Italic);
        CHECK(style == other);
    }
    SUBCASE("不同颜色种类即使值接近也不相等") {
        Style other = style;
        other.set_fg(Color::indexed(1));
        CHECK(style != other);
    }
}

TEST_CASE("core: Cell 逐字段比较与空白判定") {
    const Cell plain{'A'};
    CHECK(is_blank(Cell{}));
    CHECK_FALSE(is_blank(plain));

    Cell styled{'B', Style().set_attr(Attr::Bold)};
    CHECK(plain != styled);
    CHECK(Cell{'A'} == plain);
}

TEST_CASE("core: CellBuffer 网格与侧面表") {
    CellBuffer buffer(4, 2);
    CHECK(buffer.width() == 4);
    CHECK(buffer.height() == 2);
    CHECK(buffer.size() == 8);

    buffer.at(1, 1) = Cell{'X'};
    CHECK(buffer.at(1, 1).cp == U'X');
    CHECK(buffer.index_of(1, 1) == 5);

    SUBCASE("行 span 与线性索引一致") {
        const std::span<Cell> second_row = buffer.row(1);
        REQUIRE(second_row.size() == 4);
        CHECK(second_row[1].cp == U'X');
    }

    SUBCASE("侧面表是稀疏的") {
        CHECK(buffer.side_entry_count() == 0);
        const char32_t sides[2] = {U'\u0301', U'\u0327'};
        buffer.set_sides(buffer.index_of(0, 0), sides, 2);
        CHECK(buffer.side_entry_count() == 1);
        REQUIRE(buffer.sides(buffer.index_of(0, 0)) != nullptr);
        CHECK((*buffer.sides(buffer.index_of(0, 0)))[0] == U'\u0301');
        CHECK(buffer.sides(buffer.index_of(1, 1)) == nullptr);

        buffer.clear_sides(buffer.index_of(0, 0));
        CHECK(buffer.side_entry_count() == 0);
    }

    SUBCASE("resize 使旧侧面表失效") {
        const char32_t sides[1] = {U'\u0301'};
        buffer.set_sides(0, sides, 1);
        buffer.resize(8, 4);
        CHECK(buffer.side_entry_count() == 0);
        CHECK(buffer.size() == 32);
    }

    SUBCASE("相等性包含尺寸与内容") {
        CellBuffer copy(4, 2);
        CHECK(copy != buffer);  // 内容不同
        copy.at(1, 1) = Cell{'X'};
        CHECK(copy == buffer);
    }
}

TEST_CASE("core: Arena 帧分配器") {
    Arena arena(64);
    CHECK(arena.block_count() == 1);

    int* first = arena.create<int>(7);
    REQUIRE(first != nullptr);
    CHECK(*first == 7);
    CHECK(arena.used_bytes() > 0);

    SUBCASE("超出初始容量会开新块") {
        void* large = arena.allocate(4096, alignof(std::max_align_t));
        REQUIRE(large != nullptr);
        CHECK(arena.block_count() >= 2);
        CHECK(arena.capacity_bytes() >= 4096);
    }

    SUBCASE("reset 复用内存") {
        const std::size_t before = arena.capacity_bytes();
        arena.reset();
        CHECK(arena.used_bytes() == 0);
        CHECK(arena.capacity_bytes() == before);
    }
}

}  // namespace
