// tests/unit/autoviz/test_autoviz.cpp —— AutoViz 引擎测试（规格书驱动）
//
// 覆盖：特征探测 / 六条管道 / 一票否决 / 两段式打分 / 端到端选图。
// 样本全部内嵌字符串常量（仓库测试惯例），断言确定性。
#include "termchart/autoviz/autoviz.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <doctest.h>

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

// 某图种是否被语义否决。
bool is_vetoed(const Result<AutoVizResult>& result, const std::string& name) {
    const ChartCandidate* c = find_candidate(result, name);
    return c != nullptr && !c->vetoes.empty();
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
    CHECK(std::any_of(semantic_check(c).begin(), semantic_check(c).end(),
                      [](const std::string& r) {
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
    CHECK(std::any_of(semantic_check(c).begin(), semantic_check(c).end(),
                      [](const std::string& r) {
                          return r.find("high_cardinality_color") != std::string::npos;
                      }));

    ChartCandidate cc;
    cc.name = chart_name::kScatter;
    cc.build = c.build;
    cc.point_count = 10;
    cc.x_is_categorical = true;
    cc.color_is_categorical = true;
    cc.color_tier = termchart::data::CardinalityTier::Low;
    CHECK(std::any_of(semantic_check(cc).begin(), semantic_check(cc).end(),
                      [](const std::string& r) {
                          return r.find("categorical_x_categorical") != std::string::npos;
                      }));
}

TEST_CASE("autoviz/semantic: 地理未去重否决（安全网）") {
    ChartCandidate c;
    c.name = chart_name::kScatter;
    c.build = []() { return std::make_unique<termchart::chart::ScatterChart>(); };
    c.point_count = 10;
    c.geo_deduped = false;
    CHECK(std::any_of(semantic_check(c).begin(), semantic_check(c).end(),
                      [](const std::string& r) {
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

// ── 端到端 ──────────────────────────────────────────────────────────────────

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
