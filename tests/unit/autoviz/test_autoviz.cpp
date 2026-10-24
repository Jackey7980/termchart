// tests/unit/autoviz/test_autoviz.cpp —— AutoViz 引擎测试（规格书驱动）
//
// 覆盖：特征探测 / 六条管道 / 一票否决 / 两段式打分 / 端到端选图。
// 样本全部内嵌字符串常量（仓库测试惯例），断言确定性。
#include "termchart/autoviz/autoviz.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <doctest.h>

#include "termchart/autoviz/domain.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/text/table.hpp"
#include "termchart/chart/text/wordcloud.hpp"

using namespace termchart::autoviz;
using termchart::Result;

namespace {

// 在候选列表里按图种名找第一个候选（不论是否被否决）。
const ChartCandidate* find_candidate(const Result<AutoVizResult>& result,
                                     const std::string& name) {
    if (!result.has_value()) {
        return nullptr;
    }
    for (const ChartCandidate& c : result->candidates) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

bool has_pipeline(const Result<AutoVizResult>& result, PipelineKind kind) {
    if (!result.has_value()) {
        return false;
    }
    for (const ChartCandidate& c : result->candidates) {
        if (c.pipeline == kind) {
            return true;
        }
    }
    return false;
}

// 单候选是否含指定规则码的否决理由。
bool has_veto_rule(const ChartCandidate& c, const std::string& rule) {
    return std::any_of(c.vetoes.begin(), c.vetoes.end(), [&rule](const std::string& r) {
        return r.find(rule) != std::string::npos;
    });
}

}  // namespace

// ── 特征探测 ────────────────────────────────────────────────────────────────

TEST_CASE("autoviz/features: 角色分配（时间/分类/数值/常量剔除）") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "ts,event,duration_min,note,const_col\n"
        "2024-01-01 08:00:00,login,5,hello,x\n"
        "2024-01-01 09:00:00,return,7,world,x\n"
        "2024-01-02 10:00:00,login,,again,x\n");
    const DatasetFeatures f = detect_features(frame);

    CHECK(f.has_time());
    CHECK(f.has_category());
    CHECK(f.has_numeric());
    CHECK(f.time_columns.size() == 1);
    CHECK(f.profiles[f.time_columns.front()].name == "ts");
    // note 只有 3 个去重值（Low 档），与 event 一样算分类列；event 在队首
    // （列序优先），路由的事件管道取它。
    CHECK(f.category_columns.size() == 2);
    CHECK(f.profiles[f.category_columns.front()].name == "event");
    CHECK(f.numeric_columns.size() == 1);
    CHECK(f.profiles[f.numeric_columns.front()].name == "duration_min");
    CHECK(std::find(f.dropped.begin(), f.dropped.end(), 4) != f.dropped.end());  // const_col
}

TEST_CASE("autoviz/features: 经纬度列名配对成 GeoPair") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "lat,lon,value\n"
        "39.9,116.4,1\n"
        "31.2,121.5,2\n"
        "23.1,113.3,3\n");
    const DatasetFeatures f = detect_features(frame);
    REQUIRE(f.geo_pairs.size() == 1);
    CHECK(f.profiles[f.geo_pairs.front().lat].name == "lat");
    CHECK(f.profiles[f.geo_pairs.front().lng].name == "lon");
    CHECK(f.numeric_columns.size() == 1);  // value 留下当数值
}

TEST_CASE("autoviz/features: 无 event 列时才做全局空值率剔除") {
    // 有 event 列：duration_min 全局空 2/3 也不丢（分桶口径）。
    const termchart::data::DataFrame with_event = termchart::data::parse_dataframe(
        "event,duration_min\n"
        "return,7\n"
        "return,9\n"
        "return,11\n"
        "login,\n"
        "logout,\n"
        "refresh,\n");
    const DatasetFeatures f1 = detect_features(with_event);
    CHECK(std::find(f1.dropped.begin(), f1.dropped.end(), 1) == f1.dropped.end());

    // 无 event 列：同样 2/3 空的列被全局剔除（阈值 0.6，7/8 空超线）。
    const termchart::data::DataFrame no_event = termchart::data::parse_dataframe(
        "duration_min\n7\n\n\n\n\n\n\n\n");
    const DatasetFeatures f2 = detect_features(no_event);
    CHECK(std::find(f2.dropped.begin(), f2.dropped.end(), 0) != f2.dropped.end());
}

TEST_CASE("autoviz/time_parse: ISO 形状解析与非法日期拒绝") {
    const auto t1 = parse_timestamp("2024-01-01");
    REQUIRE(t1.has_value());
    CHECK(*t1 == 1704067200.0);  // 2024-01-01T00:00:00Z

    const auto t2 = parse_timestamp("2024-01-01 08:30:00");
    REQUIRE(t2.has_value());
    CHECK(*t2 == 1704067200.0 + 8 * 3600 + 30 * 60);

    const auto t3 = parse_timestamp("2024-01-01T08:30:00Z");
    REQUIRE(t3.has_value());
    CHECK(*t3 == *t2);

    const auto t4 = parse_timestamp("2024-01-01T08:30:00+01:00");
    REQUIRE(t4.has_value());
    CHECK(*t4 == *t2 - 3600);  // 东一区 = UTC-1h

    CHECK_FALSE(parse_timestamp("2024-02-30").has_value());  // 非法日期
    CHECK_FALSE(parse_timestamp("2024-13-01").has_value());  // 非法月份
    CHECK_FALSE(parse_timestamp("not-a-time").has_value());
    CHECK_FALSE(parse_timestamp("").has_value());

    // 分桶阶梯：一天/24 桶 → 3600s；一小时/10 桶 → 600s（不小于 360 的最小档）。
    CHECK(nice_bucket_seconds(0.0, 86400.0, 24) == 3600.0);
    CHECK(nice_bucket_seconds(0.0, 3600.0, 10) == 600.0);
}

