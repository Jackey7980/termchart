// legend_toggle.hpp —— 图例点击切换
//
// 职责切分：图例的**画**归渲染侧（chart/backend 想怎么画都行），本文件只管
// "点了图例之后系列可见性怎么变"。交互层不知道"系列"是什么——它只维护
// id → 可见性 的映射，而 id 的语义由登记方（把图例项的 HitRegion payload
// 设成 series id 的人）决定。这与 hit_test 的"不透明 payload"约定一致。
//
// 不变量：
//   * 未登记的 id 视为可见（visible() 返回 true）——这样新增系列不需要先登记；
//   * click() 只在 id 已登记且状态真的翻转时返回 true（供调用方决定要不要重绘）；
//   * hidden() 的顺序与登记顺序一致（渲染侧可以稳定地画"已隐藏"角标）。
//
// 退化输入：payload == 0 是 HitRegion 的默认值，**不**当作合法系列 id
// （登记时 id 为 0 会被拒绝），避免"点中了没登记的图元却切掉了某个系列"。
#ifndef TERMCHART_INTERACTION_LEGEND_TOGGLE_HPP
#define TERMCHART_INTERACTION_LEGEND_TOGGLE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

class LegendToggle {
public:
    // 登记一个系列（id 必须非 0；重复登记幂等，不更新 label）。
    // 返回是否真正登记（false = id 为 0 或已存在）。
    TERMCHART_API bool register_series(std::uint32_t id, std::string label);

    [[nodiscard]] TERMCHART_API std::size_t series_count() const noexcept;

    // 未登记的 id 视为可见。
    [[nodiscard]] TERMCHART_API bool visible(std::uint32_t id) const noexcept;
    TERMCHART_API void set_visible(std::uint32_t id, bool visible) noexcept;
    TERMCHART_API void toggle(std::uint32_t id) noexcept;

    TERMCHART_API void show_all() noexcept;
    TERMCHART_API void hide_all() noexcept;

    // 点击路由：payload 即 series id。已登记且发生翻转 → true；未登记 → false。
    [[nodiscard]] TERMCHART_API bool click(std::uint32_t payload) noexcept;

    // 便利路径：在图例的 HitRegion 集合里做命中测试，命中则切换其 payload 对应系列。
    // （图例项登记为 HitRegion::rect 且 payload = series id；0 payload 视为未登记。）
    [[nodiscard]] TERMCHART_API bool click_at(const std::vector<chart::HitRegion>& regions,
                                              double x, double y) noexcept;

    // 隐藏中的系列 id（按登记顺序）；全部可见时为空。
    [[nodiscard]] TERMCHART_API std::vector<std::uint32_t> hidden() const;

    // 未登记的 id 返回空串。
    [[nodiscard]] TERMCHART_API std::string label(std::uint32_t id) const;

private:
    struct Entry {
        std::uint32_t id = 0;
        std::string label;
        bool visible = true;
    };
    std::vector<Entry> entries_;   // 按登记顺序，天然支持 hidden() 的稳定顺序

    // 查找辅助：Entry 是私有嵌套类型，不能写在匿名 namespace 的自由函数里。
    static Entry* find(std::vector<Entry>& entries, std::uint32_t id) noexcept;
    static const Entry* find(const std::vector<Entry>& entries, std::uint32_t id) noexcept;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_LEGEND_TOGGLE_HPP
