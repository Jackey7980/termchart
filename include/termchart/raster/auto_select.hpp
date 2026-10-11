// auto_select.hpp —— 按终端能力自动挑光栅器
//
// 注意：本文件**不**依赖 term/（term 是更高层），能力以扁平布尔传入，与
// TerminalCaps 解耦——这样光栅器层就能在不知晓终端对象的情况下完成选择。
#ifndef TERMCHART_RASTER_AUTO_SELECT_HPP
#define TERMCHART_RASTER_AUTO_SELECT_HPP

#include "termchart/export.hpp"
#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

// 终端对各子像素光栅器的支持度（由调用方从 TerminalCaps 折算而来）。
struct RasterCaps {
    bool braille = false;
    bool halfblock = false;
    bool quadrant = false;
    bool block = false;
    bool ascii = false;
    bool sixel = false;  // 预留：真彩色图像协议，光栅器层不管
};

// 首选若支持则用之；否则按保真度（盲文 > 象限 > 半块 > 竖条 > ascii）降级；
// 全不支持兜底 ascii（任何终端都认）。
TERMCHART_API RasterKind pick_raster(const RasterCaps& caps,
                                     RasterKind preferred = RasterKind::Braille) noexcept;

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_AUTO_SELECT_HPP