// ── 六条管道 ────────────────────────────────────────────────────────────────

TEST_CASE("autoviz/router: 时间管道（折线 + 热力图 + 桶计数）") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "ts,event,duration_min\n"
        "2024-01-01 08:00:00,login,5\n"
        "2024-01-01 09:00:00,return,7\n"
        "2024-01-02 10:00:00,login,6\n"
        "2024-01-02 11:00:00,return,8\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());
    CHECK(has_pipeline(result, PipelineKind::Time));

    const ChartCandidate* line = find_candidate(result, chart_name::kLine);
    REQUIRE(line != nullptr);
    CHECK(line->pipeline == PipelineKind::Time);
    CHECK(line->x_is_temporal);
    CHECK(line->temporal_sorted);
    CHECK(line->vetoes.empty());

    // 折线可建图，且每系列 x 严格非降序（桶聚合后有序）。
    auto chart = line->build();
    REQUIRE(chart != nullptr);
    const auto* lines = dynamic_cast<termchart::chart::LineChart*>(chart.get());
    REQUIRE(lines != nullptr);
    REQUIRE_FALSE(lines->series().empty());
    for (const auto& s : lines->series()) {
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            CHECK(s.points[i - 1].x <= s.points[i].x);
        }
    }
}

TEST_CASE("autoviz/router: 地理管道（去重 + 散点）") {
    // 同一坐标上报两次 → 去重后 3 个点（第 2 行是同坐标同实体，覆盖）。
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "lat,lon,cabinet,temp\n"
        "39.900,116.400,c1,41\n"
        "39.900,116.400,c1,43\n"
        "31.200,121.500,c2,38\n"
        "23.100,113.300,c3,35\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());
    CHECK(has_pipeline(result, PipelineKind::Geo));

    const ChartCandidate* scatter = find_candidate(result, chart_name::kScatter);
    REQUIRE(scatter != nullptr);
    CHECK(scatter->pipeline == PipelineKind::Geo);
    CHECK(scatter->point_count == 3);  // 强制去重生效
    CHECK(scatter->geo_deduped);
    CHECK(scatter->vetoes.empty());
}

TEST_CASE("autoviz/router: 事件管道（柱状 + 箱线 + 饼图）") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "event,duration_min\n"
        "login,5\n"
        "login,7\n"
        "return,9\n"
        "return,11\n"
        "return,13\n"
        "logout,3\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());
    CHECK(has_pipeline(result, PipelineKind::Event));

    const ChartCandidate* bar = find_candidate(result, chart_name::kBar);
    REQUIRE(bar != nullptr);
    CHECK(bar->pipeline == PipelineKind::Event);
    CHECK(bar->category_count == 3);  // login/return/logout
    CHECK(bar->vetoes.empty());

    const ChartCandidate* box = find_candidate(result, chart_name::kBoxplot);
    REQUIRE(box != nullptr);
    CHECK(box->vetoes.empty());
    auto box_chart = box->build();
    REQUIRE(box_chart != nullptr);
    const auto* boxes = dynamic_cast<termchart::chart::BoxPlotChart*>(box_chart.get());
    REQUIRE(boxes != nullptr);
    CHECK(boxes->labels().size() == 3);  // 箱贴 event 名标签

    const ChartCandidate* pie = find_candidate(result, chart_name::kPie);
    REQUIRE(pie != nullptr);
    CHECK(pie->vetoes.empty());  // 3 片无负值 → 合法
}

TEST_CASE("autoviz/router: 分类+数值组合管道（着色散点）") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "group,x,y\n"
        "a,1,2\n"
        "b,2,3\n"
        "a,3,5\n"
        "b,4,4\n"
        "a,5,8\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());
    CHECK(has_pipeline(result, PipelineKind::CategoryNumeric));
    const ChartCandidate* scatter = find_candidate(result, chart_name::kScatter);
    REQUIRE(scatter != nullptr);
    CHECK(scatter->color_is_categorical);
    CHECK(scatter->color_tier == termchart::data::CardinalityTier::Low);
    CHECK(scatter->vetoes.empty());
}

TEST_CASE("autoviz/router: 类数超色板容量（6）降级为单系列，不强行轮换同色") {
    // 7 类：强行 6 色轮换会让第 7 类与第 1 类同色（比分不出颜色更误导），
    // 正确行为是放弃颜色通道、出单系列散点。
    std::string csv = "group,x,y\n";
    for (int i = 0; i < 7; ++i) {
        csv += "g" + std::to_string(i) + "," + std::to_string(i) + "," +
               std::to_string(i * 2) + "\n";
    }
    const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(csv));
    REQUIRE(result.has_value());
    const ChartCandidate* scatter = find_candidate(result, chart_name::kScatter);
    REQUIRE(scatter != nullptr);
    CHECK(scatter->series_count == 1);           // 单系列
    CHECK_FALSE(scatter->color_is_categorical);  // 颜色通道已让位
    CHECK(scatter->vetoes.empty());
    auto chart = scatter->build();
    REQUIRE(chart != nullptr);
    const auto* built = dynamic_cast<termchart::chart::ScatterChart*>(chart.get());
    REQUIRE(built != nullptr);
    CHECK(built->series().size() == 1);  // 真建出来也只有一个系列
}

