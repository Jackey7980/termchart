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

// ── 位置式数组行（JSONL 里"只有值、没有键"的导出形态）───────────────────────
//
// 真实脏数据里出现过：首行是带键对象建立 schema，之后的行却是裸数组
// (["X004", 1740787200, "(123.45)", ...])。旧实现把整个数组当"非记录行"丢掉，
// 于是那些行静默消失（图上少点、均值变样，还没人告诉用户）。
// 救回来的前提是**宽度严格相等**：列序 = 首现序，宽度相同才说明这条数组与既有
// schema 同宽；宽度不符一律放弃——把 5 个元素摊到 14 列上，"X004" 会落进 ts 列，
// 得到一行看着合法、实际全错位的假记录（比丢行更糟：它会污染时间轴与量程）。

TEST_CASE("autoviz::stage1 jsonl positional array zipped onto existing schema") {
    const char* jsonl =
        "{\"id\":\"X001\",\"ts\":\"2026-03-01 08:00\",\"amount\":\"1,234.50\"}\n"
        "[\"X004\",\"2026-03-01 09:00\",\"(123.45)\"]\n"
        "{\"id\":\"X005\",\"ts\":\"2026-03-01 10:00\",\"amount\":900}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const DataFrame& frame = result->frame;

    CHECK(frame.headers == std::vector<std::string>{"id", "ts", "amount"});
    CHECK(frame.row_count == 3);   // 数组行不再消失
    CHECK(result->lines_used == 3);
    CHECK(result->lines_skipped == 0);
    CHECK(result->lines_zipped == 1);

    // 按位置落到对应列（而不是整体塞进某一列）。
    CHECK(frame.columns[0][1].raw == "X004");
    CHECK(frame.columns[1][1].raw == "2026-03-01 09:00");
    CHECK(frame.columns[2][1].is_numeric());
    CHECK(frame.columns[2][1].num == -123.45);  // 会计负号照样清洗，与对象行同口径
    CHECK(frame.columns[2][2].num == 900.0);
}

TEST_CASE("autoviz::stage1 jsonl positional array with wrong width is dropped, never guessed") {
    const char* jsonl =
        "{\"id\":\"X001\",\"ts\":\"2026-03-01 08:00\",\"amount\":1234}\n"
        "[\"X004\",1740787200]\n"  // 2 个元素 vs 3 列：不猜是哪两列
        "{\"id\":\"X005\",\"ts\":\"2026-03-01 10:00\",\"amount\":900}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const DataFrame& frame = result->frame;

    CHECK(frame.row_count == 2);
    CHECK(result->lines_zipped == 0);
    CHECK(result->lines_skipped == 1);  // 丢弃要计数（可观测），不能静默
    // 关键：错位数据绝不许进表——否则 1740787200 会被当成 ts，
    // 把 2025-03-01 的假年份带进 2026 年的时间轴。
    for (const Cell& cell : frame.columns[1]) {
        CHECK(cell.raw != "1740787200");
    }
}

TEST_CASE("autoviz::stage1 jsonl positional array with no schema yet is dropped") {
    // 首行就是数组：没有列序可对齐，只能丢（不替它编列名 col0/col1）。
    const char* jsonl =
        "[\"X001\", 100]\n"
        "{\"id\":\"X002\",\"amount\":200}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    CHECK(result->frame.row_count == 1);
    CHECK(result->lines_zipped == 0);
    CHECK(result->lines_skipped == 1);
    CHECK(result->frame.headers == std::vector<std::string>{"id", "amount"});
}

TEST_CASE("autoviz::stage1 jsonl object array still unfolds to rows (not zipped)") {
    // 位置式 zip 只作用于**标量数组**；对象数组仍是"一行多记录"的批量形态，
    // 即使当前 schema 宽度恰好等于数组长度也不能被当成一条记录。
    const char* jsonl =
        "{\"a\":1,\"b\":2}\n"
        "[{\"a\":3,\"b\":4},{\"a\":5,\"b\":6}]\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    CHECK(result->frame.row_count == 3);
    CHECK(result->lines_zipped == 0);
    CHECK(result->frame.columns[0][2].num == 5.0);
    CHECK(result->frame.columns[1][2].num == 6.0);
}

