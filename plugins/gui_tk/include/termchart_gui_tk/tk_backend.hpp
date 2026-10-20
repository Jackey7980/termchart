// tk_backend.hpp —— 原生 Tk 后端（可选插件，默认不编进核心）
//
// 定位：termchart_gui 平行栈的**真实窗口后端**之一（与 gui_win32 / gui_qt 并列）。
// 实现策略同构：控件/布局/绘制零改动（只面向 TkCanvas 抽象），本后端持一块离屏
// image::Bitmap 当画布（复用 BitmapCanvas），经 Tk **photo image**
// （Tk_FindPhoto/Tk_PhotoPutBlock）把 RGBA 整块贴进 canvas 控件上屏。
//
// 可移植性取舍（为什么事件走 Tcl 层 bind 而非 Tk_CreateEventHandler）：
//   XEvent 的字段布局在 X11 与 Win32 后端定义不同（xbutton/xclient 结构体不一），
//   直解析 XEvent 只能覆盖一种平台。改为在 Tcl 层 `bind` 事件到注册的 C 命令
//   （termchart_tk_event），用 Tk 的模板变量（%x %y %b %s %k）拿语义载荷——
//   一份绑定脚本三平台通用。
//
// 为什么它是插件而非核心：Tk 需外部依赖（libtk/libtcl）+ 编译型平台代码，
// 违背核心"零强制依赖、header-only"硬约束。故独立目标、CMake 选项默认 OFF；
// 显式开启但找不到 Tcl/Tk 开发库时**配置期报错**（fail-loud，不静默跳过）。
//
// 平台隔离：tk.h/cl_Tk.h 只出现在 src/tk_backend.cpp；公开头不泄漏任何 Tk
// 类型（Tcl_Interp/Tk_Window 全部藏进 PIMPL）。
#ifndef TERMCHART_GUI_TK_BACKEND_HPP
#define TERMCHART_GUI_TK_BACKEND_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"

namespace termchart_gui_tk {

class TkBackend final : public termchart_gui::Backend {
public:
    // Impl 定义在 .cpp（PIMPL，持 Tcl_Interp/Tk_Window/photo handle）。
    struct Impl;

    // width/height 为初始客户区尺寸（像素）；title 为窗口标题。
    TkBackend(std::uint32_t width, std::uint32_t height, std::string title);
    ~TkBackend() override;
    TkBackend(const TkBackend&) = delete;
    TkBackend& operator=(const TkBackend&) = delete;

    // 解释器/主窗/canvas/photo image 是否就绪（Tk_Init 失败时为 false）。
    [[nodiscard]] bool valid() const noexcept;

    // Backend 接口。
    [[nodiscard]] std::unique_ptr<termchart_gui::TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) override;
    [[nodiscard]] termchart_gui::TkCanvas& canvas() override;
    [[nodiscard]] bool poll_event(termchart_gui::Event& out) override;
    void present() override;
    void run_loop(termchart_gui::EventLoop& loop) override;

    // 请求关闭窗口（WM_DELETE_WINDOW 协议 → Quit 事件 + 退出消息循环）。
    void request_close();

    // "尺寸变化时同步重渲染"钩子（与 Win32/Qt 同款语义）：<Configure> 绑定的
    // Tcl 命令里就地回调（传新客户区宽高），拖动中实时更新。
    void set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook);

    // 供应用层主动泵一次 Tcl 事件（不依赖 run_loop 的自持循环模式也用得上）。
    void process_events();

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart_gui_tk

#endif  // TERMCHART_GUI_TK_BACKEND_HPP