TEST_CASE("autoviz/router: 地理去重按实体+坐标，同楼不同机柜不误杀") {
    // 两个机柜同一坐标（同大楼）→ 键含实体 id，两个点都保留；
    // 同一机柜重复上报 → 后写覆盖，只留一个点。
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "lat,lon,cabinet,temp\n"
        "39.900,116.400,c1,41\n"
        "39.900,116.400,c2,42\n"
        "39.900,116.400,c1,43\n"
        "31.200,121.500,c2,38\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());
    const ChartCandidate* scatter = find_candidate(result, chart_name::kScatter);
    REQUIRE(scatter != nullptr);
    CHECK(scatter->point_count == 3);  // (c1,大楼)×1 + (c2,大楼)×1 + (c2,上海)×1
    CHECK(scatter->geo_deduped);
}

TEST_CASE("autoviz/router: 纯数值管道（1列直方图/2列散点/多列平行+相关）") {
    const Result<AutoVizResult> one = auto_visualize(termchart::data::parse_dataframe(
        "v\n1\n2\n2\n3\n3\n3\n4\n"));
    REQUIRE(one.has_value());
    const ChartCandidate* hist = find_candidate(one, chart_name::kHist);
    REQUIRE(hist != nullptr);
    CHECK(hist->pipeline == PipelineKind::Numeric);
    CHECK(hist->vetoes.empty());

    const Result<AutoVizResult> two = auto_visualize(termchart::data::parse_dataframe(
        "x,y\n1,2\n2,4\n3,5\n4,4\n"));
    REQUIRE(two.has_value());
    const ChartCandidate* scatter = find_candidate(two, chart_name::kScatter);
    REQUIRE(scatter != nullptr);
    CHECK(scatter->point_count == 4);
    CHECK(scatter->vetoes.empty());

    const Result<AutoVizResult> many = auto_visualize(termchart::data::parse_dataframe(
        "a,b,c\n1,2,3\n4,5,6\n7,8,9\n2,1,4\n"));
    REQUIRE(many.has_value());
    const ChartCandidate* parallel = find_candidate(many, chart_name::kParallel);
    REQUIRE(parallel != nullptr);
    CHECK(parallel->axis_count == 3);
    CHECK(parallel->vetoes.empty());
    const ChartCandidate* corr = find_candidate(many, chart_name::kCorr);
    REQUIRE(corr != nullptr);
    CHECK(corr->vetoes.empty());
}

TEST_CASE("autoviz/router: 纯文本管道（词云）") {
    const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
        "note\nhello\nworld\nhello\nfoo\nbar\nbaz\n"));
    REQUIRE(result.has_value());
    CHECK(has_pipeline(result, PipelineKind::Text));
    const ChartCandidate* cloud = find_candidate(result, chart_name::kWordcloud);
    REQUIRE(cloud != nullptr);
    CHECK(cloud->category_count == 5);  // hello(×2)/world/foo/bar/baz 去重后 5 个词
    CHECK(cloud->vetoes.empty());
}

TEST_CASE("autoviz/router: 兜底管道（预览表格永远可用）") {
    const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
        "note\nhello\n"));
    REQUIRE(result.has_value());
    const ChartCandidate* table = find_candidate(result, chart_name::kTable);
    REQUIRE(table != nullptr);
    CHECK(table->pipeline == PipelineKind::Preview);
    CHECK(table->vetoes.empty());
    CHECK(result->chosen() != nullptr);  // 任何输入都有答案
}

TEST_CASE("autoviz/router: 兜底预览表对全 null / 单行都绝对不空") {
    // 全 null：行列齐整（列数跟数据行走，header 长于数据行的输入由
    // DataFrame 的 Ragged 语义决定列数，不在此测）；单元格 raw 为空串，
    // 表头照印。
    {
        const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
            "a,b\nNA,\n,NA\n"));
        REQUIRE(result.has_value());
        const ChartCandidate* table = find_candidate(result, chart_name::kTable);
        REQUIRE(table != nullptr);
        CHECK(table->vetoes.empty());
        auto chart = table->build();
        REQUIRE(chart != nullptr);
        const auto* built = dynamic_cast<termchart::chart::Table*>(chart.get());
        REQUIRE(built != nullptr);
        CHECK(built->column_count() == 2);  // 表头在（Table::draw 无条件先画表头）
        CHECK(result->chosen() != nullptr);
    }
    // 单行：预览 1 行数据。
    {
        const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
            "a,b\n1,2\n"));
        REQUIRE(result.has_value());
        const ChartCandidate* table = find_candidate(result, chart_name::kTable);
        REQUIRE(table != nullptr);
        CHECK(table->point_count == 1);
        CHECK(result->chosen() != nullptr);
    }
    // 仅表头（0 数据行）：parse_dataframe 返回 0 列 → auto_visualize 显式
    // InvalidArgument（fail-loud：没有数据不是可视化问题，不编造空图）。
    {
        const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
            "a,b\n"));
        CHECK_FALSE(result.has_value());
        CHECK(result.error() == termchart::ErrorCode::InvalidArgument);
    }
}

// ── 一票否决 ────────────────────────────────────────────────────────────────

TEST_CASE("autoviz/semantic: 饼图负值与分片数否决") {
    ChartCandidate c;
    c.name = chart_name::kPie;
    c.build = []() { return std::make_unique<termchart::chart::PieChart>(); };
    c.category_count = 3;
    c.has_negative_values = true;
    const auto vetoes = semantic_check(c);
    REQUIRE_FALSE(vetoes.empty());
    CHECK(std::any_of(vetoes.begin(), vetoes.end(), [](const std::string& r) {
        return r.find("pie_negative") != std::string::npos;
    }));

    ChartCandidate many;
    many.name = chart_name::kPie;
    many.build = c.build;
    many.category_count = 12;
    const auto vetoes2 = semantic_check(many);
    CHECK(std::any_of(vetoes2.begin(), vetoes2.end(), [](const std::string& r) {
        return r.find("pie_too_many_slices") != std::string::npos;
    }));
}

