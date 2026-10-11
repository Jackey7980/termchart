// auto_domain.hpp —— 由数据范围反推「好看」的值域
//
// 图表不该把轴正好卡在 min/max 上（看着压抑且易出界）。这里按 5% 留白后，
// 把两端对齐到 nice 步长；可选 include_zero 让坐标轴穿过原点（财务/差值图常用）。
#ifndef TERMCHART_SCALE_AUTO_DOMAIN_HPP
#define TERMCHART_SCALE_AUTO_DOMAIN_HPP

#include "termchart/export.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

struct Domain {
    double min = 0.0;
    double max = 0.0;
    bool ok = false;
};

// data_min/data_max 为数据极值；include_zero 时值域下界不超过 0。
TERMCHART_API Domain nice_domain(double data_min, double data_max,
                                 bool include_zero = false) noexcept;

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_AUTO_DOMAIN_HPP
