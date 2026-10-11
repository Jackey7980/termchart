// hit_test.hpp —— 命中测试（只做几何，禁图表名）
//
// 约束（ROADMAP 阶段 3）：本文件**不得出现任何图表类型名**。它只调用
// chart::HitRegion 的几何接口（形状判定封装在 HitRegion::contains 内），
// 因此新增图表类型不需要改这里——这正是"通用 HitRegion"的意义。
#ifndef TERMCHART_INTERACTION_HIT_TEST_HPP
#define TERMCHART_INTERACTION_HIT_TEST_HPP

#include <cstdint>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

// 最上面（**最后**登记）的命中项；无命中返回 nullptr。
[[nodiscard]] TERMCHART_API const chart::HitRegion* topmost_hit(
    const std::vector<chart::HitRegion>& regions, double x, double y) noexcept;

// 全部命中项的 payload，自下而上（登记顺序）。
[[nodiscard]] TERMCHART_API std::vector<std::uint32_t> hit_payloads(
    const std::vector<chart::HitRegion>& regions, double x, double y);

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_HIT_TEST_HPP