TEST_CASE("autoviz/semantic: 时间轴未排序否决折线") {
    ChartCandidate c;
    c.name = chart_name::kLine;
    c.build = []() { return std::make_unique<termchart::chart::LineChart>(); };
    c.point_count = 10;
    c.x_is_temporal = true;
    c.temporal_sorted = false;
    const auto vetoes = semantic_check(c);
    CHECK(std::any_of(vetoes.begin(), vetoes.end(), [](const std::string& r) {
        return r.find("temporal_unsorted") != std::string::npos;
    }));
}

TEST_CASE("autoviz/semantic: 高基数颜色通道与分类×分类否决") {
    ChartCandidate c;
    c.name = chart_name::kScatter;
    c.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    c.point_count = 10;
    c.color_is_categorical = true;
    c.color_tier = termchart::data::CardinalityTier::High;
    const auto vetoes = semantic_check(c);
    CHECK(std::any_of(vetoes.begin(), vetoes.end(), [](const std::string& r) {
        return r.find("high_cardinality_color") != std::string::npos;
    }));

    ChartCandidate cc;
    cc.name = chart_name::kScatter;
    cc.build = c.build;
    cc.point_count = 10;
    cc.x_is_categorical = true;
    cc.color_is_categorical = true;
    cc.color_tier = termchart::data::CardinalityTier::Low;
    const auto vetoes_cc = semantic_check(cc);
    CHECK(std::any_of(vetoes_cc.begin(), vetoes_cc.end(), [](const std::string& r) {
        return r.find("categorical_x_categorical") != std::string::npos;
    }));
}

TEST_CASE("autoviz/semantic: 地理未去重否决（安全网）") {
    ChartCandidate c;
    c.name = chart_name::kScatter;
    c.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    c.point_count = 10;
    c.geo_deduped = false;
    const auto vetoes = semantic_check(c);
    CHECK(std::any_of(vetoes.begin(), vetoes.end(), [](const std::string& r) {
        return r.find("geo_not_deduped") != std::string::npos;
    }));
}

TEST_CASE("autoviz/semantic: 词云/散点/平行坐标/直方图的下限否决") {
    ChartCandidate few_words;
    few_words.name = chart_name::kWordcloud;
    few_words.build = []() { return std::make_unique<termchart::chart::Wordcloud>(); };
    few_words.category_count = 2;
    few_words.vetoes = semantic_check(few_words);
    CHECK(has_veto_rule(few_words, "wordcloud_too_few_words"));

    ChartCandidate few_points;
    few_points.name = chart_name::kScatter;
    few_points.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    few_points.point_count = 2;
    few_points.vetoes = semantic_check(few_points);
    CHECK(has_veto_rule(few_points, "scatter_too_few_points"));

    ChartCandidate two_axes;
    two_axes.name = chart_name::kParallel;
    two_axes.build = []() { return std::make_unique<termchart::chart::ParallelChart>(); };
    two_axes.axis_count = 2;
    two_axes.vetoes = semantic_check(two_axes);
    CHECK(has_veto_rule(two_axes, "parallel_too_few_axes"));

    ChartCandidate constant;
    constant.name = chart_name::kHist;
    constant.build = []() { return std::make_unique<termchart::chart::HistChart>(); };
    constant.value_constant = true;
    constant.point_count = 10;
    constant.vetoes = semantic_check(constant);
    CHECK(has_veto_rule(constant, "hist_constant"));
}

TEST_CASE("autoviz/semantic: 常量列在探测期被丢，端到端走预览兜底") {
    // 单值列 distinct=1 → Constant 档，探测期就丢（根本到不了直方图，
    // 所以 hist_constant 否决是安全网而非主路径）；预览表不过滤 dropped，
    // "任何输入都有答案"仍成立。
    const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
        "v\n5\n5\n5\n5\n"));
    REQUIRE(result.has_value());
    CHECK(std::find(result->features.dropped.begin(), result->features.dropped.end(), 0) !=
          result->features.dropped.end());
    CHECK(find_candidate(result, chart_name::kHist) == nullptr);
    const ChartCandidate* chosen = result->chosen();
    REQUIRE(chosen != nullptr);
    CHECK(chosen->name == chart_name::kTable);
}

// ── 打分 ────────────────────────────────────────────────────────────────────

TEST_CASE("autoviz/score: 解析式打分的单调性与否决归零") {
    ChartCandidate c;
    c.name = chart_name::kScatter;
    c.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    c.point_count = 10;
    c.value_null_rate = 0.0;
    const double clean = analytic_score(c);
    c.value_null_rate = 0.8;  // 高空值率应扣分
    const double dirty = analytic_score(c);
    CHECK(dirty < clean);
    c.vetoes.emplace_back("test");
    CHECK(analytic_score(c) == 0.0);  // 否决 = 0 分
}

