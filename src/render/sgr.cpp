// sgr.cpp —— 唯一的 SGR 生成实现
//
// 修改本文件前请重读 render/sgr.hpp 顶部的说明：任何第二处 SGR 实现都是架构事故。
#include "termchart/render/sgr.hpp"

#include "termchart/color/quantize.hpp"
#include "termchart/detail/ansi_tables.hpp"
#include "termchart/util/format.hpp"

namespace termchart::render::sgr {
namespace {

// ANSI 属性开关（开在前，关在后；同一位只发一个方向）。
constexpr int kAttrOn[5] = {1, 2, 3, 4, 7};    // Bold, Dim, Italic, Underline, Reverse
constexpr int kAttrOff[5] = {22, 22, 23, 24, 27};

void append_number(std::string& out, int value) {
    out.append(util::format_int(static_cast<std::int64_t>(value)));
}

void append_param(std::string& out, int value) {
    append_number(out, value);
    out.push_back(';');
}

void begin_sequence(std::string& out) { out.append(detail::kCsiPrefix); }

void end_sequence(std::string& out) {
    if (!out.empty() && out.back() == ';') {
        out.pop_back();  // 去掉多余分隔符
    }
    out.push_back('m');
}

}  // namespace

void append_color_params(std::string& out, const Color& color, bool foreground,
                         ColorDepth depth) noexcept {
    const Color effective = color::degrade(color, depth);
    if (effective.is_none()) {
        append_param(out, foreground ? 39 : 49);
        return;
    }
    switch (effective.kind()) {
        case ColorKind::None:
            append_param(out, foreground ? 39 : 49);
            return;
        case ColorKind::Ansi16: {
            const int index = effective.index();
            if (index < 8) {
                append_param(out, (foreground ? 30 : 40) + index);
            } else {
                append_param(out, (foreground ? 90 : 100) + (index - 8));
            }
            return;
        }
        case ColorKind::Indexed:
            append_param(out, foreground ? 38 : 48);
            append_param(out, 5);
            append_param(out, effective.index());
            return;
        case ColorKind::Rgb:
            append_param(out, foreground ? 38 : 48);
            append_param(out, 2);
            append_param(out, effective.rgb().r);
            append_param(out, effective.rgb().g);
            append_param(out, effective.rgb().b);
            return;
    }
}

void append_attr_on(std::string& out, AttrMask on_bits) noexcept {
    for (int i = 0; i < 5; ++i) {
        if (has_flag(on_bits, static_cast<Attr>(1u << i))) {
            append_param(out, kAttrOn[i]);
        }
    }
}

void append_attr_off(std::string& out, AttrMask off_bits) noexcept {
    for (int i = 0; i < 5; ++i) {
        if (has_flag(off_bits, static_cast<Attr>(1u << i))) {
            append_param(out, kAttrOff[i]);
        }
    }
}

void append_style(std::string& out, const Style& style, ColorDepth depth) noexcept {
    const std::size_t before = out.size();
    begin_sequence(out);
    if (depth != ColorDepth::None) {
        append_color_params(out, style.fg(), true, depth);
        append_color_params(out, style.bg(), false, depth);
    }
    append_attr_on(out, style.attrs());
    // 一个参数都没有（无色、无属性）时补 0，避免输出裸 "ESC[m"。
    if (out.size() == before + 2) {
        out.push_back('0');
    }
    end_sequence(out);
}

std::string style_sequence(const Style& style, ColorDepth depth) noexcept {
    std::string out;
    append_style(out, style, depth);
    return out;
}

void append_style_delta(std::string& out, const Style& from, const Style& to,
                        ColorDepth depth) noexcept {
    if (from == to) {
        return;
    }
    const std::size_t start = out.size();
    const Color from_fg = color::degrade(from.fg(), depth);
    const Color to_fg = color::degrade(to.fg(), depth);
    const Color from_bg = color::degrade(from.bg(), depth);
    const Color to_bg = color::degrade(to.bg(), depth);
    const bool fg_changed = !(from_fg == to_fg);
    const bool bg_changed = !(from_bg == to_bg);
    const AttrMask removed = static_cast<AttrMask>(from.attrs() & ~to.attrs());
    const AttrMask added = static_cast<AttrMask>(to.attrs() & ~from.attrs());
    const bool attr_changed = removed != 0 || added != 0;

    if (!fg_changed && !bg_changed && !attr_changed) {
        return;  // 降级后完全一致：一个字节都不发
    }

    begin_sequence(out);
    bool any = false;
    if (attr_changed && removed != 0) {
        append_attr_off(out, removed);
        any = true;
    }
    if (depth != ColorDepth::None) {
        if (fg_changed) {
            append_color_params(out, to.fg(), true, depth);
            any = true;
        }
        if (bg_changed) {
            append_color_params(out, to.bg(), false, depth);
            any = true;
        }
    }
    if (attr_changed && added != 0) {
        append_attr_on(out, added);
        any = true;
    }
    if (!any) {
        // 理论上到不了这里（上面已判等），兜底输出完整复位。
        out.resize(start);
        out.append(std::string(detail::kCsiPrefix) + "0m");
        return;
    }
    end_sequence(out);
}

std::string reset() noexcept { return std::string(detail::kCsiPrefix) + "0m"; }

}  // namespace termchart::render::sgr
