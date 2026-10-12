// test_chart_text.cpp —— wordcloud/tag_cloud/table/progress 端到端绘制
// （结构事实断言，不做逐字节 golden）
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/text/progress.hpp"
#include "termchart/chart/text/table.hpp"
#include "termchart/chart/text/tag_cloud.hpp"
#include "termchart/chart/text/wordcloud.hpp"
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

constexpr char32_t kFill = U'\u2588';    // █ 液条完成段
constexpr char32_t kEmptyBar = U'\u2591';  // ░ 液条未完成段
constexpr char32_t kSepV = U'\u2502';    // │ 列分隔
constexpr char32_t kSepT = U'\u253C';    // ├ 表头分隔线端点
constexpr char32_t kSepH = U'\u2500';    // ─ 表头分隔线

}  // namespace

// ---------------- wordcloud ----------------

TEST_CASE("chart/text/wordcloud: 词全部落位且 placed_count 记账") {
    canvas::Canvas canvas(60, 20);
    Wordcloud chart;
    chart.add_word("alpha", 5.0);
    chart.add_word("beta", 3.0);
    chart.add_word("gamma", 2.0);
    chart.draw({canvas});

    CHECK(chart.placed_count() == 3);
    CHECK(count_nonblank(canvas) > 0);
}

TEST_CASE("chart/text/wordcloud: 空文本与非法权重被滤除") {
    canvas::Canvas canvas(60, 20);
    Wordcloud chart;
    chart.add_word("", 5.0);
    chart.add_word("ghost", 0.0);
    chart.add_word("nan", std::numeric_limits<double>::quiet_NaN());
    chart.add_word("inf", std::numeric_limits<double>::infinity());
    chart.draw({canvas});

    CHECK(chart.placed_count() == 0);
    CHECK(count_nonblank(canvas) == 0);
}

TEST_CASE("chart/text/wordcloud: 词宽超过网格时整词放弃不越界") {
    canvas::Canvas canvas(6, 5);  // 内容区宽收敛到 0：任何词都放不下
    Wordcloud chart;
    chart.add_word("alpha", 5.0);
    chart.draw({canvas});

    CHECK(chart.placed_count() == 0);
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- tag_cloud ----------------

TEST_CASE("chart/text/tag_cloud: 权重降序流式排版") {
    canvas::Canvas canvas(40, 14);
    TagCloud chart;
    chart.add_tag("one", 3.0);
    chart.add_tag("two", 2.0);
    chart.add_tag("three", 1.0);
    chart.draw({canvas});

    CHECK(chart.placed_count() == 3);
    // 最重标签从内容区左上角起。
    CHECK(canvas.at(8, 1).cp == U'o');
    CHECK(count_nonblank(canvas) > 0);
}

TEST_CASE("chart/text/tag_cloud: 非法标签滤除与超宽放弃") {
    canvas::Canvas canvas(40, 14);
    TagCloud chart;
    chart.add_tag("", 2.0);
    chart.add_tag("zero", 0.0);
    chart.add_tag("nan", std::numeric_limits<double>::quiet_NaN());
    chart.draw({canvas});
    CHECK(chart.placed_count() == 0);
    CHECK(count_nonblank(canvas) == 0);

    // 单标签比整行还宽：放弃该标签，不截断。
    canvas::Canvas c2(40, 14);
    TagCloud wide;
    wide.add_tag("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 1.0);  // 40 列
    wide.draw({c2});
    CHECK(wide.placed_count() == 0);
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- table ----------------

TEST_CASE("chart/text/table: 表头、分隔线与数据行齐备") {
    canvas::Canvas canvas(40, 14);
    Table chart;
    chart.add_column("Name");
    chart.add_column("Val");
    chart.add_row({"a", "1"});
    chart.add_row({"b", "2"});
    CHECK(chart.column_count() == 2);
    CHECK(chart.row_count() == 2);
    chart.draw({canvas});

    // 表头从 (8,1) 起："Name" 占 4 列 → │ 在第 12 列。
    CHECK(canvas.at(8, 1).cp == U'N');
    CHECK(canvas.at(12, 1).cp == kSepV);
    // 表头分隔线：├ 在行首、─ 填充。
    CHECK(canvas.at(8, 2).cp == kSepT);
    CHECK(count_char(canvas, kSepH) > 0);
    // 数据行的列分隔与内容。
    CHECK(canvas.at(12, 3).cp == kSepV);
    CHECK(canvas.at(8, 3).cp == U'a');
}

TEST_CASE("chart/text/table: 无列不绘制、行多于高度截断不崩") {
    canvas::Canvas c1(40, 14);
    Table empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 8);  // 内容区 4 行：表头 + 分隔线 + 2 数据行
    Table many;
    many.add_column("c");
    for (int i = 0; i < 50; ++i) {
        many.add_row({"r"});
    }
    REQUIRE_NOTHROW(many.draw({c2}));
    CHECK(count_nonblank(c2) > 0);
    // 行数用尽即停：最后一行之外无内容。
    CHECK(c2.at(8, 7).cp == U'\u0020');
}

// ---------------- progress ----------------

TEST_CASE("chart/text/progress: █/░ 液条与百分比齐备") {
    canvas::Canvas canvas(40, 14);
    Progress chart;
    chart.add_bar("cpu", 0.5);
    chart.add_bar("mem", 0.25);
    CHECK(chart.bar_count() == 2);
    chart.draw({canvas});

    // label_d=3、bar_w = 31-3-1-5 = 22 → filled = 11 + round(5.5)=6。
    CHECK(count_char(canvas, kFill) == 17);
    CHECK(count_char(canvas, kEmptyBar) == 44 - 17);
    CHECK(count_char(canvas, U'%') >= 1);
    CHECK(canvas.at(8, 1).cp == U'c');  // 标签首字符
}

TEST_CASE("chart/text/progress: 无条不绘制、非法 fraction 收敛") {
    canvas::Canvas c1(40, 14);
    Progress empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    // NaN/负值/超 1 都收敛，不崩且行内液条仍画满 bar_w 格。
    canvas::Canvas c2(40, 14);
    Progress weird;
    weird.add_bar("nan", std::numeric_limits<double>::quiet_NaN());
    weird.add_bar("neg", -2.0);
    weird.add_bar("big", 7.0);
    REQUIRE_NOTHROW(weird.draw({c2}));
    CHECK(count_char(c2, kFill) == 22);   // 只有大 bar 全填
    CHECK(count_char(c2, kEmptyBar) == 2 * 22);
    // show_percent=false 时无百分号。
    canvas::Canvas c3(40, 14);
    Progress nopct;
    ProgressOptions nopct_opts;
    nopct_opts.show_percent = false;
    nopct.set_options(nopct_opts);
    nopct.add_bar("x", 0.5);
    nopct.draw({c3});
    CHECK(count_char(c3, U'%') == 0);
    CHECK(count_char(c3, kFill) > 0);
}
