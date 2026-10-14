// backend_tk/backend.hpp —— 后端抽象 + 无头参考实现
//
// Backend 是"窗口 + 画布 + 事件源 + 上屏"的总接口；真实 tk（Qt/Tk/Win32…）提供
// 具体实现。这里同时给出一个 headless 参考实现（BitmapCanvas + HeadlessBackend），
// 用 image::Bitmap 当画布、内部队列当事件源，使整条平行栈能在无窗口系统下编译与
// 测试；将来接真实 tk 时按需替换，widget_tree / layout / native_loop 不动。
#ifndef TERMCHART_GUI_BACKEND_TK_BACKEND_HPP
#define TERMCHART_GUI_BACKEND_TK_BACKEND_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "termchart/gui/font.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/native_loop/loop.hpp"

namespace termchart_gui {

class Backend {
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual std::unique_ptr<TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) = 0;
    [[nodiscard]] virtual TkCanvas& canvas() = 0;
    // 取一个原生事件（映射到 Event）；无事件返回 false。
    [[nodiscard]] virtual bool poll_event(Event& out) = 0;
    virtual void present() = 0;  // 上屏
    virtual void run_loop(EventLoop& loop) = 0;

    // 无头用：注入事件（真实后端由 OS 消息泵产生事件）。
    virtual void inject_event(Event e) { (void)e; }
};

// ── 无头参考实现 ──────────────────────────────────────────────────────────────

class BitmapCanvas final : public TkCanvas {
public:
    explicit BitmapCanvas(std::uint32_t w, std::uint32_t h) : bmp_(w, h, 4) {}

    void clear(Rgb bg) override { bmp_.fill(bg.r, bg.g, bg.b, 255); }

    void fill_rect(const Rect& r, Rgb color) override {
        const int ix = floor_to_int(r.x);
        const int iy = floor_to_int(r.y);
        const int iw = ceil_to_int(r.x + r.w) - ix;
        const int ih = ceil_to_int(r.y + r.h) - iy;
        if (iw <= 0 || ih <= 0) {
            return;
        }
        bmp_.fill_rect(static_cast<std::uint32_t>(ix), static_cast<std::uint32_t>(iy),
                       static_cast<std::uint32_t>(iw), static_cast<std::uint32_t>(ih),
                       color.r, color.g, color.b, 255);
    }

    void stroke_rect(const Rect& r, Rgb color, double width) override {
        const int wd = width < 1.0 ? 1 : static_cast<int>(width);
        const double dwd = static_cast<double>(wd);
        fill_rect(Rect{r.x, r.y, r.w, dwd}, color);
        fill_rect(Rect{r.x, r.y + r.h - dwd, r.w, dwd}, color);
        fill_rect(Rect{r.x, r.y, dwd, r.h}, color);
        fill_rect(Rect{r.x + r.w - dwd, r.y, dwd, r.h}, color);
    }

    void draw_text(double x, double y, std::string_view text, Rgb color) override {
        // 复用已落地的 GUI 位图字体（5×7 点阵，advance=cell_w=6，格高=8）：
        // 平行栈不重复造字体，直接调 termchart::gui 的文本出口。
        (void)termchart::gui::draw_text(bmp_, text, Point{x, y}, color, kCellW, kCellH);
    }

    // 文本格尺寸：与 termchart::gui 点阵字体的 advance/格高约定一致（5×7 字形
    // 画在前 5/6 宽、7/8 高，末列/末行留作字距），供控件排版居中/量宽。
    static constexpr double kCellW = 6.0;
    static constexpr double kCellH = 8.0;

    [[nodiscard]] Bitmap snapshot() const override { return bmp_; }

private:
    static int floor_to_int(double v) { return static_cast<int>(v); }  // 坐标已为像素
    static int ceil_to_int(double v) {
        const int i = static_cast<int>(v);
        return (v > static_cast<double>(i)) ? i + 1 : i;
    }
    Bitmap bmp_;
};

class HeadlessBackend final : public Backend {
public:
    explicit HeadlessBackend(std::uint32_t w, std::uint32_t h) : canvas_(w, h) {}

    [[nodiscard]] std::unique_ptr<TkWindow> create_window(std::uint32_t, std::uint32_t,
                                                         std::string) override {
        return nullptr;  // 无头不需要真实窗口
    }
    [[nodiscard]] TkCanvas& canvas() override { return canvas_; }
    [[nodiscard]] bool poll_event(Event& out) override {
        if (queue_.empty()) {
            return false;
        }
        out = queue_.front();
        queue_.pop_front();
        return true;
    }
    void present() override {}  // 无头无需上屏
    void run_loop(EventLoop& loop) override { loop.run(); }
    void inject_event(Event e) override { queue_.push_back(std::move(e)); }

private:
    BitmapCanvas canvas_;
    std::deque<Event> queue_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_BACKEND_HPP
