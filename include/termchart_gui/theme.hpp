// theme.hpp —— termchart_gui 像素层统一主题（调色板 + 预设 + 全局当前主题）
//
// 解决 GUI 平行栈"颜色硬编码、无一键 Dark/Light、默认不好看"的痛点。所有渲染器
// （HsiKLineRenderer / ComboRenderer）与控件（Button / Slider / ChartWidget 十字轴）
// 都从 current_theme() 取色，做到：① 默认主题 DefaultDark() 像素等价改动前的硬色
// （既有 gui 像素测试零破坏）；② 提供 GitHubDark()/AppleLight() 杀手预设；③ 运行期
// set_theme()/cycle_theme() 一键换肤。theme 是 header-only，无 .cpp。
#ifndef TERMCHART_GUI_THEME_HPP
#define TERMCHART_GUI_THEME_HPP

#include <cmath>

#include "termchart_gui/fwd.hpp"  // 引入 termchart::Rgb → termchart_gui::Rgb 别名

namespace termchart_gui {

// 一套完整的像素层调色板。初值即 DefaultDark()（与改动前的硬色一致）。
struct GuiTheme {
    Rgb background{24, 26, 32};       // 图表/应用主背景
    Rgb surface{30, 34, 44};          // 卡片/信息框底
    Rgb surface_alt{36, 40, 52};      // 次级面
    Rgb border{84, 96, 116};         // 描边
    Rgb grid{48, 51, 60};             // 网格线
    Rgb axis{96, 100, 112};           // 坐标轴
    Rgb foreground{229, 229, 229};    // 主文字
    Rgb text_dim{150, 152, 162};      // 次要文字
    Rgb accent{80, 226, 247};         // 品牌强调
    Rgb up{22, 163, 74};              // 涨/多头
    Rgb down{220, 50, 50};            // 跌/空头
    Rgb crosshair{90, 100, 120};      // 悬停十字轴
    Rgb tooltip_bg{30, 34, 44};       // 信息框底
    Rgb tooltip_border{84, 96, 116};  // 信息框描边
    Rgb tooltip_fg{236, 240, 246};    // 信息框文字
    Rgb button_bg{50, 50, 58};        // 按钮底
    Rgb button_pressed{74, 74, 84};   // 按钮按下
    Rgb button_border{124, 124, 136}; // 按钮描边
    Rgb button_label{236, 236, 240};  // 按钮文字
    Rgb slider_track{58, 58, 64};     // 滑块轨道
    Rgb slider_thumb{120, 184, 236};  // 滑块拇指
    bool is_dark = true;              // 是否深色系（用于自动选反色文字等）
};

// 默认深色：精确复刻改动前的硬色，保证既有 GUI 像素测试零破坏。
inline GuiTheme DefaultDark() { return GuiTheme{}; }

// GitHub 暗色：精炼的杀手级深色（英雄 Demo 默认用）。
inline GuiTheme GitHubDark() {
    GuiTheme t;
    t.background = {13, 17, 23};
    t.surface = {22, 27, 34};
    t.surface_alt = {33, 38, 46};
    t.border = {48, 54, 61};
    t.grid = {33, 38, 45};
    t.axis = {55, 60, 68};
    t.foreground = {230, 237, 243};
    t.text_dim = {139, 148, 158};
    t.accent = {88, 166, 255};
    t.up = {63, 185, 80};
    t.down = {248, 81, 73};
    t.crosshair = {139, 148, 158};
    t.tooltip_bg = {22, 27, 34};
    t.tooltip_border = {48, 54, 61};
    t.tooltip_fg = {230, 237, 243};
    t.button_bg = {33, 38, 46};
    t.button_pressed = {48, 54, 61};
    t.button_border = {86, 95, 107};
    t.button_label = {230, 237, 243};
    t.slider_track = {48, 54, 61};
    t.slider_thumb = {88, 166, 255};
    t.is_dark = true;
    return t;
}

// Apple 风浅色：毛玻璃感的干净浅色。
inline GuiTheme AppleLight() {
    GuiTheme t;
    t.background = {255, 255, 255};
    t.surface = {247, 248, 250};
    t.surface_alt = {241, 243, 245};
    t.border = {208, 213, 221};
    t.grid = {228, 231, 236};
    t.axis = {199, 205, 214};
    t.foreground = {17, 24, 39};
    t.text_dim = {107, 114, 128};
    t.accent = {0, 122, 255};
    t.up = {22, 163, 74};
    t.down = {220, 50, 50};
    t.crosshair = {180, 190, 200};
    t.tooltip_bg = {255, 255, 255};
    t.tooltip_border = {208, 213, 221};
    t.tooltip_fg = {17, 24, 39};
    t.button_bg = {240, 242, 245};
    t.button_pressed = {224, 227, 231};
    t.button_border = {208, 213, 221};
    t.button_label = {17, 24, 39};
    t.slider_track = {216, 220, 226};
    t.slider_thumb = {0, 122, 255};
    t.is_dark = false;
    return t;
}

inline GuiTheme LightTheme() { return AppleLight(); }

// —— 全局当前主题（function-local static，线程内一致）——
inline GuiTheme& theme_storage() {
    static GuiTheme current = DefaultDark();
    return current;
}

inline const GuiTheme& current_theme() { return theme_storage(); }
inline void set_theme(GuiTheme t) { theme_storage() = std::move(t); }
inline bool is_dark_theme() { return theme_storage().is_dark; }

// 循环切换：DefaultDark → GitHubDark → AppleLight → …（Demo 按 'T' 调用）。
inline void cycle_theme() {
    static int idx = 0;
    switch ((++idx) % 3) {
        case 1:
            set_theme(GitHubDark());
            break;
        case 2:
            set_theme(AppleLight());
            break;
        default:
            set_theme(DefaultDark());
            break;
    }
}

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_THEME_HPP
