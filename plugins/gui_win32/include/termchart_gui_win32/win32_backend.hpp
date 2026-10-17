// win32_backend.hpp —— 原生 Win32 后端（可选插件，默认不编进核心）
//
// 定位：termchart_gui 平行栈的**真实窗口后端**之一。与无头/终端两种参考后端并列，
// 差别只在"事件来源"与"上屏目标"：这里用 OS 消息泵产事件、用 GDI StretchDIBits 上屏。
// 控件树/布局/绘制**零改动**——它们只面向 TkCanvas 抽象；本后端持一块离屏
// image::Bitmap 当画布，WM_PAINT 时整块贴进窗口 DC（BGRA 重排）。
//
// 为什么它是插件而非核心：Win32 需要编译型平台代码 + 链 user32/gdi32，违背核心
// "零强制依赖、header-only"硬约束。故独立目标、CMake 选项默认 OFF（见顶层）。
//
// 平台隔离：<windows.h> 只出现在 src/win32_backend.cpp；本公开头不泄漏任何 Win32
// 类型（HWND 等全部藏进 PIMPL）。
#ifndef TERMCHART_GUI_WIN32_BACKEND_HPP
#define TERMCHART_GUI_WIN32_BACKEND_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"

namespace termchart_gui_win32 {

class Win32Backend final : public termchart_gui::Backend {
public:
    // Impl 定义在 .cpp（PIMPL）。设为 public 仅为让同文件内的窗口过程能具名访问，
    // 不构成对外契约。
    struct Impl;

    Win32Backend(std::uint32_t width, std::uint32_t height, std::string title);
    ~Win32Backend() override;
    Win32Backend(const Win32Backend&) = delete;
    Win32Backend& operator=(const Win32Backend&) = delete;

    // 窗口是否成功创建（无交互桌面/会话时 CreateWindow 可能失败）。
    [[nodiscard]] bool valid() const noexcept;

    // Backend 接口。
    [[nodiscard]] std::unique_ptr<termchart_gui::TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) override;
    [[nodiscard]] termchart_gui::TkCanvas& canvas() override;
    [[nodiscard]] bool poll_event(termchart_gui::Event& out) override;
    void present() override;
    void run_loop(termchart_gui::EventLoop& loop) override;

    // 请求关闭窗口（触发 WM_DESTROY → 消息泵退出）。
    void request_close();

    // 注册"尺寸变化时同步重渲染"钩子：拖动边框时系统进入模态 resize 循环、应用主循环被
    // 阻塞，仅入队事件会"拖动卡住、松手才动"。设了它，WndProc 每收到 WM_SIZE 就地回调
    // （传新客户区宽高），应用可立刻 root.set_bounds + 吸附 + 重绘 → 拖动中实时更新。
    void set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook);

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart_gui_win32

#endif  // TERMCHART_GUI_WIN32_BACKEND_HPP
