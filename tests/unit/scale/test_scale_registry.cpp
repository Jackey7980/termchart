// test_scale_registry.cpp —— 比例尺注册表
#include <doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "termchart/scale/linear.hpp"
#include "termchart/scale/registry.hpp"
#include "termchart/scale/scale.hpp"

namespace {

using namespace termchart;
using namespace termchart::scale;

// 线性域 [0,10] → 显示 [0,100]，用来验证"拿到的确实是登记的那一个"。
class Doubler final : public Scale {
public:
    [[nodiscard]] double forward(double data) const noexcept override { return data * 2.0; }
    [[nodiscard]] double inverse(double pixel) const noexcept override { return pixel / 2.0; }
    [[nodiscard]] bool valid() const noexcept override { return true; }
    [[nodiscard]] double domain_min() const noexcept override { return 0.0; }
    [[nodiscard]] double domain_max() const noexcept override { return 1.0; }
    [[nodiscard]] double range_min() const noexcept override { return 0.0; }
    [[nodiscard]] double range_max() const noexcept override { return 1.0; }
};

}  // namespace

TEST_CASE("scale/registry: 内置 linear/log 可按名创建") {
    ScaleRegistry& registry = ScaleRegistry::instance();
    CHECK(registry.supports("linear"));
    CHECK(registry.supports("log"));
    CHECK_FALSE(registry.supports("nope"));

    const std::unique_ptr<Scale> linear = registry.create("linear");
    REQUIRE(linear != nullptr);
    CHECK(linear->forward(0.5) == doctest::Approx(0.5));  // 默认单位映射

    const std::unique_ptr<Scale> log = registry.create("log");
    REQUIRE(log != nullptr);
    CHECK(log->valid());

    CHECK(registry.create("nope") == nullptr);
}

TEST_CASE("scale/registry: 自定义登记可按名创建，且未知名字仍为 nullptr") {
    ScaleRegistry& registry = ScaleRegistry::instance();
    registry.register_scale("doubler", [] { return std::make_unique<Doubler>(); });

    CHECK(registry.supports("doubler"));
    const std::unique_ptr<Scale> scale = registry.create("doubler");
    REQUIRE(scale != nullptr);
    CHECK(scale->forward(3.0) == doctest::Approx(6.0));
    CHECK(registry.create("still-nope") == nullptr);
}

TEST_CASE("scale/registry: 登记名覆盖内置名，names() 去重且有序") {
    ScaleRegistry& registry = ScaleRegistry::instance();
    registry.register_scale("linear", [] { return std::make_unique<Doubler>(); });
    const std::unique_ptr<Scale> overridden = registry.create("linear");
    REQUIRE(overridden != nullptr);
    CHECK(overridden->forward(1.0) == doctest::Approx(2.0));  // 自定义实现生效

    const std::vector<std::string> names = registry.names();
    CHECK(std::is_sorted(names.begin(), names.end()));
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());  // 无重复
    CHECK(std::find(names.begin(), names.end(), "linear") != names.end());
    CHECK(std::find(names.begin(), names.end(), "doubler") != names.end());
}
