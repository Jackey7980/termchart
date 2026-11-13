// x11_backend.hpp —— 原生 X11 后端（Take 2：用户拥有窗口）
//
// 定位：与 gui_win32 并列的 Linux/X11 句柄注入后端。用户自己 XCreateWindow 并持有
// 窗口，把 Window(XID)+Display* 经 NativeSurface 交给本后端；后端只做两件事：
//   ① present() 用 XPutImage 把 BitmapCanvas 贴进该窗口客户区；
//   ② poll_event() 从该 Display 取 X 事件，经 tk_event_map 翻成中性 Event。
// 不创建/不销毁用户窗口，不跑 Xlib 私有主循环（消息泵在宿主手里）。
//
// 平台隔离：<X11/Xlib.h> 只出现在 src/x11_backend.cpp；公开头零 X11 类型（PIMPL）。
#ifndef TERMCHART_GUI_X11_BACKEND_HPP
#define TERMCHART_GUI_X11_BACKEND_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"

namespace termchart_gui_x11 {

class X11Backend final : public termchart_gui::Backend {
public:
    struct Impl;

    // 绑定用户已创建并拥有的 X11 窗口：surface.surface = Window(XID)，
    // surface.display = Display*，surface.width/height = 客户区初始尺寸。
    explicit X11Backend(const termchart_gui::NativeSurface& host);
    ~X11Backend() override;
    X11Backend(const X11Backend&) = delete;
    X11Backend& operator=(const X11Backend&) = delete;

    [[nodiscard]] bool valid() const noexcept;  // Display* 与 Window 均有效
    [[nodiscard]] bool is_external() const noexcept;

    // 宿主尺寸变化时调用：重排画布缓冲并投递 Resize 事件。
    void resize(std::uint32_t width, std::uint32_t height);

    // Backend 接口。
    [[nodiscard]] std::unique_ptr<termchart_gui::TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) override;
    [[nodiscard]] termchart_gui::TkCanvas& canvas() override;
    [[nodiscard]] bool poll_event(termchart_gui::Event& out) override;
    void present() override;
    void run_loop(termchart_gui::EventLoop& loop) override;
    void inject_event(termchart_gui::Event e) override;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart_gui_x11

#endif  // TERMCHART_GUI_X11_BACKEND_HPP
