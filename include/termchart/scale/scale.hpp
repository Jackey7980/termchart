// scale.hpp —— 数据坐标 ↔ 显示坐标的抽象（比例尺基类）
//
// 终端图表和 GUI 控件都要把「数据域」映射到「像素/归一域」。把这件事抽成
// 一个多态基类，是为了让 chart / graph / canvas 在**不知道具体映射**的情况下
// 工作：换线性为对数、换连续为分类，调用方代码一行不动。
//
// 不变量（所有派生类必须遵守）：
//   * forward(data) 单调；inverse(forward(x)) ≈ x（在 valid 域内）；
//   * 越界输入走 NaN 或夹紧，绝不未定义行为（数据来自用户 CSV，含 NaN）。
#ifndef TERMCHART_SCALE_SCALE_HPP
#define TERMCHART_SCALE_SCALE_HPP

#include "termchart/export.hpp"

namespace termchart::scale {

class Scale {
public:
    virtual ~Scale();

    // 数据坐标 → 显示坐标（像素或归一化 [0,1]）。
    [[nodiscard]] virtual double forward(double data) const noexcept = 0;
    // 显示坐标 → 数据坐标。
    [[nodiscard]] virtual double inverse(double pixel) const noexcept = 0;

    // 是否可逆向求解（对称对数等在零附近不可逆，返回 false）。
    [[nodiscard]] virtual bool invertible() const noexcept { return true; }
    // 域是否合法（未退化、对数域为正等）。
    [[nodiscard]] virtual bool valid() const noexcept = 0;

    [[nodiscard]] virtual double domain_min() const noexcept = 0;
    [[nodiscard]] virtual double domain_max() const noexcept = 0;
    [[nodiscard]] virtual double range_min() const noexcept = 0;
    [[nodiscard]] virtual double range_max() const noexcept = 0;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_SCALE_HPP
