#include "termchart/interaction/legend_toggle.hpp"

#include "termchart/interaction/hit_test.hpp"

namespace termchart::interaction {

LegendToggle::Entry* LegendToggle::find(std::vector<Entry>& entries,
                                        std::uint32_t id) noexcept {
    for (auto& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

const LegendToggle::Entry* LegendToggle::find(const std::vector<Entry>& entries,
                                              std::uint32_t id) noexcept {
    for (const auto& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

bool LegendToggle::register_series(std::uint32_t id, std::string label) {
    if (id == 0) {
        return false;  // 0 是 HitRegion 的默认 payload，不能当系列 id（见头文件）
    }
    if (find(entries_, id) != nullptr) {
        return false;  // 重复登记幂等：不覆盖调用方已切换的可见性
    }
    Entry entry;
    entry.id = id;
    entry.label = std::move(label);
    entry.visible = true;
    entries_.push_back(std::move(entry));
    return true;
}

std::size_t LegendToggle::series_count() const noexcept { return entries_.size(); }

bool LegendToggle::visible(std::uint32_t id) const noexcept {
    const Entry* entry = find(entries_, id);
    return entry == nullptr || entry->visible;  // 未登记视为可见
}

void LegendToggle::set_visible(std::uint32_t id, bool visible) noexcept {
    if (Entry* entry = find(entries_, id)) {
        entry->visible = visible;
    }
}

void LegendToggle::toggle(std::uint32_t id) noexcept {
    if (Entry* entry = find(entries_, id)) {
        entry->visible = !entry->visible;
    }
}

void LegendToggle::show_all() noexcept {
    for (auto& entry : entries_) {
        entry.visible = true;
    }
}

void LegendToggle::hide_all() noexcept {
    for (auto& entry : entries_) {
        entry.visible = false;
    }
}

bool LegendToggle::click(std::uint32_t payload) noexcept {
    if (Entry* entry = find(entries_, payload)) {
        entry->visible = !entry->visible;
        return true;
    }
    return false;  // 未登记：不算状态变化
}

bool LegendToggle::click_at(const std::vector<chart::HitRegion>& regions, double x,
                            double y) noexcept {
    const chart::HitRegion* hit = topmost_hit(regions, x, y);
    if (hit == nullptr || hit->payload() == 0) {
        return false;
    }
    return click(hit->payload());
}

std::vector<std::uint32_t> LegendToggle::hidden() const {
    std::vector<std::uint32_t> out;
    for (const auto& entry : entries_) {
        if (!entry.visible) {
            out.push_back(entry.id);
        }
    }
    return out;
}

std::string LegendToggle::label(std::uint32_t id) const {
    const Entry* entry = find(entries_, id);
    return entry == nullptr ? std::string{} : entry->label;
}

}  // namespace termchart::interaction