TEST_CASE("autoviz/score: 画布级复核对空图给低分、对正常图给高分") {
    // 空画布：table 无列无行 → 全空格。
    ChartCandidate empty;
    empty.name = chart_name::kTable;
    empty.build = []() {
        auto chart = std::make_unique<termchart::chart::Table>();
        return chart;
    };
    const double empty_score = raster_confirm(empty, 80, 24);
    CHECK(empty_score < 0.5);

    // 正常图：30 点散点，覆盖率与行使用率都应落在满意区间。
    ChartCandidate normal;
    normal.name = chart_name::kScatter;
    normal.build = []() {
        auto chart = std::make_unique<termchart::chart::ScatterChart>();
        termchart::chart::Series s;
        s.name = "s";
        for (int i = 0; i < 30; ++i) {
            s.points.push_back(termchart::Point{static_cast<double>(i % 10),
                                                static_cast<double>((i * 7) % 10)});
        }
        chart->add_series(std::move(s));
        return chart;
    };
    const RasterStats stats = raster_stats(normal, 80, 24);
    CHECK(stats.ink_cells > 0);
    CHECK(stats.coverage > 0.02);
    CHECK(raster_confirm(normal, 80, 24) > empty_score);

    // 建图炸锅 → 0 分（验证 catch 路径不炸测试）。
    ChartCandidate boom;
    boom.name = chart_name::kScatter;
    boom.build = []() -> std::unique_ptr<termchart::chart::Chart> {
        throw std::string("boom");
    };
    CHECK(raster_confirm(boom, 80, 24) == 0.0);
}

TEST_CASE("autoviz/score: 复核只做乘性下调，不给复核过的候选发奖金") {
    ChartCandidate c;
    c.name = chart_name::kScatter;
    c.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    c.analytic_score = 0.80;

    CHECK(c.total_score() == doctest::Approx(0.80));  // 未复核（NaN）→ 原样

    c.raster_score = 1.0;  // 形状健康
    CHECK(c.total_score() == doctest::Approx(0.80));  // 乘 1.0，不上浮

    c.raster_score = 0.0;  // 空图/糊图
    CHECK(c.total_score() ==
          doctest::Approx(0.80 * (1.0 - ChartCandidate::kRasterWeight)));

    // 核心回归：解析分高但**未复核**的候选，必须压过解析分低却复核满分的候选。
    // （旧实现把"复核过"当排序首要判据，会让低分候选凭复核翻盘。）
    ChartCandidate unreviewed;
    unreviewed.name = chart_name::kBar;
    unreviewed.build = c.build;
    unreviewed.analytic_score = 0.90;
    CHECK(unreviewed.total_score() > c.total_score());

    // 被否决的候选恒 -1，排在所有可用候选之后。
    c.vetoes.emplace_back("test");
    CHECK(c.total_score() == doctest::Approx(-1.0));
}

TEST_CASE("autoviz/time_parse: 轴标签粒度与人类可读跨度") {
    const auto t = parse_timestamp("2026-03-01T08:00:00Z");
    REQUIRE(t.has_value());
    // span < 1 天 → 只给时刻（同一天内无歧义）
    CHECK(format_timestamp_short(*t, 6 * 3600.0, 900.0) == "08:00");
    // 跨天但刻度间隔按小时 → 必须带 MM-DD：否则相邻刻度全显示 "03-01"，信息为零
    CHECK(format_timestamp_short(*t, 1.7 * 86400.0, 13 * 3600.0) == "03-01 08:00");
    // 跨度数月、间隔按天 → MM-DD
    CHECK(format_timestamp_short(*t, 60 * 86400.0, 7 * 86400.0) == "03-01");
    // 多年跨度 → YYYY-MM
    CHECK(format_timestamp_short(*t, 5 * 365 * 86400.0, 30 * 86400.0) == "2026-03");
    // 非有限 epoch 给占位符，不把 "nan" 画到轴上
    CHECK(format_timestamp_short(std::nan(""), 3600.0, 60.0) == "?");

    // 桶宽的人类可读描述（标题里的 "按 2 小时均值"）
    CHECK(format_span_human(7200.0) == "2 小时");
    CHECK(format_span_human(5400.0) == "1.5 小时");
    CHECK(format_span_human(7 * 86400.0) == "7 天");
    CHECK(format_span_human(90.0) == "1.5 分钟");
    CHECK(format_span_human(0.0) == "0 秒");
}

// ── 稳健 Y 域（离群裁剪）────────────────────────────────────────────────────

TEST_CASE("autoviz/domain: Tukey 篱裁掉极端值、保留主体域") {
    const std::vector<double> clean{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const RobustDomain quiet = robust_y_domain(clean);
    CHECK_FALSE(quiet.active());
    CHECK(quiet.clipped == 0);
    CHECK(quiet.lo == doctest::Approx(1.0));
    CHECK(quiet.hi == doctest::Approx(10.0));

    const std::vector<double> dirty{1, 2, 3, 4, 5, 6, 7, 8, 9, 1e21};
    const RobustDomain dom = robust_y_domain(dirty);
    CHECK(dom.active());
    CHECK(dom.clipped == 1);
    CHECK(dom.high);
    CHECK(dom.extreme() == doctest::Approx(1e21));
    CHECK(dom.hi < 100.0);  // 1e21 绝不进轴域
    CHECK(dom.hi >= 9.0);   // 但不许把主体一起裁掉
    CHECK(dom.lo == doctest::Approx(1.0));
}

TEST_CASE("autoviz/domain: 小样本/常数列/全空都不硬裁") {
    // 少于 kOutlierMinSamples 个值：分位数没有统计意义，宁可画平线也不猜。
    const RobustDomain tiny = robust_y_domain(std::vector<double>{1.0, 1e21});
    CHECK_FALSE(tiny.active());
    CHECK(tiny.hi == doctest::Approx(1e21));

    // 常数列 + 一个脏值：IQR 退化为 0，改用相对中位数的篱；裁完域退化时要撑开，
    // 否则比例尺 span=0（除零），图直接烂掉。
    const RobustDomain same = robust_y_domain(std::vector<double>{1, 1, 1, 1, 1, 1e21});
    CHECK(same.active());
    CHECK(same.clipped == 1);
    CHECK(same.hi < 10.0);
    CHECK(same.hi > same.lo);

    // 全 NaN/Inf：usable() 为 false，调用方必须退回数据自身域。
    const RobustDomain bad = robust_y_domain(std::vector<double>{std::nan(""), HUGE_VAL});
    CHECK_FALSE(bad.usable());

    // fence <= 0 = 关闭裁剪（用户显式要"轴域就是 [min,max]"）。
    CHECK_FALSE(robust_y_domain(std::vector<double>{1, 2, 3, 4, 5, 1e21}, 0.0).active());
}

TEST_CASE("autoviz/domain: 多系列域取并集、裁剪点数相加") {
    const RobustDomain a = robust_y_domain(std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 1e21});
    const RobustDomain b =
        robust_y_domain(std::vector<double>{10, 20, 30, 40, 50, 60, 70, 80, 90, -1e21});
    const RobustDomain m = merge_domains(a, b);
    CHECK(m.lo == doctest::Approx(-45.0));  // b 的篱内下界（Q1=22.5, IQR=45）
    CHECK(m.hi == doctest::Approx(90.0));
    CHECK(m.clipped == 2);  // 上下各裁一个
}

