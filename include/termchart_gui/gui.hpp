// gui.hpp —— termchart_gui 平行栈伞形头
//
// 平行栈（widget_tree / native_loop / layout / backend_tk）与终端栈共享到 scene(L4)
// 为止。渲染核心已先期落地在 termchart::gui（font + SceneRenderer，Scene→Bitmap），
// 本栈直接复用，不再重复造 render 核心（见 docs/design/gui_stack.md）。
#ifndef TERMCHART_GUI_GUI_HPP
#define TERMCHART_GUI_GUI_HPP

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/layout/constraint.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/layout/grid.hpp"
#include "termchart_gui/layout/layout.hpp"
#include "termchart_gui/native_loop/dispatcher.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/native_loop/invalidate.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui/native_loop/timer.hpp"
#include "termchart_gui/render/style_map.hpp"
#include "termchart_gui/version.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/node.hpp"
#include "termchart_gui/widget_tree/reconcile.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

// 复用已落地的 GUI 渲染核心（Scene → Bitmap），避免平行栈重复实现 render。
#include "termchart/gui/scene_renderer.hpp"

namespace termchart_gui {

using SceneRenderer = termchart::gui::SceneRenderer;  // 便捷别名

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_GUI_HPP
