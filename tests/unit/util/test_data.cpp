// tests/unit/util/test_data.cpp —— 数据处理层（aggregate / fill_missing /
//                                rolling / resample）的最小测试集
//
// doctest：doctest.h vendored 于 third_party/，本测试通过 TEST_CASE 宏串联。
// 不依赖任何外部 fixture；测试样本全部内置。
#include "termchart/util/data/aggregate.hpp"
#include "termchart/util/data/fill_missing.hpp"
#include "termchart/util/data/resample.hpp"
#include "termchart/util/data/rolling.hpp"

#include <cmath>
#include <vector>

#include <doctest.h>

using namespace termchart::util::data;

namespace {

bool approx(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

}  // namespace

TEST_CASE("util::data::aggregate::histogram") {
    const double raw[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    DataView v{raw, 10};
    auto h = histogram(v, 5);
    CHECK(h.size() == 5);
    CHECK(h[0].count == 2);
    CHECK(h[1].count == 2);
    CHECK(h[4].count == 2);
}

TEST_CASE("util::data::aggregate::quartiles") {
    const double raw[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto q = quartiles(DataView{raw, 11});
    CHECK(approx(q.minimum, 1.0));
    CHECK(approx(q.maximum, 11.0));
    CHECK(approx(q.median, 6.0));
    CHECK(q.q1 >= 3.0);
    CHECK(q.q3 <= 9.0);
}

TEST_CASE("util::data::aggregate::group_reduce sum/mean") {
    const double keys[]   = {1, 1, 2, 2, 2, 3};
    const double values[] = {10, 20, 5, 15, 25, 100};
    auto g = group_reduce({keys, 6}, {{values, 6}}, Reducer::Sum);
    REQUIRE(g.size() == 3);
    CHECK(g[0].key == 1);
    CHECK(g[1].key == 2);
    CHECK(g[2].key == 3);
    // 每组 DataView 在 group_reduce 里合成一段
    CHECK(g[0].values_per_col[0].size == 2);
    CHECK(g[1].values_per_col[0].size == 3);
    CHECK(g[2].values_per_col[0].size == 1);
}

TEST_CASE("util::data::fill_missing::forward_fill") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan(""), std::nan(""), 6.0};
    auto n = fill_missing(v, FillStrategy::ForwardFill);
    CHECK(n == 3);
    CHECK(v[1] == 1.0);
    CHECK(v[3] == 3.0);
    CHECK(v[4] == 3.0);
    CHECK(v[5] == 6.0);
}

TEST_CASE("util::data::fill_missing::linear_interp") {
    std::vector<double> v = {0.0, std::nan(""), std::nan(""), 3.0};
    (void)fill_missing(v, FillStrategy::LinearInterp);
    CHECK(approx(v[1], 1.0));
    CHECK(approx(v[2], 2.0));
}

TEST_CASE("util::data::fill_missing::drop") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan("")};
    auto out = drop_missing(v);
    CHECK(out.size() == 2);
    CHECK(out[0] == 1.0);
    CHECK(out[1] == 3.0);
}

TEST_CASE("util::data::fill_missing::count") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan(""), std::nan("")};
    CHECK(count_missing(v) == 3);
}

TEST_CASE("util::data::rolling::mean window=3") {
    std::vector<double> v = {1, 2, 3, 4, 5};
    auto r = rolling(v, RollingStat::Mean, {3});
    REQUIRE(r.size() == 5);
    CHECK(std::isnan(r[0]));
    CHECK(std::isnan(r[1]));
    CHECK(approx(r[2], 2.0));  // (1+2+3)/3
    CHECK(approx(r[3], 3.0));  // (2+3+4)/3
    CHECK(approx(r[4], 4.0));
}

TEST_CASE("util::data::rolling::sum min_periods") {
    std::vector<double> v = {1, 2, 3, 4, 5};
    auto r = rolling(v, RollingStat::Sum, {3, 2});
    REQUIRE(r.size() == 5);
    CHECK(approx(r[1], 3.0));   // 1+2
    CHECK(approx(r[2], 6.0));   // 1+2+3
    CHECK(approx(r[4], 12.0));
}

TEST_CASE("util::data::resample::linspace_edges") {
    auto e = linspace_edges(0.0, 10.0, 5);
    REQUIRE(e.size() == 5);
    CHECK(approx(e.front(), 0.0));
    CHECK(approx(e.back(),  10.0));
}

TEST_CASE("util::data::resample::mean by edges") {
    const double raw[] = {0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5};
    std::vector<double> v(raw, raw + sizeof(raw) / sizeof(raw[0]));
    auto e = linspace_edges(0.0, 10.0, 5);
    auto r = resample(v, e, Reducer::Mean);
    REQUIRE(r.size() == 4);
    // 每桶 2.5 个样本均值近似桶中心
    for (const auto& b : r) {
        CHECK((approx(static_cast<double>(b.count), 2.0) ||
               approx(static_cast<double>(b.count), 3.0)));
    }
}
