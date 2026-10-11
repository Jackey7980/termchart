// test_diff_damage.cpp —— 差分层（纯函数 diff + 有状态 DamageTracker）
//
// 分工（别混淆，否则重绘范围会失控）：
//   * diff()：两个 buffer 之间"哪里变了"，纯函数、无状态；
//   * DamageTracker：本帧"累计脏区"，供 renderer 与将来的 canvas 部分重绘。
// 合并规则是性能契约：多写一个跳转字节比多写十个字节的样式重置便宜。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/render/damage.hpp"
#include "termchart/render/diff.hpp"

namespace {

using namespace termchart;
using namespace termchart::render;

CellBuffer filled(std::uint32_t width, std::uint32_t height) {
    CellBuffer buffer(width, height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            buffer.at(x, y) = Cell{'a' + static_cast<char>(x % 26)};
        }
    }
    return buffer;
}

}  // namespace

TEST_CASE("render: identical 逐单元比较，尺寸不同即不等") {
    const CellBuffer a = filled(4, 2);
    CellBuffer b = a;
    CHECK(identical(a, b));

    b.at(1, 1) = Cell{'z'};
    CHECK_FALSE(identical(a, b));

    CHECK_FALSE(identical(a, filled(5, 2)));
    CHECK_FALSE(identical(a, filled(4, 3)));
}

TEST_CASE("render: 无变化时 diff 返回空列表") {
    const CellBuffer a = filled(4, 2);
    const std::vector<DiffRegion> regions = diff(a, a);
    CHECK(regions.empty());
}

TEST_CASE("render: 单个变化单元 → 一段半开区间") {
    CellBuffer before = filled(8, 1);
    CellBuffer after = before;
    after.at(3, 0) = Cell{'Z'};

    const std::vector<DiffRegion> regions = diff(before, after);
    REQUIRE(regions.size() == 1);
    CHECK(regions[0].row == 0);
    CHECK(regions[0].begin == 3);
    CHECK(regions[0].end == 4);
}

TEST_CASE("render: 相邻变化并成一段") {
    CellBuffer before = filled(8, 1);
    CellBuffer after = before;
    after.at(2, 0) = Cell{'X'};
    after.at(3, 0) = Cell{'Y'};

    const std::vector<DiffRegion> regions = diff(before, after);
    REQUIRE(regions.size() == 1);
    CHECK(regions[0].begin == 2);
    CHECK(regions[0].end == 4);
}

TEST_CASE("render: 相隔不超过 merge_gap 的两段被合并") {
    CellBuffer before = filled(20, 1);
    CellBuffer after = before;
    after.at(2, 0) = Cell{'X'};
    after.at(6, 0) = Cell{'Y'};  // 间隔 3 个未变单元

    const std::vector<DiffRegion> merged = diff(before, after, 4);
    REQUIRE(merged.size() == 1);
    CHECK(merged[0].begin == 2);
    CHECK(merged[0].end == 7);

    const std::vector<DiffRegion> strict = diff(before, after, 1);
    REQUIRE(strict.size() == 2);
    CHECK(strict[0].begin == 2);
    CHECK(strict[0].end == 3);
    CHECK(strict[1].begin == 6);
    CHECK(strict[1].end == 7);
}

TEST_CASE("render: 不同行的变化不跨行合并") {
    CellBuffer before = filled(8, 3);
    CellBuffer after = before;
    after.at(1, 0) = Cell{'X'};
    after.at(1, 2) = Cell{'Y'};

    const std::vector<DiffRegion> regions = diff(before, after);
    REQUIRE(regions.size() == 2);
    CHECK(regions[0].row == 0);
    CHECK(regions[1].row == 2);
}

TEST_CASE("render: 尺寸变化不产出差分（调用方应走全量）") {
    const std::vector<DiffRegion> regions = diff(filled(4, 2), filled(5, 2));
    CHECK(regions.empty());
}

TEST_CASE("render: DamageTracker 单点标记与查询") {
    DamageTracker damage;
    damage.resize(8, 2);
    CHECK_FALSE(damage.dirty(3, 1));

    damage.mark(3, 1);
    CHECK(damage.dirty(3, 1));
    CHECK(damage.dirty_row(1));
    CHECK_FALSE(damage.dirty_row(0));
}

TEST_CASE("render: DamageTracker 合并相邻与重叠区间") {
    DamageTracker damage;
    damage.resize(16, 1);

    damage.mark_run(0, 2, 4);
    damage.mark_run(0, 4, 6);  // 相邻
    CHECK(damage.run_count() == 1);
    CHECK(damage.runs_of(0)[0].begin == 2);
    CHECK(damage.runs_of(0)[0].end == 6);

    damage.mark_run(0, 10, 12);
    CHECK(damage.run_count() == 2);  // 与 [2,6) 不相邻：新开一段

    damage.mark_run(0, 5, 11);  // 跨接两段 → 并成一段
    CHECK(damage.run_count() == 1);
    CHECK(damage.runs_of(0)[0].begin == 2);
    CHECK(damage.runs_of(0)[0].end == 12);
}

TEST_CASE("render: DamageTracker mark_row / mark_all") {
    DamageTracker damage;
    damage.resize(4, 3);

    damage.mark_row(2);
    CHECK(damage.dirty(0, 2));
    CHECK(damage.dirty(3, 2));
    CHECK_FALSE(damage.dirty(0, 1));

    damage.mark_all();
    CHECK(damage.dirty(0, 0));
    CHECK(damage.dirty(3, 2));
    CHECK(damage.run_count() == 3);
}

TEST_CASE("render: DamageTracker runs 按行升序、clear 清空") {
    DamageTracker damage;
    damage.resize(4, 3);
    damage.mark(2, 2);
    damage.mark(1, 0);

    const std::vector<DamageTracker::RowRun> runs = damage.runs();
    REQUIRE(runs.size() == 2);
    CHECK(runs[0].row == 0);
    CHECK(runs[1].row == 2);

    damage.clear();
    CHECK(damage.run_count() == 0);
    CHECK_FALSE(damage.dirty(1, 0));
}

TEST_CASE("render: DamageTracker resize 后尺寸生效且脏区清空") {
    DamageTracker damage;
    damage.resize(4, 2);
    damage.mark(1, 1);
    CHECK(damage.run_count() == 1);

    damage.resize(10, 5);
    CHECK(damage.width() == 10);
    CHECK(damage.height() == 5);
    CHECK(damage.run_count() == 0);
}
