// backend_tk/tk_window.hpp —— 原生窗口抽象
//
// GUI 平行栈对"窗口"的抽象：真实后端（Qt/Tk/Win32…）提供具体实现；无头后端返回
// 空窗口（绘制走内部 BitmapCanvas，不依赖真实窗口）。这里只定义接口。
#ifndef TERMCHART_GUI_BACKEND_TK_TK_WINDOW_HPP
#define TERMCHART_GUI_BACKEND_TK_TK_WINDOW_HPP

#include <cstdint>
#include <memory>
#include <string>

namespace termchart_gui {

class TkWindow {
public:
    virtual ~TkWindow() = default;

    virtual void set_title(std::string title) = 0;
    virtual void resize(std::uint32_t w, std::uint32_t h) = 0;
    [[nodiscard]] virtual std::uint32_t width() const = 0;
    [[nodiscard]] virtual std::uint32_t height() const = 0;
    virtual void close() = 0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_TK_WINDOW_HPP
