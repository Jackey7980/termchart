// tests/unit/data/test_autoviz_pipeline.cpp —— AutoViz 流水线断言（规格书驱动）
//
// 覆盖规格书阶段一（双轨解析/保 Key/轻底座）与阶段二（抽样探测/基数分档/
// 分桶空值率）的可测部分。路由/否决/打分属 autoviz 模块（L21），其断言
// 随该模块落地追加到本文件（测试名统一 autoviz:: 前缀）。
//
// 注：本文件的断言按规格书文本推导（原始 14 条清单不在对话上下文中），
// 如与既定清单有出入，以清单为准修订。
#include "termchart/data/column_profile.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/data/dataframe_json.hpp"

#include <cmath>
#include <string>
#include <vector>

#include <doctest.h>

using namespace termchart::data;
using termchart::Result;  // Result 住 core 命名空间，data 只是用户

namespace {

bool is_nan(double d) { return d != d; }

// 生成 N 个不同取值的列（用于基数分档断言）。
std::string csv_with_distinct(std::size_t distinct) {
    std::string csv = "v\n";
    for (std::size_t i = 0; i < distinct; ++i) {
        csv += "k" + std::to_string(i) + "\n";
    }
    return csv;
}

const ColumnProfile* find_column(const std::vector<ColumnProfile>& profiles,
                                 const std::string& name) {
    for (const ColumnProfile& p : profiles) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

}  // namespace

// ── 阶段一：双轨制解析 ──────────────────────────────────────────────────────

TEST_CASE("autoviz::stage1 headerless csv gets Col_0..Col_N-1") {
    const DataFrame frame = parse_dataframe("a,1\nb,2\n", DataFrameOptions{0, false});
    CHECK(frame.headers.size() == 2);
    CHECK(frame.headers[0] == "col0");
    CHECK(frame.headers[1] == "col1");
    CHECK(frame.row_count == 2);
}

TEST_CASE("autoviz::stage1 jsonl preserves original keys (no rename, no reorder)") {
    const char* jsonl =
        "{\"event\":\"login\",\"duration_min\":5}\n"
        "{\"event\":\"return\",\"duration_min\":7}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const DataFrame& frame = result->frame;
    CHECK(frame.headers == std::vector<std::string>{"event", "duration_min"});
    CHECK(frame.row_count == 2);
    CHECK(frame.columns[0][0].raw == "login");
    CHECK(frame.columns[1][0].is_numeric());
    CHECK(frame.columns[1][0].num == 5.0);
}

TEST_CASE("autoviz::stage1 jsonl nested flatten to dotted path") {
    const char* jsonl =
        "{\"name\":\"a\",\"loc\":{\"lat\":39.9,\"lng\":116.4}}\n"
        "{\"name\":\"b\",\"loc\":{\"lat\":31.2,\"lng\":121.5}}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const DataFrame& frame = result->frame;
    CHECK(frame.headers == std::vector<std::string>{"name", "loc.lat", "loc.lng"});
    CHECK(frame.columns[1][0].num == 39.9);
    CHECK(frame.columns[2][1].num == 121.5);
}

TEST_CASE("autoviz::stage1 jsonl missing key becomes Empty, not shifted") {
    const char* jsonl =
        "{\"a\":1,\"b\":2}\n"
        "{\"a\":3}\n";  // 第二行缺 b
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const DataFrame& frame = result->frame;
    CHECK(frame.headers == std::vector<std::string>{"a", "b"});
    CHECK(frame.columns[0][1].num == 3.0);
    CHECK(frame.columns[1][1].is_missing());  // b 缺位 → Empty，不是把 a 挪过来
}

TEST_CASE("autoviz::stage1 jsonl new key in later line appended at end") {
    const char* jsonl =
        "{\"a\":1}\n"
        "{\"a\":2,\"b\":9}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    CHECK(result->frame.headers == std::vector<std::string>{"a", "b"});
    CHECK(result->frame.columns[1][0].is_missing());  // 首行没有 b → 补空
    CHECK(result->frame.columns[1][1].num == 9.0);
}

TEST_CASE("autoviz::stage1 jsonl skips invalid lines by default, fail-loud on demand") {
    const char* jsonl =
        "{\"a\":1}\n"
        "{oops\n"       // 畸形行
        "\"scalar\"\n"  // 非记录行
        "\n"            // 空白行不计入
        "{\"a\":2}\n";
    const Result<JsonFrameResult> tolerant = dataframe_from_jsonl(jsonl);
    REQUIRE(tolerant.has_value());
    CHECK(tolerant->lines_total == 4);
    CHECK(tolerant->lines_used == 2);
    CHECK(tolerant->lines_skipped == 2);
    CHECK(tolerant->frame.row_count == 2);

    const Result<JsonFrameResult> strict =
        dataframe_from_jsonl(jsonl, JsonFrameOptions{false});
    CHECK_FALSE(strict.has_value());
}

TEST_CASE("autoviz::stage1 json array of objects unfolds to rows") {
    const char* json = "[{\"a\":1,\"b\":\"x\"},{\"a\":2,\"b\":\"y\"}]";
    const Result<JsonFrameResult> result = dataframe_from_json(json);
    REQUIRE(result.has_value());
    CHECK(result->frame.row_count == 2);
    CHECK(result->frame.headers == std::vector<std::string>{"a", "b"});
    CHECK(result->frame.columns[1][1].raw == "y");
}

TEST_CASE("autoviz::stage1 json scalar top level rejected (not a record)") {
    const Result<JsonFrameResult> result = dataframe_from_json("42");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("autoviz::stage1 no stats at parse: raw text preserved verbatim") {
    // 阶段一只建轻底座：脏值原样留在 raw 里，统计量一个都不算。
    const char* jsonl = "{\"price\":\"$1,234.50\",\"pct\":\"15%\"}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const Cell& price = result->frame.columns[0][0];
    CHECK(price.raw == "$1,234.50");  // 原文不丢
    CHECK(price.is_numeric());        // 但同时给出清洗后的数值与标记
    CHECK(price.num == 1234.50);
    const Cell& pct = result->frame.columns[1][0];
    CHECK(pct.raw == "15%");
    CHECK(pct.is_numeric());
    CHECK(pct.num == 0.15);
}

TEST_CASE("autoviz::stage1 jsonl dirty string values cleaned via scalar_parse") {
    const char* jsonl = "{\"amount\":\"(1,000)\",\"note\":\"N/A\"}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    CHECK(result->frame.columns[0][0].is_numeric());
    CHECK(result->frame.columns[0][0].num == -1000.0);
    CHECK(result->frame.columns[1][0].is_missing());  // NA 族 → 缺失记号
}

// ── 阶段二：特征探测 ────────────────────────────────────────────────────────

TEST_CASE("autoviz::stage2 column kind inference") {
    const DataFrame frame = parse_dataframe(
        "i,f,b,t,ts,empty\n"
        "1,1.5,true,hello,2024-01-01,\n"
        "2,2.5,false,world,2024-01-02 08:30:00,\n"
        "3,3.5,yes,x,2024-01-03T09:00:00Z,\n"
        "4,4.5,no,y,2024-01-04,\n");
    const std::vector<ColumnProfile> profiles = profile_columns(frame);
    REQUIRE(profiles.size() == 6);
    CHECK(profiles[0].kind == ColumnKind::Numeric);
    CHECK(profiles[1].kind == ColumnKind::Numeric);
    CHECK(profiles[2].kind == ColumnKind::Boolean);
    CHECK(profiles[3].kind == ColumnKind::Text);
    CHECK(profiles[4].kind == ColumnKind::Timestamp);
    CHECK(profiles[5].kind == ColumnKind::NullOnly);
}

TEST_CASE("autoviz::stage2 timestamp shapes recognized") {
    CHECK(looks_like_timestamp("2024-01-01"));
    CHECK(looks_like_timestamp("2024-01-01 08:30"));
    CHECK(looks_like_timestamp("2024-01-01 08:30:59"));
    CHECK(looks_like_timestamp("2024-01-01 08:30:59.123"));
    CHECK(looks_like_timestamp("2024-01-01T08:30:59Z"));
    CHECK(looks_like_timestamp("2024-01-01T08:30:59+08:00"));
    CHECK_FALSE(looks_like_timestamp("2024-1-1"));    // 未补零
    CHECK_FALSE(looks_like_timestamp("20240101"));    // 无分隔符（归 Numeric）
    CHECK_FALSE(looks_like_timestamp("hello"));
    CHECK_FALSE(looks_like_timestamp(""));
}

TEST_CASE("autoviz::stage2 cardinality tiers: constant/low/mid/high") {
    const CardinalityPolicy policy;  // 20 / 200

    const std::vector<ColumnProfile> constant = profile_columns(parse_dataframe(csv_with_distinct(1)));
    CHECK(constant[0].tier == CardinalityTier::Constant);
    CHECK(tier_of(1, policy) == CardinalityTier::Constant);

    const std::vector<ColumnProfile> low = profile_columns(parse_dataframe(csv_with_distinct(3)));
    CHECK(low[0].tier == CardinalityTier::Low);

    const std::vector<ColumnProfile> mid = profile_columns(parse_dataframe(csv_with_distinct(30)));
    CHECK(mid[0].tier == CardinalityTier::Mid);
    CHECK(mid[0].distinct == 30);

    // 默认 cap=256 > mid_max=200：熔断即 High，且 tier 可信。
    const std::vector<ColumnProfile> high =
        profile_columns(parse_dataframe(csv_with_distinct(500)));
    CHECK(high[0].distinct_capped);
    CHECK(high[0].tier == CardinalityTier::High);
}

TEST_CASE("autoviz::stage2 cardinality policy is adjustable (not a hardcoded constant)") {
    const CardinalityPolicy strict{2, 4};  // 3 distinct 就该算 Mid
    const std::vector<ColumnProfile> profiles =
        profile_columns(parse_dataframe(csv_with_distinct(3)), ProfileOptions{strict});
    CHECK(profiles[0].tier == CardinalityTier::Mid);
    const std::vector<ColumnProfile> dflt =
        profile_columns(parse_dataframe(csv_with_distinct(3)));
    CHECK(dflt[0].tier == CardinalityTier::Low);  // 默认策略下仍是 Low
}

TEST_CASE("autoviz::stage2 distinct probe short-circuits at cap") {
    ProfileOptions options;
    options.distinct_cap = 4;  // 用户主动用小 cap 求快
    const std::vector<ColumnProfile> profiles =
        profile_columns(parse_dataframe(csv_with_distinct(100)), options);
    CHECK(profiles[0].distinct_capped);
    CHECK(profiles[0].distinct == 5);  // cap+1：下界，不是精确值
    CHECK(cardinality_tier_name(profiles[0].tier) == std::string("low"));  // 下界档位
}

TEST_CASE("autoviz::stage2 null rate per event bucket keeps key field") {
    // 规格书原案：duration_min 全局空值率高，但在 return 桶里几乎不空——
    // 全局口径会误杀，分桶口径才留得住。
    const DataFrame frame = parse_dataframe(
        "event,duration_min\n"
        "login,\n"
        "logout,\n"
        "return,7\n"
        "return,9\n"
        "return,11\n");
    const std::vector<ColumnProfile> profiles = profile_columns(frame);
    const ColumnProfile* duration = find_column(profiles, "duration_min");
    REQUIRE(duration != nullptr);
    CHECK(duration->null_rate == 0.4);  // 全局 2/5——单看这个数就该丢

    const std::vector<BucketNullRate> buckets =
        null_rate_by_bucket(frame, "event", "duration_min");
    REQUIRE(buckets.size() == 3);
    CHECK(buckets[0].key == "login");
    CHECK(buckets[0].null_rate == 1.0);
    CHECK(buckets[1].key == "logout");
    CHECK(buckets[1].null_rate == 1.0);
    CHECK(buckets[2].key == "return");
    CHECK(buckets[2].rows == 3);
    CHECK(buckets[2].null_rate == 0.0);  // 桶内极低 → 保留
}

TEST_CASE("autoviz::stage2 per-bucket null rate skips rows with missing key") {
    const DataFrame frame = parse_dataframe(
        "event,v\n"
        "a,1\n"
        ",2\n"      // event 缺失 → 无法归属，跳过
        "a,\n"
        "b,3\n");
    const std::vector<BucketNullRate> buckets = null_rate_by_bucket(frame, "event", "v");
    REQUIRE(buckets.size() == 2);
    CHECK(buckets[0].key == "a");
    CHECK(buckets[0].rows == 2);
    CHECK(buckets[0].null_rate == 0.5);
    CHECK(buckets[1].key == "b");
    CHECK(buckets[1].rows == 1);
    CHECK(buckets[1].null_rate == 0.0);
}

TEST_CASE("autoviz::stage2 bucket query on missing column returns empty (global fallback)") {
    const DataFrame frame = parse_dataframe("a,b\n1,2\n");
    CHECK(null_rate_by_bucket(frame, "event", "b").empty());
    CHECK(null_rate_by_bucket(frame, "a", "nope").empty());
}

TEST_CASE("autoviz::stage2 numeric domain and monotonicity") {
    const DataFrame frame = parse_dataframe("v\n3\n1\n2\n");
    const std::vector<ColumnProfile> profiles = profile_columns(frame);
    CHECK(profiles[0].min == 1.0);
    CHECK(profiles[0].max == 3.0);
    CHECK_FALSE(profiles[0].monotonic_nondecreasing);  // 3,1,2 乱序

    const std::vector<ColumnProfile> sorted = profile_columns(parse_dataframe("v\n1\n2\n3\n"));
    CHECK(sorted[0].monotonic_nondecreasing);

    const std::vector<ColumnProfile> text = profile_columns(parse_dataframe("v\na\nb\n"));
    CHECK(is_nan(text[0].min));
    CHECK(is_nan(text[0].max));
}

TEST_CASE("autoviz::stage2 empty frame and all-missing column") {
    const std::vector<ColumnProfile> empty = profile_columns(parse_dataframe(""));
    CHECK(empty.empty());

    const std::vector<ColumnProfile> nulls =
        profile_columns(parse_dataframe("v\nNA\n\n-\n"));
    REQUIRE(nulls.size() == 1);
    CHECK(nulls[0].kind == ColumnKind::NullOnly);
    CHECK(nulls[0].non_null == 0);
    CHECK(nulls[0].null_rate == 1.0);
}

TEST_CASE("autoviz::stage2 profile is per-column single pass with plottable flags") {
    const DataFrame frame = parse_dataframe("cat,val\nx,1\ny,2\nx,3\n");
    const std::vector<ColumnProfile> profiles = profile_columns(frame);
    const ColumnProfile* cat = find_column(profiles, "cat");
    const ColumnProfile* val = find_column(profiles, "val");
    REQUIRE(cat != nullptr);
    REQUIRE(val != nullptr);
    CHECK(cat->is_categorical());
    CHECK_FALSE(cat->is_plottable_value());
    CHECK(val->is_plottable_value());
    CHECK(val->distinct == 3);
    CHECK(cat->distinct == 2);
}
