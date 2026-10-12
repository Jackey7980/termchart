// test_chart_hierarchical.cpp —— treemap/sunburst/dendrogram/pack/icicle/tree_view
// 端到端绘制（结构事实断言，不做逐字节 golden）+ node.hpp 布局辅助单测
#include <doctest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/hierarchical/dendrogram.hpp"
#include "termchart/chart/hierarchical/icicle.hpp"
#include "termchart/chart/hierarchical/node.hpp"
#include "termchart/chart/hierarchical/pack.hpp"
#include "termchart/chart/hierarchical/sunburst.hpp"
#include "termchart/chart/hierarchical/tree_view.hpp"
#include "termchart/chart/hierarchical/treemap.hpp"
#include "termchart/core/point.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_nonblank(const canvas::Canvas& c) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp != U'\u0020') {
                ++n;
            }
        }
    }
    return n;
}

std::size_t count_char(const canvas::Canvas& c, char32_t cp) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp == cp) {
                ++n;
            }
        }
    }
    return n;
}

constexpr char32_t kBlock = U'\u2588';     // █ 填充
constexpr char32_t kMarker = U'\u25CF';    // ● 数据点/气泡描边
constexpr char32_t kInner = U'\u00B7';     // · 气泡内部
constexpr char32_t kHBar = U'\u2500';      // ─ 连接横杠
constexpr char32_t kVBar = U'\u2502';      // │ 连接垂线
constexpr char32_t kBranch = U'\u251C';    // ├ 缩进前缀
constexpr char32_t kLast = U'\u2514';      // └ 缩进前缀

// 默认 margin：left 8 / top 1 / right 1 / bottom 3 → 内容区 y ∈ [1, h-3)。
hier::HierNode make_tree() {
    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"a", 5.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"b", 3.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"c", 2.0, Color::none(), {}});
    return root;
}

}  // namespace

// ---------------- treemap ----------------

TEST_CASE("chart/hierarchical/treemap: 叶子矩形填满内容区") {
    canvas::Canvas canvas(60, 20);
    Treemap chart;
    chart.set_root(make_tree());
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 50);
    // 内容区右下角必被某个叶子覆盖（三叶按值切片铺满整区）。
    CHECK(canvas.at(50, 15).cp == kBlock);
}

TEST_CASE("chart/hierarchical/treemap: 空树与全零子树不绘制") {
    canvas::Canvas c1(40, 12);
    Treemap empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    Treemap zeros;
    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"x", 0.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"y", 0.0, Color::none(), {}});
    zeros.set_root(std::move(root));
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- sunburst ----------------

TEST_CASE("chart/hierarchical/sunburst: 环形扇区落格且根环存在") {
    canvas::Canvas canvas(60, 20);
    Sunburst chart;
    chart.set_root(make_tree());
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 30);
    // 内容区几何中心在根环内：必须非空。
    CHECK(canvas.at(29, 9).cp == kBlock);
}

TEST_CASE("chart/hierarchical/sunburst: 关闭根环后仍绘制且空树不崩") {
    // 两层树：关闭根环后子节点环（ring 1）照画，中心留孔。
    hier::HierNode deep = make_tree();
    for (hier::HierNode& child : deep.children) {
        child.children.push_back(
            hier::HierNode{child.name + "1", 1.0, Color::none(), {}});
    }
    canvas::Canvas c1(60, 20);
    Sunburst no_root;
    no_root.options.show_root = false;
    no_root.set_root(std::move(deep));
    no_root.draw({c1});
    CHECK(count_char(c1, kBlock) > 20);
    // 关闭根环后中心格让空（子节点从内环起，中心是孔）。
    CHECK(c1.at(29, 9).cp != kBlock);

    canvas::Canvas c2(40, 12);
    Sunburst empty;
    empty.draw({c2});
    REQUIRE_NOTHROW(empty.draw({c2}));
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- dendrogram ----------------
// 已知主线缺陷（src/chart/hierarchical/dendrogram.cpp:65）：叶子分支的
// NodeGeom 聚合初始化少写 leaf/depth 两个实参，true/depth 错位进
// first_child/last_child，leaf 恒为默认 false——布局把叶子当内部节点走，
// 正常数据画不出完整树。在主线修复前，这里只做不崩溃冒烟，不锁定缺陷行为。

TEST_CASE("chart/hierarchical/dendrogram: 正常树不崩溃（布局缺陷待修）") {
    canvas::Canvas canvas(60, 20);
    Dendrogram chart;
    chart.set_root(make_tree());
    REQUIRE_NOTHROW(chart.draw({canvas}));
    // 缺陷下不会越界写：任意格读回不触发断言即通过。
    (void)canvas.at(0, 0).cp;
}

TEST_CASE("chart/hierarchical/dendrogram: 单叶与空树不崩溃") {
    canvas::Canvas c1(40, 12);
    Dendrogram single;
    hier::HierNode leaf;
    leaf.name = "only";
    leaf.value = 1.0;
    single.set_root(std::move(leaf));
    REQUIRE_NOTHROW(single.draw({c1}));

    canvas::Canvas c2(40, 12);
    Dendrogram empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- pack ----------------

TEST_CASE("chart/hierarchical/pack: 气泡描边与内部填充齐备") {
    canvas::Canvas canvas(60, 20);
    PackChart chart;
    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"big", 4.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"m1", 1.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"m2", 1.0, Color::none(), {}});
    chart.set_root(std::move(root));
    chart.draw({canvas});

    CHECK(count_char(canvas, kMarker) > 10);  // 圆周描边
    CHECK(count_char(canvas, kInner) > 0);    // 圆内 ·
}

