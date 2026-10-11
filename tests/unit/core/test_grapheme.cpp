// test_grapheme.cpp —— core::GraphemeCluster 只定义结构，不做分割
//
// 分层约定的可执行断言：本用例**不**涉及任何分割算法（那是 util/grapheme.hpp）。
#include <doctest.h>

#include "termchart/core/grapheme.hpp"

namespace {

using namespace termchart;

TEST_CASE("core: GraphemeCluster 主码点与侧面项") {
    GraphemeCluster cluster(U'e');
    CHECK(cluster.base() == U'e');
    CHECK(cluster.side_count() == 0);
    CHECK_FALSE(cluster.has_sides());

    CHECK(cluster.add_side_bearing(U'\u0301'));
    CHECK(cluster.add_side_bearing(U'\u0327'));
    CHECK(cluster.side_count() == 2);
    CHECK(cluster.sides()[0] == U'\u0301');
    CHECK(cluster.sides()[1] == U'\u0327');
}

TEST_CASE("core: 侧面项达到上限后追加失败而非增长") {
    GraphemeCluster cluster(U'a');
    for (std::size_t i = 0; i < kMaxSideBearings; ++i) {
        REQUIRE(cluster.add_side_bearing(static_cast<char32_t>(U'\u0300' + i)));
    }
    CHECK(cluster.side_count() == kMaxSideBearings);
    // 结构定长：超出即丢弃，绝不分配堆内存（否则 CellBuffer 侧面包会被拖垮）。
    CHECK_FALSE(cluster.add_side_bearing(U'\u0301'));
    CHECK(cluster.side_count() == kMaxSideBearings);
}

TEST_CASE("core: 簇相等性比较主码点与全部侧面项") {
    GraphemeCluster lhs(U'e');
    lhs.add_side_bearing(U'\u0301');

    GraphemeCluster rhs(U'e');
    rhs.add_side_bearing(U'\u0301');
    CHECK(lhs == rhs);

    rhs.add_side_bearing(U'\u0327');
    CHECK(lhs != rhs);

    GraphemeCluster different_base(U'a');
    different_base.add_side_bearing(U'\u0301');
    CHECK(lhs != different_base);
}

TEST_CASE("core: clear_sides 只清侧面不动主码点") {
    GraphemeCluster cluster(U'x');
    cluster.add_side_bearing(U'\u0301');
    cluster.clear_sides();
    CHECK(cluster.side_count() == 0);
    CHECK(cluster.base() == U'x');
}

}  // namespace