TEST_CASE("autoviz: 极端离群值 → 轴域裁剪 + 标题留痕 + 体检点名") {
    const termchart::data::DataFrame frame = termchart::data::parse_dataframe(
        "ts,v\n"
        "2024-01-01 08:00:00,1000\n"
        "2024-01-01 09:00:00,1200\n"
        "2024-01-01 10:00:00,900\n"
        "2024-01-02 08:00:00,1100\n"
        "2024-01-02 09:00:00,1e21\n"
        "2024-01-02 10:00:00,800\n");
    const Result<AutoVizResult> result = auto_visualize(frame);
    REQUIRE(result.has_value());

    // 体检：离群列被点名（维度是"值域"，与 suspects 的"列名口径"是两回事）。
    REQUIRE(result->features.outliers.size() == 1);
    CHECK(result->features.outliers[0].name == "v");
    CHECK(result->features.outliers[0].high);
    CHECK(result->features.outliers[0].extreme == doctest::Approx(1e21));

    const ChartCandidate* line = find_candidate(result, chart_name::kLine);
    REQUIRE(line != nullptr);
    CHECK(line->clipped_points > 0);
    CHECK(line->title.find("裁去") != std::string::npos);  // 标题必须留痕

    // 真建出来的图：Y 轴域里绝不许出现 1e21（否则主体又被压成平线）。
    std::unique_ptr<termchart::chart::Chart> chart = line->build();
    REQUIRE(chart != nullptr);
    auto* line_chart = dynamic_cast<termchart::chart::LineChart*>(chart.get());
    REQUIRE(line_chart != nullptr);
    termchart::canvas::Canvas canvas(80, 24);
    line_chart->draw(termchart::chart::DrawContext{canvas});
    const termchart::layout::PlotArea& area = line_chart->last_area();
    CHECK(area.y_scale().domain_max() < 1e6);
    CHECK(area.y_scale().domain_min() <= 800.0);
}

// ── 端到端 ──────────────────────────────────────────────────────────────────

