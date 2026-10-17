// test_scale_discrete.cpp —— 离散（band / category）与时间比例尺 + nice_domain
//
// scale 模块此前只测了 linear / log / ticks / nice / registry，band / category /
// time / auto_domain 四个头已实现并登记进注册表，却一个断言都没有。这里补齐。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "termchart/scale/auto_domain.hpp"
#include "termchart/scale/band.hpp"
#include "termchart/scale/category.hpp"
#include "termchart/scale/registry.hpp"
#include "termchart/scale/time.hpp"

namespace {

using namespace termchart;
using namespace termchart::scale;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

}  // namespace

TEST_CASE("scale/band: 类别落在等宽带中心") {
    const BandScale band(3, 0.0, 100.0);
    CHECK(band.categories() == 3);
    CHECK(band.valid());
    CHECK(band.band_width() == doctest::Approx(100.0 / 3.0));
    CHECK(band.domain_min() == doctest::Approx(0.0));
    CHECK(band.domain_max() == doctest::Approx(2.0));
    CHECK(band.clamped());

    // 第 i 个类别落在第 i 条带的中心：0.5 / 1.5 / 2.5 个带宽。
    CHECK(band.forward(0.0) == doctest::Approx(100.0 / 6.0));
    CHECK(band.forward(1.0) == doctest::Approx(50.0));
    CHECK(band.forward(2.0) == doctest::Approx(100.0 - 100.0 / 6.0));
    // 往返：像素 → 类别索引。
    CHECK(band.inverse(band.forward(1.0)) == doctest::Approx(1.0));
}

TEST_CASE("scale/band: clamp 关闭时不夹，padding 被限制在 [0,0.5]") {
    BandScale band(3, 0.0, 100.0);
    band.set_clamp(false);
    CHECK_FALSE(band.clamped());
    CHECK(band.forward(-1.0) == doctest::Approx(-100.0 / 6.0));
    CHECK(band.forward(5.0) == doctest::Approx(5.5 * 100.0 / 3.0));

    // clamp 的下界是 rmin + inner/2，inner 随 padding 收缩：padding 0.1 → 下界 13.33，
    // set_padding(0.9) 被夹到 0.5 → inner 归零 → 下界回到 rmin。
    BandScale padded(3, 0.0, 100.0, 0.1);
    CHECK(padded.forward(-10.0) == doctest::Approx(13.3333333));
    padded.set_padding(0.9);
    CHECK(padded.forward(-10.0) == doctest::Approx(0.0));
}

TEST_CASE("scale/band: 空类别与退化值域不 valid") {
    const BandScale empty;
    CHECK_FALSE(empty.valid());
    CHECK(empty.band_width() == doctest::Approx(0.0));
    CHECK(empty.forward(0.0) == doctest::Approx(0.0));

    const BandScale degenerate(3, 50.0, 50.0);  // rmax == rmin
    CHECK_FALSE(degenerate.valid());
}

TEST_CASE("scale/category: 类别落在等距网格点") {
    const CategoryScale cat(4, 0.0, 90.0);
    CHECK(cat.categories() == 4);
    CHECK(cat.valid());
    CHECK(cat.forward(0.0) == doctest::Approx(0.0));
    CHECK(cat.forward(1.0) == doctest::Approx(30.0));
    CHECK(cat.forward(3.0) == doctest::Approx(90.0));
    // 越界被 clamp 到端点。
    CHECK(cat.forward(9.0) == doctest::Approx(90.0));
    CHECK(cat.forward(-1.0) == doctest::Approx(0.0));
    CHECK(cat.inverse(90.0) == doctest::Approx(3.0));
    CHECK(cat.inverse(0.0) == doctest::Approx(0.0));
    CHECK(cat.domain_max() == doctest::Approx(3.0));
}

TEST_CASE("scale/category: 单类别退化到 range_min，空类别不 valid") {
    const CategoryScale one(1, 5.0, 10.0);
    CHECK(one.valid());
    CHECK(one.forward(0.0) == doctest::Approx(5.0));
    CHECK(one.domain_max() == doctest::Approx(0.0));

    const CategoryScale none;
    CHECK_FALSE(none.valid());
}

TEST_CASE("scale/time: 时间戳线性映射且可逆") {
    const TimeScale time(1000.0, 2000.0, 0.0, 50.0);
    CHECK(time.valid());
    CHECK(time.clamped());
    CHECK(time.forward(1000.0) == doctest::Approx(0.0));
    CHECK(time.forward(2000.0) == doctest::Approx(50.0));
    CHECK(time.forward(1500.0) == doctest::Approx(25.0));
    CHECK(time.inverse(25.0) == doctest::Approx(1500.0));
    // 往返：域内任意点回到原值。
    CHECK(time.inverse(time.forward(1234.0)) == doctest::Approx(1234.0));
    // 越域被夹住。
    CHECK(time.forward(3000.0) == doctest::Approx(50.0));
}

TEST_CASE("scale/time: clamp 关闭后可越域，退化/非有限域不 valid") {
    TimeScale time(1000.0, 2000.0, 0.0, 50.0);
    time.set_clamp(false);
    CHECK_FALSE(time.clamped());
    CHECK(time.forward(3000.0) == doctest::Approx(100.0));

    const TimeScale degenerate(5.0, 5.0, 0.0, 1.0);  // dmin == dmax
    CHECK_FALSE(degenerate.valid());

    const TimeScale nan_domain(kNaN, 1.0);
    CHECK_FALSE(nan_domain.valid());
}

TEST_CASE("scale/nice_domain: 值域外包住数据并吸附到 nice 步长") {
    const Domain d = nice_domain(3.0, 7.0);
    CHECK(d.ok);
    CHECK(d.min <= 3.0);
    CHECK(d.max >= 7.0);
    CHECK(d.min < d.max);

    // include_zero：下界压到 0 以下（或正好 0），用于"轴穿过原点"的图。
    const Domain z = nice_domain(5.0, 8.0, true);
    CHECK(z.ok);
    CHECK(z.min <= 0.0);
    CHECK(z.max >= 8.0);

    // 退化数据（min == max）也给出非零宽域，避免零宽轴与除零。
    const Domain flat = nice_domain(5.0, 5.0);
    CHECK(flat.ok);
    CHECK(flat.min <= 4.5);
    CHECK(flat.max >= 5.5);
    CHECK(flat.min < flat.max);
}

TEST_CASE("scale/nice_domain: 非有限输入直接判失败") {
    CHECK_FALSE(nice_domain(kNaN, 1.0).ok);
    CHECK_FALSE(nice_domain(1.0, kNaN).ok);
    CHECK_FALSE(nice_domain(kNaN, kNaN).ok);
    CHECK_FALSE(nice_domain(std::numeric_limits<double>::infinity(), 1.0).ok);
}

TEST_CASE("scale/registry: band / category / time 三个内置名可造") {
    const ScaleRegistry& registry = ScaleRegistry::instance();
    for (const std::string name : {"band", "category", "time"}) {
        CHECK(registry.supports(name));
        const std::unique_ptr<Scale> scale = registry.create(name);
        REQUIRE(scale != nullptr);
    }
    CHECK_FALSE(registry.supports("no_such_scale"));
    CHECK(registry.create("no_such_scale") == nullptr);

    const std::vector<std::string> names = registry.names();
    const auto has = [&names](const std::string& want) {
        return std::find(names.begin(), names.end(), want) != names.end();
    };
    CHECK(has("band"));
    CHECK(has("category"));
    CHECK(has("time"));
    CHECK(has("linear"));
    CHECK(has("log"));
}
