// test_util_misc.cpp —— bits / math / random / format / logging
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/util/bits.hpp"
#include "termchart/util/format.hpp"
#include "termchart/util/logging.hpp"
#include "termchart/util/math.hpp"
#include "termchart/util/random.hpp"

namespace {

using namespace termchart::util;

TEST_CASE("util: 位操作") {
    CHECK(bit32(0) == 1u);
    CHECK(bit32(31) == (1u << 31));
    CHECK(bit32(32) == 0u);  // 越界安全
    CHECK(popcount(std::uint8_t{0b10110}) == 3);
    CHECK(test_bit(std::uint8_t{0b0100}, 2));
    CHECK(set_bit(std::uint8_t{0}, 3) == std::uint8_t{0b1000});
    CHECK(clear_bit(std::uint8_t{0b1010}, 1) == std::uint8_t{0b1000});
    CHECK(low_mask(3) == 0b111u);
    CHECK(rotl8(std::uint8_t{0b00000001}, 1) == std::uint8_t{0b00000010});
    CHECK(rotl8(std::uint8_t{0b10000000}, 1) == std::uint8_t{0b00000001});
}

TEST_CASE("util: 数学工具") {
    CHECK(clamp(5, 0, 3) == 3);
    CHECK(clamp(-1, 0, 3) == 0);
    CHECK(lerp(0.0, 10.0, 0.25) == 2.5);
    CHECK(clamp_lerp(0.0, 10.0, 5.0) == 10.0);   // t 被钳位
    CHECK(normalize(5.0, 0.0, 10.0) == 0.5);
    CHECK(normalize(1.0, 5.0, 5.0) == 0.0);      // 退化区间不产生 NaN
    CHECK(smoothstep(0.0) == 0.0);
    CHECK(smoothstep(1.0) == 1.0);
    CHECK(sign(-3) == -1);
    CHECK(sign(0) == 0);
    CHECK(nearly_equal(1.0, 1.0 + 1e-12));
    CHECK_FALSE(is_finite(1.0 / 0.0));
}

TEST_CASE("util: 伪随机可复现") {
    Rng first(42);
    Rng second(42);
    CHECK(first.next() == second.next());
    CHECK(first.unit() == second.unit());

    Rng other(43);
    CHECK(first.next() != other.next());

    SUBCASE("取值落在区间内") {
        Rng rng(7);
        for (int i = 0; i < 100; ++i) {
            const double value = rng.range(-1.0, 1.0);
            CHECK(value >= -1.0);
            CHECK(value < 1.0);
        }
    }
}

TEST_CASE("util: 格式化与 locale 无关") {
    CHECK(format_double(1.5, 2) == "1.50");
    CHECK(format_double_trimmed(1.50, 2) == "1.5");
    CHECK(format_double_trimmed(1.0, 3) == "1");
    CHECK(format_int(-42) == "-42");
    CHECK(format_double(1.0 / 0.0, 1) == "inf");
    CHECK(format_double(0.0 / 0.0, 1) == "nan");
    CHECK(format_compact(2000.0) == "2k");
}

TEST_CASE("util: 日志分级与捕获") {
    set_log_level(LogLevel::Warn);
    CHECK(log_level() == LogLevel::Warn);
    CHECK(std::string(level_name(LogLevel::Error)) == "error");

    capture_log_begin();
    set_log_level(LogLevel::Trace);
    TERMCHART_LOG_ERROR("boom");
    TERMCHART_LOG_TRACE("ignored by level? no: captured here");
    set_log_level(LogLevel::Warn);
    const std::vector<std::string> captured = capture_log_end();

    REQUIRE(captured.size() == 2);
    CHECK(captured[0].find("error") != std::string::npos);
    CHECK(captured[1].find("trace") != std::string::npos);
}

}  // namespace
