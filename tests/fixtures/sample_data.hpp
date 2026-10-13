// sample_data.hpp —— 由 tools/gen_fixtures.py 生成，禁手改。
//
// 用途：chart/dataset 与各类图种的通用输入样例——折线 / 柱 / 散点共用同一份数据，
// 让"同一组数在不同图种下的表现"可比。数值由生成器算出：x 等距、linear 等差、
// sine = sin(x/2)、counts 为非负计数（柱状语义）、scatter 含负值与重复 x。
//
// 消费方：tests/unit/test_fixtures.cpp（守夹具自身的不变量）。
#ifndef TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP
#define TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP

#include <array>
#include <cstddef>
#include <string_view>

namespace termchart::fixtures {

// 序列长度：所有 *_Series 的长度都等于它，方便调用方静态断言。
inline constexpr std::size_t kSeriesLength = 12;

// 一条 (x, y) 散点。
struct XyPoint {
    double x;
    double y;
};

inline constexpr std::array<double, 12> kXSeries{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
inline constexpr std::array<double, 12> kLinearSeries{{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23}};
inline constexpr std::array<double, 12> kSineSeries{{0, 0.479426, 0.841471, 0.997495, 0.909297, 0.598472, 0.14112, -0.350783, -0.756802, -0.97753, -0.958924, -0.70554}};
inline constexpr std::array<double, 12> kCountSeries{{3, 7, 5, 11, 9, 13, 8, 6, 12, 4, 10, 2}};

inline constexpr std::array<std::string_view, 12> kCategoryLabels{{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}};

inline constexpr std::array<XyPoint, 8> kScatterPoints{{
    {-2, 4},
    {-1, 1},
    {0, 0},
    {0.5, 0.25},
    {1, 1},
    {1.5, 2.25},
    {2, 4},
    {2.5, 6.25}
}};

}  // namespace termchart::fixtures

#endif  // TERMCHART_TESTS_FIXTURES_SAMPLE_DATA_HPP
