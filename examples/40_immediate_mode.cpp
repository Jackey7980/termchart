// 40_immediate_mode.cpp —— 即时模式组件：无状态绘制 + 返回"意图"驱动调用方状态
//
// 消费 widget/immediate_widget.hpp：button_clicked / checkbox_toggled。
// 组件不持状态：输入旗标每帧由调用方从合成指针位置推导（命中测试用同一个
// rect）；返回值是意图而非沿——沿检测（"这次算新点击吗"）由调用方比对上一帧
// 收到的返回值完成，checked 翻转发生在调用方手里。
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/widget/immediate_widget.hpp"
#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/widget.hpp"

namespace {

using namespace termchart;

struct Pointer {
    Point cell;
    bool down;
};

}  // namespace

int main() {
    const widget::ThemePalette theme = widget::kDarkTheme;
    const Rect btn_rect{2.0, 2.0, 14.0, 3.0};
    const Rect cb_rect{2.0, 7.0, 18.0, 1.0};

    // 调用方持有的全部状态（组件侧零状态）。
    bool checked = false;
    bool btn_intent_prev = false;
    bool cb_intent_prev = false;
    int clicks = 0;

    struct Step {
        const char* caption;
        Pointer pointer;
    };
    const std::vector<Step> steps{
        {"idle: pointer off-controls", Pointer{Point{30.0, 10.0}, false}},
        {"hover button", Pointer{Point{8.0, 3.0}, false}},
        {"press button (click intent reported)", Pointer{Point{8.0, 3.0}, true}},
        {"press checkbox (toggle intent reported)", Pointer{Point{4.0, 7.0}, true}},
        {"release: state kept by the caller", Pointer{Point{30.0, 10.0}, false}},
    };

    render::Renderer renderer;
    bool ok = true;
    for (const Step& step : steps) {
        canvas::Canvas c(40, 12);

        // 每帧从指针推导输入旗标（事件与几何正交：命中测试与绘制用同一 rect）。
        widget::ControlState btn_state;
        btn_state.hovered = btn_rect.contains(step.pointer.cell);
        btn_state.pressed = step.pointer.down && btn_state.hovered;
        const bool btn_intent = widget::button_clicked(c, btn_rect, "OK", theme, btn_state);
        if (btn_intent && !btn_intent_prev) {
            ++clicks;  // 沿检测在调用方：上一帧已报告不算新点击
        }
        btn_intent_prev = btn_intent;

        widget::ControlState cb_state;
        cb_state.hovered = cb_rect.contains(step.pointer.cell);
        cb_state.pressed = step.pointer.down && cb_state.hovered;
        const bool cb_intent =
            widget::checkbox_toggled(c, cb_rect, "auto refresh", checked, theme, cb_state);
        if (cb_intent && !cb_intent_prev) {
            checked = !checked;  // 翻转发生在调用方手里，下一帧传入新值
        }
        cb_intent_prev = cb_intent;

        char status[64] = {};
        std::snprintf(status, sizeof(status), "clicks=%d  checked=%s", clicks,
                      checked ? "yes" : "no");
        c.draw_text(Point{2.0, 10.0}, status, widget::style_fg(theme.text_dim));

        if (api::write_stdout(std::string("\n== ") + step.caption + " ==\n") != ErrorCode::Ok) {
            ok = false;
        }
        if (api::write_stdout(renderer.render_full(c.to_buffer())) != ErrorCode::Ok) {
            ok = false;
        }
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
