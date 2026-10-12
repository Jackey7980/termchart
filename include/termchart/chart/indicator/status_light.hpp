// status_light.hpp —— 状态灯
//
// 一个 ● 按状态着色（Ok 绿 / Warning 黄 / Error 红 / Unknown 灰），
// 可带标签。是全部 indicator 里最轻的读数件：一格即语义。
#ifndef TERMCHART_CHART_INDICATOR_STATUS_LIGHT_HPP
#define TERMCHART_CHART_INDICATOR_STATUS_LIGHT_HPP

#include <cstdint>
#include <string>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

enum class LightState : std::uint8_t { Ok, Warning, Error, Unknown };

struct StatusLightOptions {
    bool show_label = true;
};

class StatusLight final : public Chart {
public:
    void set_state(LightState state) { state_ = state; }
    void set_label(std::string text) { label_ = std::move(text); }
    void set_options(const StatusLightOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    LightState state_ = LightState::Unknown;
    std::string label_;
    StatusLightOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_INDICATOR_STATUS_LIGHT_HPP
