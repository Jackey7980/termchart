// autoviz/domain.hpp —— 稳健 Y 轴域：分位数裁剪（L21）
//
// 问题：一列里混进一个 1e21（脏数据/单位错/哨兵值），[min, max] 域会把其余
// 全部数据压成贴地的一条平线——图还在画，信息量归零。商业 BI 的标准做法是
// **分位数裁剪**：轴域不取极值，取"篱"内的范围，篱外的点钳到边缘并明确标注。
//
// 为什么不用 P1/P99：这两个分位数对**小样本**毫无意义（11 个值时 P99 就是
// max，裁不掉任何东西；本仓库真实脏订单 CSV 只有 6 个有效值）。改用 Tukey
// 篱 Q1/Q3 ± k·IQR（箱线图的须），5 个点就能判、对单个极端值稳健。
//
// 但"判得出离群"不等于"该裁轴"：样本太少（< kOutlierMinSamples）或越界占比
// 过高（> kOutlierMaxClipRatio）时裁剪会把少数点强行拉满整根轴，读者会以为
// 波动剧烈。两道保护闸见下，命中时退回原始域，但**告警照发**——脏值该被看见。
//
// 为什么不在 data::profile_columns 里算：那里是"整表单遍画像"的公共底座，
// 目标是判定"这列像什么"，替所有调用方（含根本不画图的）付排序开销不合适。
// 分位数只在**真要定轴**的地方算一次，且只对已聚合的少量值算。
#ifndef TERMCHART_AUTOVIZ_DOMAIN_HPP
#define TERMCHART_AUTOVIZ_DOMAIN_HPP

#include <cmath>
#include <cstddef>
#include <span>

#include "termchart/export.hpp"

namespace termchart::autoviz {

// Tukey 篱系数：Q3 + k·IQR 之外算极端值。1.5 = 箱线图默认须长，业界标准；
// 调小会更激进地裁（1.0 已接近"正常波动也裁"），调大会漏掉中等离群。
inline constexpr double kOutlierFence = 1.5;

// 有效样本下限：少于这么多点一律不裁（宁可画平线，也不要在 3 个点的样本上
// "猜"哪个是离群）。**为什么是 10 而不是 5**：5 个点时 Tukey 篱判得出"谁是
// 离群"，但样本太小，裁掉 1 个就占了 20%，剩下的点在轴上被强行拉伸——那是
// 视觉欺骗（读者会以为波动很大），比略微压平更糟。样本量与裁剪率两道闸配合，
// 一起挡住"小样本强行放大"。
inline constexpr std::size_t kOutlierMinSamples = 10;

// 越界占比上限：越界值占抽样值的比例超过这个数，就不再视为"少数脏值"，
// 而是"整列本来就离散"——此时裁剪等于人为制造趋势，必须退回原始域。
// 反例（本仓库真实数据）：8 个有效值里裁掉 2 个 = 25%，剩下的 6 个点被拉满
// 整根轴，图看着像剧烈波动，实际业务量级平稳。
inline constexpr double kOutlierMaxClipRatio = 0.20;

// 裁剪决策：**检测到离群**与**真的裁了轴**是两件事，必须分开表达——
// 否则告警会说"已裁剪"、标题会说"裁去 N 个"，而轴其实一动没动（在骗人）。
enum class ClipDecision {
    kNone,         // 没有越界值：轴域就是原始 [min, max]
    kApplied,      // 越界值少且样本足够 → 已裁剪
    kSmallSample,  // 检测到越界，但有效值太少 → 放弃裁剪（保护闸一）
    kTooMany,      // 检测到越界，但越界占比过高 → 放弃裁剪（保护闸二）
};

// 一列的稳健域扫描结果（纯值语义，无所有权）。
struct RobustDomain {
    double raw_lo = 0.0;  // 实际极值（诊断/告警用）
    double raw_hi = 0.0;
    double lo = 0.0;      // 建议轴域（放弃裁剪时等于 raw）
    double hi = 0.0;
    double median = 0.0;   // 中位数（告警里给"主体在哪"的锚）
    std::size_t clipped = 0;  // 篱外的点数（**诊断口径**：放弃裁剪时也会统计）
    bool high = true;         // 越界主要发生在高端（true）还是低端
    ClipDecision decision = ClipDecision::kNone;

    // 域可用：两界有限且非退化。退化域会让比例尺 span=0（除零/全 NaN），
    // 所以三处调用点都必须先问这一句。
    [[nodiscard]] bool usable() const noexcept {
        return std::isfinite(lo) && std::isfinite(hi) && hi > lo;
    }
    // 真的裁了：域比原始范围更紧，且可用。false 表示轴域就是原始 min/max。
    [[nodiscard]] bool active() const noexcept {
        return decision == ClipDecision::kApplied && usable() && (lo > raw_lo || hi < raw_hi);
    }
    // 检测到越界但主动放弃裁剪（小样本保护闸）：告警要说"没裁"，不许装没事。
    [[nodiscard]] bool abandoned() const noexcept {
        return (decision == ClipDecision::kSmallSample || decision == ClipDecision::kTooMany) &&
               clipped > 0;
    }
    // 告警/标题里要展示的那个值。
    [[nodiscard]] double extreme() const noexcept { return high ? raw_hi : raw_lo; }
};

// 扫描一组值，给出稳健轴域。非有限值（NaN/Inf）直接跳过（空值不是离群）。
// fence <= 0 视为关闭裁剪（返回原始 [min, max]）。
[[nodiscard]] TERMCHART_API RobustDomain robust_y_domain(std::span<const double> values,
                                                         double fence = kOutlierFence) noexcept;

// 多系列并集：同一根 Y 轴上的系列必须共享一个域（域取并、裁剪点数相加）。
// 系列间量级差异极大时这是**有意**的保守选择——那种情形该换对数/对称对数
// 轴（见 docs/reference/backlog.md 的 SymLog 条目），而不是让每系列各自裁
// 一套、让读者以为它们同量级。
[[nodiscard]] TERMCHART_API RobustDomain merge_domains(const RobustDomain& a,
                                                       const RobustDomain& b) noexcept;

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_DOMAIN_HPP