// 真实脏订单数据（用户提供，2026-10-24）：斜杠/点/中文日期、Unix epoch、
// 非法日期 2026/13/40、未加引号的 ¥2,000 冲错整行、15%、1e21、JSON 嵌套+null、
// mojibake、status 混 1/0/NULL/空格。两个修复的回归锚点：
//   ① order_time 靠多分隔符形状族认出 Timestamp（7/13 → 11/13 形状命中）；
//   ② 事件键按"分组力"（distinct 升序）选列，order_id(12) 不再当分组维度。
TEST_CASE("autoviz/engine: 真实脏订单 CSV（torture，选中时间折线 + 饼图负值否决）") {
    const char* kOrders = R"(order_id,customer_id,order_time,channel,total_amount,currency,shipping_json,status
O001,C001,2026-03-01T08:00:00Z,APP,"1,234.50",CNY,"{""city"":""上海"",""zip"":""200000"",""tags"":[""vip"",""fast""]}",paid
O002,C002,2026/03/01 09:00,WEB,¥2,000,USD,"{""city"":""Beijing"",""zip"":""100000"",""tags"":[]}",PAID
O003,C999,2026.03.01 10:00,APP,-150.00,CNY,"{""city"":""锟斤拷"",""zip"":null}", 
O004,C003,2026-03-01 11:00:00+08:00,MINI,1.2e3,, "{}",1
O005,C004,2026/13/40,H5,(123.45),CNY,"{""city"":""广州"",""zip"":""510000"",""tags"":[""new""]}",0
O006,C005,1740787200,APP,1000000,CNY,"{""city"":""深圳"",""zip"":""518000""}",paid
O007,C006,2026-03-02T12:00:00Z,WEB,0.000001,USD,"{""city"":""杭州"",""zip"":""310000"",""tags"":[""test""]}",paid
O008,C007,2026年3月2日,APP,"1,000,000",CNY,"{""city"":""上海"",""zip"":""200001""}",PAID
O001,C008,2026-03-02 14:00,WEB,500,CNY,"{""city"":""上海"",""zip"":""200002""}",paid
O009,C009,2026-03-02 15:00,MINI,,CNY,"{""city"":""成都"",""zip"":""610000"",""tags"":null}",NULL
O010,C010,2026-03-02 16:00,H5,15%,CNY,"{""city"":""武汉"",""zip"":""430000""}",paid
O011,C011,2026-03-02 17:00,APP,999999999999999999999,CNY,"{""city"":""西安"",""zip"":""710000""}",paid
O012,C012,2026-03-02 18:00,WEB,-100,USD,"{""city"":""南京"",""zip"":""210000"",""tags"":[""refund""]}",refunded)";

    const Result<AutoVizResult> result =
        auto_visualize(termchart::data::parse_dataframe(kOrders));
    REQUIRE(result.has_value());

    // ① order_time 认出时间列（未加引号的 ¥2,000 把 O002 行冲出第 9 列，
    //     phantom 列因常量被丢，不影响主表 8 列）。
    const DatasetFeatures& f = result->features;
    CHECK(f.time_columns.size() == 1);
    CHECK(f.profiles[f.time_columns.front()].name == "order_time");
    CHECK(f.profiles[f.time_columns.front()].kind == termchart::data::ColumnKind::Timestamp);
    CHECK(f.numeric_columns.size() == 1);
    CHECK(f.profiles[f.numeric_columns.front()].name == "total_amount");

    // 该表列序与表头一致 → 体检不误报（total_amount 是 Numeric，
    // channel/currency 是 Text，均与该有的口径相符）。
    CHECK(f.suspects.empty());

    // ② 事件键是分组力最强的分类列（currency 3 类 / channel 4 类在前，
    //    order_id 12 类、customer_id 13 类沉底）——绝不再是 order_id。
    CHECK(f.profiles[f.category_columns.front()].name == "currency");

    // 选中时间折线（订单数据的最优解）；饼图因负值被一票否决。
    const ChartCandidate* chosen = result->chosen();
    REQUIRE(chosen != nullptr);
    CHECK(chosen->name == chart_name::kLine);
    CHECK(chosen->pipeline == PipelineKind::Time);

    const ChartCandidate* pie = find_candidate(result, chart_name::kPie);
    REQUIRE(pie != nullptr);
    CHECK(has_veto_rule(*pie, "pie_negative"));  // -150 / -123.45 / -100

    // 折线真能建出来，且 x（桶中心）严格非降序。
    auto chart = chosen->build();
    REQUIRE(chart != nullptr);
    const auto* lines = dynamic_cast<termchart::chart::LineChart*>(chart.get());
    REQUIRE(lines != nullptr);
    REQUIRE_FALSE(lines->series().empty());
    for (const auto& s : lines->series()) {
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            CHECK(s.points[i - 1].x <= s.points[i].x);
        }
    }
}

// 真实脏支付数据（用户提供，2026-10-24）：表头与数据列序错位——从 P003 起
// 每行少一个 amount 字段，amount/method 两列整体错位；再叠加未加引号的 ¥2,000
// 冲行、2026/13/40 非法日期、Unix epoch、1e21 级大数、(123.45)、15%、JSON 内
// null、mojibake、status 混 success/1/0/NULL。
// 期望：引擎**信类型不信名字**，照错位的真实内容路由（图本身语义仍成立），
// 同时由 suspects 体检报出两条口径矛盾提示——人据此去修表，而不是被静默放过。
TEST_CASE("autoviz/engine: 真实脏支付 CSV（torture，表头错位体检报出 amount/method）") {
    const char* kPayments = R"(payment_id,order_id,pay_time,amount,method,gateway_json,status
P001,O001,2026-03-01T08:01:00Z,"1,234.50",ALIPAY,"{""trade"":""T001"",""ip"":""1.1.1.1"",""ok"":true}",success
P002,O002,2026/03/01 09:01,WECHAT,¥2,000,"{""trade"":""T002"",""ip"":""2.2.2.2"",""ok"":true}",SUCCESS
P003,O999,2026.03.01 10:01,CARD,-150.00,"{""trade"":""T003"",""ip"":null}", 
P004,O003,1740787260,ALIPAY,1.2e3,"{""trade"":""T004"",""ok"":1}",1
P005,O004,2026-03-01 11:01:00+08:00,WECHAT,(123.45),"{""trade"":""T005"",""ip"":""5.5.5.5"",""ok"":false}",0
P006,O005,2026/13/40,CARD,1000000,"{""trade"":""T006"",""ip"":""6.6.6.6""}",success
P007,O006,2026-03-02T12:01:00Z,ALIPAY,0.000001,"{""trade"":""T007"",""ip"":""7.7.7.7"",""ok"":true}",success
P008,O007,2026年3月2日,WECHAT,"1,000,000","{""trade"":""T008"",""ip"":""8.8.8.8""}",SUCCESS
P001,O008,2026-03-02 14:01,CARD,500,"{""trade"":""T009"",""ip"":""9.9.9.9"",""ok"":true}",success
P009,O009,2026-03-02 15:01,ALIPAY,,"{""trade"":""T010"",""ip"":""10.0.0.1""}",NULL
P010,O010,2026-03-02 16:01,WECHAT,15%,"{""trade"":""T011"",""ip"":""11.1.1.1""}",success
P011,O011,2026-03-02 17:01,CARD,999999999999999999999,"{""trade"":""T012"",""ip"":""12.2.2.2""}",success
P012,O012,2026-03-02 18:01,ALIPAY,-100,"{""trade"":""T013"",""ip"":""13.3.3.3""}",refunded)";

    const Result<AutoVizResult> result =
        auto_visualize(termchart::data::parse_dataframe(kPayments));
    REQUIRE(result.has_value());
    const DatasetFeatures& f = result->features;

    // 错位的实证：amount 列里装的是支付方式（Text），method 列里装的是金额（Numeric）。
    CHECK(f.profiles[3].name == "amount");
    CHECK(f.profiles[3].kind == termchart::data::ColumnKind::Text);
    CHECK(f.profiles[4].name == "method");
    CHECK(f.profiles[4].kind == termchart::data::ColumnKind::Numeric);

    // 体检报出两条口径矛盾（只诊断、不改路由）。
    REQUIRE(f.suspects.size() == 2);
    CHECK(f.suspects[0].name == "amount");
    CHECK(f.suspects[0].expected == "numeric");
    CHECK(f.suspects[0].actual == "text");
    CHECK(f.suspects[1].name == "method");
    CHECK(f.suspects[1].expected == "categorical");
    CHECK(f.suspects[1].actual == "numeric");

    // 路由未被体检干预：仍是"按 amount 列的取值分组、对 method 列求均值"——
    // 分组维度是真实内容（4 类支付方式），度量是真实金额，图本身没错。
    CHECK(f.numeric_columns.size() == 1);
    CHECK(f.profiles[f.numeric_columns.front()].name == "method");
    REQUIRE_FALSE(f.category_columns.empty());
    CHECK(f.profiles[f.category_columns.front()].name == "amount");

    const ChartCandidate* chosen = result->chosen();
    REQUIRE(chosen != nullptr);
    CHECK(chosen->name == chart_name::kBar);
    CHECK(chosen->pipeline == PipelineKind::Event);
}

