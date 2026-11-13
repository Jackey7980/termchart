// cocoa_backend.hpp —— 原生 Cocoa 后端（Take 2：用户拥有窗口）
//
// 定位：macOS 的 NSView 句柄注入后端。用户自己持有 NSWindow/NSView，把 NSView*
// 经 NativeSurface.surface 交给本后端；后端不建窗，只：
//   ① present() 置 setNeedsDisplay:YES，驱动宿主重绘；
//   ② draw_in_current_context() 在宿主 drawRect: 里把 BitmapCanvas 画进当前 CGContext
//      （RGBA 位图 → CGImage，按 NSView 左下原点翻转）。
// 事件由宿主的 mouseDown:/mouseMoved:/keyDown: 剥壳后经 inject_event 回传。
//
// 平台隔离：<AppKit/AppKit.h> 只出现在 src/cocoa_backend.mm；公开头零 Objective-C 类型。
#ifndef TERMCHART_GUI_COCOA_BACKEND_HPP
#define TERMCHART_GUI_COCOA_BACKEND_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"

namespace termchart_gui_cocoa {

class CocoaBackend final : public termchart_gui::Backend {
public:
    struct Impl;

    // surface.surface = NSView*（__bridge 语义），width/height = 视图逻辑尺寸。
    explicit CocoaBackend(const termchart_gui::NativeSurface& host);
    ~CocoaBackend() override;
    CocoaBackend(const CocoaBackend&) = delete;
    CocoaBackend& operator=(const CocoaBackend&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool is_external() const noexcept;

    void resize(std::uint32_t width, std::uint32_t height);

    // 宿主 NSView 的 drawRect: 中调用：把画布画进 [NSGraphicsContext currentContext]。
    void draw_in_current_context();

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

}  // namespace termchart_gui_cocoa

#endif  // TERMCHART_GUI_COCOA_BACKEND_HPP