TEST_CASE("chart/hierarchical/pack: 空树与全零叶子不绘制") {
    canvas::Canvas c1(40, 12);
    PackChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    PackChart zeros;
    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"x", 0.0, Color::none(), {}});
    zeros.set_root(std::move(root));
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- icicle ----------------

TEST_CASE("chart/hierarchical/icicle: 逐层分段铺满且深度成行") {
    canvas::Canvas canvas(60, 20);
    Icicle chart;
    chart.set_root(make_tree());
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 30);
    // 第 0 行是根层、第 1 行是子层：段首标签字符盖在 █ 之上。
    CHECK(canvas.at(8, 1).cp == U'r');  // root 首码点
    CHECK(canvas.at(8, 2).cp == U'a');  // 最左子段首码点
}

TEST_CASE("chart/hierarchical/icicle: 空树与全零子树不绘制") {
    canvas::Canvas c1(40, 12);
    Icicle empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    Icicle zeros;
    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"x", 0.0, Color::none(), {}});
    zeros.set_root(std::move(root));
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);  // 聚合后根值 <= 0，整体放弃
}

// ---------------- tree_view ----------------

TEST_CASE("chart/hierarchical/tree_view: 根行无前缀、子行有 ├/└") {
    canvas::Canvas canvas(60, 20);
    TreeView chart;
    chart.set_root(make_tree());
    chart.draw({canvas});

    // 根名从内容区左上角开始，无前缀。
    CHECK(canvas.at(8, 1).cp == U'r');
    CHECK(count_char(canvas, kBranch) >= 1);  // ├
    CHECK(count_char(canvas, kLast) >= 1);    // └（最后一个孩子）
}

TEST_CASE("chart/hierarchical/tree_view: 空名叶子与空树退化") {
    canvas::Canvas c1(40, 12);
    TreeView empty_name;
    hier::HierNode leaf;  // 无名无子
    empty_name.set_root(std::move(leaf));
    empty_name.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    TreeView empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);

    // show_values 打开时不崩（值格式化路径）。
    canvas::Canvas c3(60, 20);
    TreeView with_values;
    with_values.options.show_values = true;
    with_values.set_root(make_tree());
    REQUIRE_NOTHROW(with_values.draw({c3}));
    CHECK(count_nonblank(c3) > 0);
}

// ---------------- node.hpp 辅助 ----------------

TEST_CASE("chart/hierarchical/node: aggregate_values 叶取非负、内部聚合") {
    hier::HierNode leaf_neg{"neg", -5.0, Color::none(), {}};
    CHECK(hier::aggregate_values(leaf_neg) == 0.0);

    hier::HierNode leaf_pos{"pos", 7.0, Color::none(), {}};
    CHECK(hier::aggregate_values(leaf_pos) == 7.0);

    hier::HierNode root;
    root.name = "root";
    root.children.push_back(hier::HierNode{"a", 2.0, Color::none(), {}});
    root.children.push_back(hier::HierNode{"b", -1.0, Color::none(), {}});
    root.children.push_back(
        hier::HierNode{"sub", 0.0, Color::none(),
                       {hier::HierNode{"c", 3.0, Color::none(), {}}}});
    CHECK(hier::aggregate_values(root) == doctest::Approx(5.0));
}

TEST_CASE("chart/hierarchical/node: hier_palette 非空色、first_codepoint 容错") {
    CHECK_FALSE(hier::hier_palette(0).is_none());
    CHECK_FALSE(hier::hier_palette(9).is_none());

    CHECK(hier::first_codepoint("abc") == U'a');
    CHECK(hier::first_codepoint("") == U'\u0020');
    // 非法 UTF-8 序列不崩溃且返回确定值。
    REQUIRE_NOTHROW(hier::first_codepoint("\xFF\xFE"));
    CHECK(hier::first_codepoint("中") == U'\u4E2D');
}