TEST_CASE("autoviz/time_parse: 多分隔符族解析 + 历法拒绝 + epoch 不认") {
    const double kMar1 = 1772323200.0;  // 2026-03-01T00:00:00Z

    const auto t1 = parse_timestamp("2026/03/01 09:00");
    REQUIRE(t1.has_value());
    CHECK(*t1 == kMar1 + 9 * 3600);

    const auto t2 = parse_timestamp("2026.03.01 10:00");
    REQUIRE(t2.has_value());
    CHECK(*t2 == kMar1 + 10 * 3600);

    const auto t3 = parse_timestamp("2026年3月2日");
    REQUIRE(t3.has_value());
    CHECK(*t3 == kMar1 + 86400);

    const auto t4 = parse_timestamp("2026年3月2日9时30分");
    REQUIRE(t4.has_value());
    CHECK(*t4 == kMar1 + 86400 + 9 * 3600 + 30 * 60);

    const auto t5 = parse_timestamp("2026-03-01T08:00:00+01:00");
    REQUIRE(t5.has_value());
    CHECK(*t5 == kMar1 + 7 * 3600);  // 东一区 08:00 = UTC 07:00

    CHECK_FALSE(parse_timestamp("2026/13/40").has_value());  // 形状合法、历法非法
    CHECK_FALSE(parse_timestamp("2026-02-30").has_value());  // 2 月 30 号
    CHECK_FALSE(parse_timestamp("1740787200").has_value());  // Unix epoch 刻意不认
    CHECK_FALSE(parse_timestamp("2026-03-01T08:00:00+99:00").has_value());  // 偏移越界
}

TEST_CASE("autoviz/engine: JSONL 一站式入口（脏行跳过 + 选图）") {
    const char* jsonl =
        "{\"ts\":\"2024-01-01 08:00:00\",\"event\":\"login\",\"duration_min\":5}\n"
        "{oops-not-json\n"
        "{\"ts\":\"2024-01-01 09:00:00\",\"event\":\"return\",\"duration_min\":7}\n"
        "{\"ts\":\"2024-01-02 10:00:00\",\"event\":\"login\",\"duration_min\":6}\n";
    const Result<AutoVizResult> result = auto_visualize_jsonl(jsonl);
    REQUIRE(result.has_value());
    CHECK(result->features.rows == 3);        // 脏行被跳过
    CHECK(result->chosen() != nullptr);       // 有答案
    CHECK_FALSE(result->candidates.empty());  // 候选明细可查
    auto chart = result->build_chosen();
    CHECK(chart != nullptr);
}

TEST_CASE("autoviz/engine: 空表报 InvalidArgument，单行表走兜底") {
    const Result<AutoVizResult> empty = auto_visualize(termchart::data::parse_dataframe(""));
    CHECK_FALSE(empty.has_value());
    CHECK(empty.error() == termchart::ErrorCode::InvalidArgument);

    const Result<AutoVizResult> single =
        auto_visualize(termchart::data::parse_dataframe("note\nhello\n"));
    REQUIRE(single.has_value());
    CHECK(single->chosen() != nullptr);
}

TEST_CASE("autoviz/engine: 候选按综合分降序，否决的沉底") {
    const Result<AutoVizResult> result = auto_visualize(termchart::data::parse_dataframe(
        "event,duration_min\n"
        "login,5\n"
        "login,7\n"
        "return,9\n"
        "return,11\n"
        "return,13\n"));
    REQUIRE(result.has_value());
    const ChartCandidate* chosen = result->chosen();
    REQUIRE(chosen != nullptr);
    CHECK(chosen->vetoes.empty());
    CHECK(chosen == &result->candidates.front());  // 选中即榜首
    // 榜首之后不允许出现"可用候选分数低于末尾否决候选"之外的乱序——简化断言：
    // 第一个否决候选之后不再有可用候选。
    bool seen_vetoed = false;
    for (const ChartCandidate& c : result->candidates) {
        if (!c.viable()) {
            seen_vetoed = true;
        } else {
            CHECK_FALSE(seen_vetoed);
        }
    }
}
