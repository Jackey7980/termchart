// fwd.hpp —— termchart_gui 前向声明与共享类型别名
//
// 平行栈复用 termchart 的语义层（scene L4 及以下）：几何用 core::Rect/Point/Rgb，
// 颜色经 color::resolve 量化，位图用 image::Bitmap。这里把常用类型别名到
// termchart_gui 命名空间，保持栈内代码整洁。
#ifndef TERMCHART_GUI_FWD_HPP
#define TERMCHART_GUI_FWD_HPP

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/media/image/bitmap.hpp"

namespace termchart_gui {

using Rect = termchart::Rect;
using Point = termchart::Point;
using Rgb = termchart::Rgb;
using Style = termchart::Style;
using Bitmap = termchart::image::Bitmap;

class Node;
class Widget;
class Container;
class Layout;
class EventLoop;
class Backend;
class TkWindow;
class TkCanvas;
struct Event;
struct NativeSurface;  // Take 2：外部宿主表面描述（backend_tk/native_surface.hpp）

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_FWD_HPP