// ── 量纲标记（JSON 轨必须与 CSV 轨同口径）──────────────────────────────────

TEST_CASE("autoviz::stage2 json percent cells still carry Percent flag") {
    // 这条曾是真实 bug：JSON 轨解析字符串后忘了映射 ScalarFlag → CellFlag，
    // 于是 "15%" 只剩 0.15、标记全丢；同一份数据从 CSV 进来有标记、从 JSONL
    // 进来没有，列剖析给出两种结论。
    const char* jsonl =
        "{\"amount\":\"15%\",\"raw\":1234.5}\n"
        "{\"amount\":\"20%\",\"raw\":999}\n";
    const Result<JsonFrameResult> result = dataframe_from_jsonl(jsonl);
    REQUIRE(result.has_value());
    const Cell& first = result->frame.columns[0][0];
    CHECK(first.is_numeric());
    CHECK(first.num == 0.15);
    CHECK(has_cell_flag(first.flags, CellFlag::Percent));

    const std::vector<ColumnProfile> profiles = profile_columns(result->frame);
    REQUIRE(profiles.size() == 2);
    CHECK(profiles[0].percent_cells == 2);
    CHECK_FALSE(profiles[0].mixed_units);  // 整列都是百分比：量纲一致，不算冲突
    CHECK(profiles[1].percent_cells == 0);
    CHECK_FALSE(profiles[1].mixed_units);
}

TEST_CASE("autoviz::stage2 percent mixed with plain numbers flags mixed_units (both tracks)") {
    // 同一份数据走 CSV 与 JSONL 两条入口，结论必须一致。
    const char* csv =
        "amount\n"
        "1234.5\n"
        "15%\n"
        "999\n";
    const std::vector<ColumnProfile> from_csv = profile_columns(parse_dataframe(csv));
    REQUIRE(from_csv.size() == 1);
    CHECK(from_csv[0].percent_cells == 1);
    CHECK(from_csv[0].mixed_units);

    const char* jsonl =
        "{\"amount\":1234.5}\n"
        "{\"amount\":\"15%\"}\n"
        "{\"amount\":999}\n";
    const Result<JsonFrameResult> parsed = dataframe_from_jsonl(jsonl);
    REQUIRE(parsed.has_value());
    const std::vector<ColumnProfile> from_json = profile_columns(parsed->frame);
    REQUIRE(from_json.size() == 1);
    CHECK(from_json[0].percent_cells == 1);
    CHECK(from_json[0].mixed_units);
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

TEST_CASE("autoviz::stage2 timestamp shapes recognized (multi-separator family)") {
    // ISO 短横线族。
    CHECK(looks_like_timestamp("2024-01-01"));
    CHECK(looks_like_timestamp("2024-01-01 08:30"));
    CHECK(looks_like_timestamp("2024-01-01 08:30:59"));
    CHECK(looks_like_timestamp("2024-01-01 08:30:59.123"));
    CHECK(looks_like_timestamp("2024-01-01T08:30:59Z"));
    CHECK(looks_like_timestamp("2024-01-01T08:30:59+08:00"));
    // 斜杠/点/中文族 + 1-2 位不补零（真实导出常见；2026-10-24 按用户脏数据扩展）。
    CHECK(looks_like_timestamp("2026/03/01 09:00"));
    CHECK(looks_like_timestamp("2026.03.01 10:00"));
    CHECK(looks_like_timestamp("2026年3月2日"));
    CHECK(looks_like_timestamp("2026年3月2日9时30分"));
    CHECK(looks_like_timestamp("2024-1-1"));      // 未补零也认（形状）
    CHECK(looks_like_timestamp("2026/13/40"));    // 形状合法（历法非法由 parse 拒）
    // 非时间形状。
    CHECK_FALSE(looks_like_timestamp("20240101"));   // 无分隔符（归 Numeric）
    CHECK_FALSE(looks_like_timestamp("1740787200")); // Unix epoch 刻意不认
    CHECK_FALSE(looks_like_timestamp("hello"));
    CHECK_FALSE(looks_like_timestamp(""));
    CHECK_FALSE(looks_like_timestamp("2024-01"));    // 缺日
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
